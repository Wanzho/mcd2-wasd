// wasdmod for Mac: installs the Minecraft Dungeons II controller mod into the
// CrossOver bottle that has the game, and edits its key layout.
//
// The install work is done by wasdmod.sh (in the app's Resources). The key layout
// editor is Key Layout Editor.html in a web view; window.wasdmodHost (below) keeps
// its layouts in ~/Library/Application Support/wasdmod and saves straight into
// the game folder. The page also shows the mod's state and has the mod's own buttons,
// the game folder and Troubleshooting buttons; the toolbar has the language picker.
import Cocoa
import UniformTypeIdentifiers
import WebKit

// ------------------------------------------------------------ languages
// Every language the game has (lang.json in Resources, made by lang.py from
// lang/*.json). English is the key: L("Install") is that text in the current
// language, or the English if it has none. The toolbar's picker (or the editor) picks the language.
struct Languages {
    var order = ["en"], names = ["en": "English"], text: [String: [String: String]] = [:]
    init() {
        guard let url = Bundle.main.url(forResource: "lang", withExtension: "json"), let data = try? Data(contentsOf: url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        order = json["order"] as? [String] ?? order
        names = json["names"] as? [String: String] ?? names
        for (code, strings) in json["text"] as? [String: [String: Any]] ?? [:] { text[code] = strings.compactMapValues { $0 as? String } }
    }
    // "de-DE", "pt-BR", "zh-Hant-TW", "zh-HK"... -> one of ours.
    func match(_ tag: String) -> String? {
        let t = tag.lowercased().replacingOccurrences(of: "_", with: "-")
        if t == "zh" || t.hasPrefix("zh-") {
            let hant = ["zh-hant", "zh-tw", "zh-hk", "zh-mo"].contains { t.hasPrefix($0) }
            return order.contains(hant ? "zh-Hant" : "zh-Hans") ? (hant ? "zh-Hant" : "zh-Hans") : nil
        }
        let base = t.split(separator: "-").first.map(String.init) ?? t
        return order.first { $0.lowercased() == base }
    }
}
// The app's settings. Test builds can keep theirs apart (WASDMOD_TEST_DEFAULTS, a
// defaults domain), so a test never touches the player's settings.
#if SELFTEST
let prefs = ProcessInfo.processInfo.environment["WASDMOD_TEST_DEFAULTS"].flatMap { UserDefaults(suiteName: $0) } ?? .standard
// WASDMOD_TEST_UPDATE=check|update|after: the update flow without a window or Dock
// icon (updater.swift), logged to WASDMOD_TEST_LOG. WASDMOD_TEST_WATCH=<a .sav copy>: the
// watch on the game's keyboard settings (ControlsWatch), tried on that file, no window.
let testHeadless = ProcessInfo.processInfo.environment["WASDMOD_TEST_UPDATE"] != nil || ProcessInfo.processInfo.environment["WASDMOD_TEST_WATCH"] != nil
func testLog(_ s: String) {
    print(s); fflush(stdout)
    guard let path = ProcessInfo.processInfo.environment["WASDMOD_TEST_LOG"] else { return }
    if !FileManager.default.fileExists(atPath: path) { FileManager.default.createFile(atPath: path, contents: nil) }
    if let h = FileHandle(forWritingAtPath: path) { h.seekToEndOfFile(); h.write(Data("[\(getpid())] \(s)\n".utf8)); h.closeFile() }
}
#else
let prefs = UserDefaults.standard
let testHeadless = false
@inline(__always) func testLog(_ s: @autoclosure () -> String) {}
#endif
let languages = Languages()
var lang = languages.match(prefs.string(forKey: "lang") ?? "")
    ?? Locale.preferredLanguages.lazy.compactMap { languages.match($0) }.first ?? "en"
func L(_ s: String, _ vars: [String: String] = [:]) -> String {
    var out = languages.text[lang]?[s].flatMap { $0.isEmpty ? nil : $0 } ?? s
    for (k, v) in vars { out = out.replacingOccurrences(of: "{\(k)}", with: v) }
    return out
}

// The bar under the toolbar: a tinted strip (redrawn for light and dark).
final class Banner: NSStackView {
    var tint: NSColor = .controlAccentColor { didSet { needsDisplay = true } }
    override var wantsUpdateLayer: Bool { true }
    override func updateLayer() {
        effectiveAppearance.performAsCurrentDrawingAppearance { layer?.backgroundColor = tint.withAlphaComponent(0.12).cgColor }
    }
}

// The game's keyboard settings file, watched for the editor. The editor asks for the file's
// stamp (its size and time) with the one it has; the answer waits until the stamp is a
// different one, or half a minute. While a question waits, the file and its folder are
// watched with kqueue (DispatchSource): nothing runs until the file system says something
// changed there, also while the app is in the background and the game in front.
final class ControlsWatch {
    var path: String? { didSet { if path != oldValue { answer() } } }
    var timeout: TimeInterval = 30
    #if SELFTEST
    var stats = 0
    #endif
    private var waiters: [Int: (have: String?, reply: (String) -> Void)] = [:]
    private var count = 0
    private var sources: [DispatchSourceFileSystemObject] = []
    private var settle: DispatchWorkItem?

    var stamp: String {
        #if SELFTEST
        stats += 1
        #endif
        guard let path = path else { return "" }
        var st = stat()
        guard stat(path, &st) == 0 else { return "" }
        return "\(st.st_size):\(st.st_mtimespec.tv_sec).\(st.st_mtimespec.tv_nsec)"
    }
    func wait(have: String?, reply: @escaping (String) -> Void) {
        let now = stamp
        if have != now { return reply(now) }
        count += 1
        let id = count
        waiters[id] = (have, reply)
        start()
        DispatchQueue.main.asyncAfter(deadline: .now() + timeout) { [weak self] in
            guard let self = self, let w = self.waiters.removeValue(forKey: id) else { return }
            w.reply(self.stamp)
            if self.waiters.isEmpty { self.stop() }
        }
    }
    private func start() {
        guard sources.isEmpty, let path = path else { return }
        let folder = (path as NSString).deletingLastPathComponent
        let watched: [(String, DispatchSource.FileSystemEvent)] = [(folder, .write), (path, [.write, .extend, .attrib, .delete, .rename])]
        for (p, mask) in watched {
            let fd = open(p, O_EVTONLY)
            if fd < 0 { continue }
            let source = DispatchSource.makeFileSystemObjectSource(fileDescriptor: fd, eventMask: mask, queue: .main)
            source.setEventHandler { [weak self] in self?.touched() }
            source.setCancelHandler { close(fd) }
            source.resume()
            sources.append(source)
        }
    }
    private func stop() { for s in sources { s.cancel() }; sources = [] }
    // A moment after the last event: the game writes the file in a few steps, or replaces it.
    private func touched() {
        settle?.cancel()
        let w = DispatchWorkItem { [weak self] in self?.answer() }
        settle = w
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25, execute: w)
    }
    private func answer() {
        stop() // the file may be a new one now: watched again below while someone waits
        let now = stamp
        for (id, w) in waiters where w.have != now { waiters[id] = nil; w.reply(now) }
        if !waiters.isEmpty { start() }
    }
}

extension NSToolbarItem.Identifier {
    static let language = Self("language")
}

final class App: NSObject, NSApplicationDelegate, NSToolbarDelegate, WKScriptMessageHandlerWithReply, WKNavigationDelegate, WKUIDelegate {
    var window: NSWindow!
    var web: WKWebView!
    // The window's toolbar: the language picker on the right (a globe and the language's
    // name), with the editor's languages in its order. The mod's buttons are in the page.
    var languageItem: NSToolbarItem?
    lazy var languagePicker: NSPopUpButton = {
        let p = NSPopUpButton(frame: .zero, pullsDown: false)
        p.target = self; p.action = #selector(pickLanguage)
        for code in languages.order { p.addItem(withTitle: languages.names[code] ?? code); p.lastItem?.representedObject = code }
        (p.cell as? NSPopUpButtonCell)?.usesItemFromMenu = false // (shows its own item: the globe, then the name)
        return p
    }()
    // A bar under the toolbar while something needs doing (no game, not installed, an update...).
    let banner = Banner(), bannerLine = NSBox()
    let bannerIcon = NSImageView()
    let bannerText = NSTextField(wrappingLabelWithString: "")
    let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"
    lazy var chooseButton = button(#selector(chooseGame))
    lazy var actionButton: NSButton = { let b = button(#selector(bannerAction)); b.keyEquivalent = "\r"; return b }()
    lazy var setupButton = button(#selector(openSetupHelp))
    let res = Bundle.main.resourceURL!
    #if SELFTEST
    let support = URL(fileURLWithPath: ProcessInfo.processInfo.environment["WASDMOD_SUPPORT"] ?? NSTemporaryDirectory() + "wasdmod-test-support")
    #else
    let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("wasdmod")
    #endif
    var storeURL: URL { support.appendingPathComponent("editor.json") }
    var info: [String: String] = [:]
    var busy = false
    #if !NEXUS
    lazy var updater = Updater(app: self) // the one-click update (updater.swift)
    #endif

    // The game's folder, picked by hand (wins over the automatic search while it exists).
    var gameDir: String? {
        get { prefs.string(forKey: "game") }
        set { prefs.set(newValue, forKey: "game"); controlsWatch.path = nil } // the game's settings file is looked for again
    }
    let controlsWatch = ControlsWatch()
    // Where the game's keyboard settings file is (nil while there's none).
    func findControls() -> String? {
        #if SELFTEST
        if let p = ProcessInfo.processInfo.environment["WASDMOD_TEST_CONTROLS"] { return p } // tests: a copy, never the game's own
        #endif
        let r = sh(["controls"])
        return r.ok ? fields(r.out)["controls"] : nil
    }
    // A bottle picked by hand in earlier versions.
    var bottle: String? {
        get { prefs.string(forKey: "bottle") }
        set { prefs.set(newValue, forKey: "bottle") }
    }

    func button(_ action: Selector) -> NSButton {
        let b = NSButton(title: "", target: self, action: action)
        b.bezelStyle = .rounded
        return b
    }

    // ------------------------------------------------------------ wasdmod.sh

    func sh(_ args: [String], _ extra: [String: String] = [:]) -> (ok: Bool, out: String) {
        #if SELFTEST
        // Test builds: with WASDMOD_TEST_GAMEROOT set, nothing that changes a game folder
        // runs on a game outside it (the player's own game stays untouched).
        if let root = ProcessInfo.processInfo.environment["WASDMOD_TEST_GAMEROOT"], let cmd = args.first,
           !["status", "locate", "folder", "report", "controls"].contains(cmd) || (cmd == "controls" && args.count > 1) {
            let game = fields(shRun(["status"], extra).out)["game"] ?? ""
            if game.isEmpty || !game.hasPrefix(root) { testLog("test guard: refused \(cmd) on \(game.isEmpty ? "(no game)" : game)"); return (false, "test guard: not a test game folder") }
        }
        #endif
        return shRun(args, extra)
    }
    func shRun(_ args: [String], _ extra: [String: String] = [:]) -> (ok: Bool, out: String) {
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/bin/sh")
        p.arguments = [res.appendingPathComponent("wasdmod.sh").path] + args
        var env = ProcessInfo.processInfo.environment
        if let g = gameDir { env["GAMEDIR"] = g } else if let b = bottle { env["BOTTLE"] = b }
        for (k, v) in extra { env[k] = v }
        p.environment = env
        let out = Pipe(), err = Pipe()
        p.standardOutput = out; p.standardError = err
        do { try p.run() } catch { return (false, error.localizedDescription) }
        let o = String(decoding: out.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
        let e = String(decoding: err.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
        p.waitUntilExit()
        let ok = p.terminationStatus == 0
        return (ok, (ok ? o : e).trimmingCharacters(in: .whitespacesAndNewlines))
    }

    func fields(_ out: String) -> [String: String] {
        var d: [String: String] = [:]
        for line in out.split(separator: "\n") {
            if let eq = line.firstIndex(of: "=") { d[String(line[..<eq])] = String(line[line.index(after: eq)...]) }
        }
        return d
    }

    func layoutName(_ f: String) -> String {
        switch f {
        case "default.txt": return L("Official layout (the game's own keys)")
        case "author.txt": return L("Recommended")
        case "": return L("none")
        default: return L("your own ({file})", ["file": f])
        }
    }

    func refresh() {
        info = fields(sh(["status"]).out)
        let state = info["state"] ?? ""
        let found = !(info["game"] ?? "").isEmpty
        let installed = state == "current" || state == "older" || state == "off"
        // Short in the editor's status line; the whole story (and what to click) in the bar.
        var line: String, text: String?, symbol = "info.circle.fill", tint = NSColor.controlAccentColor
        if !found {
            line = L("Game not found")
            text = L("Minecraft Dungeons II wasn't found in a CrossOver bottle. Set it up with MCD2 Crossover (Steam in CrossOver), or if it's somewhere else (a Minecraft Launcher copy…), click Choose Game Folder.")
            symbol = "exclamationmark.triangle.fill"; tint = .systemOrange
        } else if state == "current" {
            line = L("Installed")
        } else if state == "older" {
            // Never "up to date" while the game folder has an older mod than this app.
            line = L("Mod in the game is out of date")
            let v = info["modversion"] ?? "", running = info["running"] == "yes"
            text = v.isEmpty ? (running ? L("The mod in the game is an older version. Quit the game, then click Update Mod.") : L("The mod in the game is an older version. Click Update Mod to update it."))
                : (running ? L("The mod in the game is still {version}; this app is {app}. Quit the game, then click Update Mod.", ["version": v, "app": version])
                           : L("The mod in the game is still {version}; this app is {app}. Click Update Mod to update it.", ["version": v, "app": version]))
            symbol = "arrow.down.circle.fill"
        } else if state == "off" {
            line = L("Disabled")
            text = L("Disabled: the game starts without wasdmod. Your layouts are kept; click Enable to use it again.")
            symbol = "power.circle.fill"; tint = .secondaryLabelColor
        } else if state == "other" {
            line = L("Not installed")
            text = L("The game folder has a different xinput1_4.dll (another mod?). Install Mod replaces it and keeps a copy.")
            symbol = "exclamationmark.triangle.fill"; tint = .systemOrange
        } else {
            line = L("Not installed")
            text = L("Not installed yet. Quit the game, then click Install Mod.")
        }
        if installed { line += "  ·  " + L("Layout: {layout}", ["layout": layoutName(info["layout"] ?? "")]) }
        #if !NEXUS
        if updater.coversModState { text = nil } // the update card says it, as step 2
        #endif
        setStatus(busy ? nil : line) // (while busy: what's being done)
        bannerText.stringValue = text ?? ""
        bannerIcon.image = NSImage(systemSymbolName: symbol, accessibilityDescription: nil)
        bannerIcon.contentTintColor = tint
        banner.tint = tint
        banner.isHidden = text == nil
        bannerLine.isHidden = text == nil
        chooseButton.isHidden = found
        setupButton.isHidden = found

        // The bar's own button does what it says to do (blue, Return).
        actionButton.title = state == "off" ? L("Enable") : state == "older" ? L("Update Mod") : L("Install Mod")
        actionButton.isHidden = !found || state == "current"
        actionButton.isEnabled = !busy
    }

    func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] { [.flexibleSpace, .language] }
    func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] { toolbarDefaultItemIdentifiers(toolbar) }
    func toolbar(_ toolbar: NSToolbar, itemForItemIdentifier id: NSToolbarItem.Identifier, willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
        guard id == .language else { return nil }
        let item = NSToolbarItem(itemIdentifier: id)
        item.view = languagePicker
        languageItem = item
        showLanguage()
        return item
    }

    // The picker shows the current language (again after the editor picks another).
    func showLanguage() {
        languagePicker.selectItem(at: languages.order.firstIndex(of: lang) ?? 0)
        let shown = NSMenuItem(title: languages.names[lang] ?? lang, action: nil, keyEquivalent: "")
        shown.image = NSImage(systemSymbolName: "globe", accessibilityDescription: nil)
        (languagePicker.cell as? NSPopUpButtonCell)?.menuItem = shown
        languagePicker.toolTip = L("Language")
        languagePicker.setAccessibilityLabel(L("Language"))
        languageItem?.label = L("Language"); languageItem?.paletteLabel = L("Language")
    }

    // A language picked in the toolbar or in the editor: the app's own text follows (and
    // the system's panels from the next start). Picked in the toolbar, the editor follows too.
    func setLanguage(_ code: String, fromPage: Bool) {
        lang = code
        prefs.set(code, forKey: "lang")
        prefs.set([code == "pt" ? "pt-BR" : code], forKey: "AppleLanguages")
        applyTexts()
        if !fromPage, let web, let json = try? JSONSerialization.data(withJSONObject: [code]) {
            web.evaluateJavaScript("window.wasdmodSetLang && window.wasdmodSetLang(\(String(decoding: json, as: UTF8.self))[0]); 0")
        }
    }
    @objc func pickLanguage() {
        guard let code = languagePicker.selectedItem?.representedObject as? String, code != lang else { return showLanguage() }
        setLanguage(code, fromPage: false)
    }

    // The mod's state, for the editor page: window.wasdmodHost.status when it loads, then
    // window.wasdmodStatus(...) on every change. text is what refresh() found, or what's
    // being done (Installing…); state is none (also another mod's file), older, current or
    // off, for the page's mod buttons; busy while wasdmod.sh changes something (work()).
    var statusText = ""
    var pushedStatus: NSDictionary?
    var status: [String: Any] {
        let state = info["state"] ?? ""
        return ["text": statusText, "recording": info["recording"] == "yes", "found": !(info["game"] ?? "").isEmpty,
                "state": ["older", "current", "off"].contains(state) ? state : "none", "busy": busy]
    }
    func statusJSON() -> String {
        guard let d = try? JSONSerialization.data(withJSONObject: status) else { return "null" }
        return String(decoding: d, as: UTF8.self)
    }
    func setStatus(_ text: String? = nil) {
        if let text { statusText = text }
        let now = status as NSDictionary
        guard now != pushedStatus, let web else { return }
        pushedStatus = now
        web.evaluateJavaScript("window.wasdmodStatus && window.wasdmodStatus(\(statusJSON())); 0")
    }

    // Runs wasdmod.sh off the main thread (the CrossOver settings take a few seconds).
    func work(_ label: String, _ args: [String], _ extra: [String: String] = [:], done: @escaping (Bool, String) -> Void) {
        busy = true; setStatus(label); refresh()
        DispatchQueue.global().async {
            let r = self.sh(args, extra)
            DispatchQueue.main.async { self.busy = false; self.refresh(); done(r.ok, r.out) }
        }
    }

    func alert(_ title: String, _ text: String, style: NSAlert.Style = .informational, buttons: [String] = []) -> NSApplication.ModalResponse {
        #if SELFTEST
        if testHeadless { testLog("alert: \(title) | \(text)"); return .alertFirstButtonReturn }
        // WASDMOD_TEST_ALERT=1|2|3: answered with that button (logged), for the page's mod buttons.
        if let n = ProcessInfo.processInfo.environment["WASDMOD_TEST_ALERT"].flatMap(Int.init) {
            testLog("alert: \(title) | \(text) -> \(n)"); return NSApplication.ModalResponse(rawValue: NSApplication.ModalResponse.alertFirstButtonReturn.rawValue + n - 1)
        }
        #endif
        let a = NSAlert()
        a.messageText = title; a.informativeText = text; a.alertStyle = style
        for b in buttons.isEmpty ? [L("OK")] : buttons { a.addButton(withTitle: b) }
        return a.runModal()
    }

    // Install Mod (Update Mod, Reinstall Mod), Disable/Enable and Uninstall: the page's mod
    // buttons (and the bar's own button). Each calls done once it's all over, alerts included:
    // with the error's text when it failed, nil when it worked or was cancelled.
    typealias Done = (String?) -> Void
    @objc func bannerAction() { info["state"] == "off" ? enableDisable() : runInstall(force: false) }
    func runInstall(force: Bool, done: @escaping Done = { _ in }) {
        work(L("Installing…"), ["install"], force ? ["FORCE": "1"] : [:]) { ok, out in
            if ok {
                _ = self.alert(L("Installed. Start (or restart) Minecraft Dungeons II."),
                               L("Key layout: {layout}. Pick another one below and click Install/Apply Layout.\n\nIn game: Tab opens the menu wheel (the game's own key, S, moves you now), F9 shows the key list, hold Option for the cursor, T starts typing (Esc ends it), and the backtick key (`) turns the mod off and on.",
                                 ["layout": self.layoutName(self.fields(out)["layout"] ?? "")]))
                done(nil)
            } else if out.contains("different xinput1_4.dll") {
                if self.alert(L(out), L("Replace it? A copy is kept as xinput1_4.dll.other."), style: .warning, buttons: [L("Replace"), L("Cancel")]) == .alertFirstButtonReturn {
                    self.runInstall(force: true, done: done)
                } else { done(nil) }
            } else {
                _ = self.alert(L("Couldn't install"), L(out), style: .warning)
                done(L(out))
            }
        }
    }

    func uninstall(done: @escaping Done = { _ in }) {
        let r = alert(L("Uninstall the mod?"), L("The game goes back to click-to-move. Your saved layouts (author.txt, wasdmod*.txt) can stay for next time."),
                      buttons: [L("Uninstall"), L("Cancel"), L("Delete Layouts Too")])
        if r == .alertSecondButtonReturn { return done(nil) }
        work(L("Uninstalling…"), ["uninstall"], r == .alertThirdButtonReturn ? ["ALL": "1"] : [:]) { ok, out in
            _ = ok ? self.alert(L("Uninstalled."), L("Restart the game to go back to click-to-move."))
                   : self.alert(L("Couldn't uninstall"), L(out), style: .warning)
            done(ok ? nil : L(out))
        }
    }

    @objc func showFolder() { _ = sh(["folder"]) }

    // Record Logs (the editor's Troubleshooting): while on, the mod logs every key and
    // button it handles; the second click stops it and puts one report file on the
    // Desktop for a bug report.
    @objc func recordLogs() {
        if info["recording"] == "yes" {
            _ = sh(["record", "stop"])
            let r = sh(["report"], ["WASDMOD_VERSION": version])
            refresh()
            guard r.ok, let path = fields(r.out)["report"] else { _ = alert(L("Couldn't save the logs"), L(r.out), style: .warning); return }
            let url = URL(fileURLWithPath: path)
            NSWorkspace.shared.activateFileViewerSelecting([url])
            if alert(L("Saved {file} on your Desktop.", ["file": url.lastPathComponent]),
                     L("It has the mod's log, your key layout, and your Mac and CrossOver versions. Nothing you type is recorded. Attach it to a report on GitHub."),
                     buttons: [L("Report on GitHub"), L("Done")]) == .alertFirstButtonReturn {
                NSWorkspace.shared.open(URL(string: "https://github.com/Wanzho/mcd2-wasd/issues/new")!)
            }
        } else {
            let r = sh(["record", "start"])
            refresh()
            if !r.ok { _ = alert(L("Couldn't start recording"), L(r.out), style: .warning); return }
            _ = alert(L("Recording logs."), L("Play until the problem happens, then come back to wasdmod and stop recording under Troubleshooting to save the logs.\n\nIf the game is running, recording starts within a second; otherwise it starts with the game. Nothing you type is recorded."))
        }
    }

    // Disable: the game starts without the mod (its file is renamed); layouts stay.
    func enableDisable(done: @escaping Done = { _ in }) {
        let on = info["state"] == "off"
        work(on ? L("Enabling…") : L("Disabling…"), [on ? "on" : "off"]) { ok, out in
            if !ok { _ = self.alert(on ? L("Couldn't enable wasdmod") : L("Couldn't disable wasdmod"), L(out), style: .warning); return done(L(out)) }
            _ = on ? self.alert(L("Enabled."), L("Start (or restart) the game to use wasdmod again."))
                   : self.alert(L("Disabled."), L("From the next game start, the game runs without wasdmod. Your layouts are kept; click Enable to use it again.\n\nIn a running game, the backtick key (`) turns it off right away."))
            done(nil)
        }
    }

    #if NEXUS
    // The Nexus Mods build has no internet code: updates are on the mod's Nexus Mods
    // page, opened in the browser.
    @objc func openNexusPage() { NSWorkspace.shared.open(URL(string: "https://www.nexusmods.com/minecraftdungeons2/mods/104")!) }
    #endif

    // Getting the game itself to run in CrossOver (Steam, sign-in) is MCD2 Crossover's job.
    @objc func openSetupHelp() { NSWorkspace.shared.open(URL(string: "https://github.com/Wanzho/mcd2-crossover")!) }

    // A copy the automatic search doesn't find (a Minecraft Launcher copy, another
    // folder or bottle): the player picks the game's folder, its exe, or the bottle.
    @objc func chooseGame() { _ = pickGame() }
    // True once the game is found in the picked place.
    func pickGame() -> Bool {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = true
        panel.message = L("Pick Minecraft Dungeons II's folder, its Dungeons-Win64-Shipping.exe, or the CrossOver bottle it's in")
        panel.prompt = L("Use This")
        panel.directoryURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/CrossOver/Bottles")
        guard panel.runModal() == .OK, let url = panel.url else { return false }
        setStatus(L("Looking for the game in {folder}…", ["folder": url.lastPathComponent]))
        let r = sh(["locate", url.path])
        guard r.ok, let dir = fields(r.out)["game"], !dir.isEmpty else {
            refresh(); _ = alert(L("Minecraft Dungeons II wasn't found there."), L(r.out), style: .warning); return false
        }
        gameDir = dir
        refresh()
        return true
    }

    // Back to finding the game by itself (Steam in a CrossOver bottle).
    @objc func findGameAutomatically() { gameDir = nil; bottle = nil; refresh() }

    // ------------------------------------------------------------ the editor

    // window.wasdmodHost: the editor's saved layouts, saving into the game, the mod's
    // state, and the app's buttons that are in the page (the mod's own, the game folder,
    // Troubleshooting). nativeLanguage: the toolbar has the language picker, so the page
    // doesn't show its own; a language picked there comes as window.wasdmodSetLang(code).
    func hostScript() -> String {
        let saved = (try? Data(contentsOf: storeURL)) ?? Data("{}".utf8)
        // The layout the game uses, so the editor can show it (and add it when it's not there).
        if info.isEmpty { info = fields(sh(["status"]).out) }
        var game = Data("null".utf8)
        if let dir = info["game"], let file = info["layout"], !dir.isEmpty, !file.isEmpty,
           let text = try? String(contentsOfFile: dir + "/" + file, encoding: .utf8),
           let json = try? JSONSerialization.data(withJSONObject: ["file": file, "text": text]) { game = json }
        return """
        window.wasdmodHost = (() => {
          let store = {};
          try { store = JSON.parse(new TextDecoder().decode(Uint8Array.from(atob("\(saved.base64EncodedString())"), c => c.charCodeAt(0)))) || {}; } catch (e) {}
          const post = m => window.webkit.messageHandlers.wasdmod.postMessage(m);
          return {
            store,
            lang: "\(lang)",
            app: "mac",
            nativeLanguage: true,
            game: JSON.parse(new TextDecoder().decode(Uint8Array.from(atob("\(game.base64EncodedString())"), c => c.charCodeAt(0)))),
            set(k, v) { store[k] = v; post({ cmd: "store", value: JSON.stringify(store) }); },
            setLang(code) { post({ cmd: "lang", value: code }); },
            gameControls() { return post({ cmd: "controls" }); },
            waitGameControls(stamp) { return post({ cmd: "controls-wait", stamp }); },
            writeGameControls(data) { return post({ cmd: "controls-write", data }); },
            save(name, text) { return post({ cmd: "save", name, text }); },
            status: \(statusJSON()),
            chooseGameFolder() { return post({ cmd: "choose-folder" }); },
            showGameFolder() { return post({ cmd: "show-folder" }); },
            findGameAutomatically() { return post({ cmd: "find-game" }); },
            recordLogs() { return post({ cmd: "record" }); },
            modAction(name) { return post({ cmd: "mod", name }); },
            popupMenu(items, x, y) { return post({ cmd: "menu", items, x, y }); },
            exportFile(name, text) { return post({ cmd: "export", name, text }); }
          };
        })();
        """
    }

    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage,
                               replyHandler: @escaping (Any?, String?) -> Void) {
        guard let body = message.body as? [String: Any], let cmd = body["cmd"] as? String else { return replyHandler(nil, "Unknown request.") }
        if cmd == "store", let value = body["value"] as? String {
            try? FileManager.default.createDirectory(at: support, withIntermediateDirectories: true)
            try? Data(value.utf8).write(to: storeURL, options: .atomic)
            return replyHandler(nil, nil)
        }
        // A language picked in the editor (the toolbar's picker follows).
        if cmd == "lang", let value = body["value"] as? String, let code = languages.match(value) {
            setLanguage(code, fromPage: true)
            return replyHandler(nil, nil)
        }
        // The game's own keyboard settings file, as base64 (nil when there's none).
        if cmd == "controls" {
            if controlsWatch.path.map({ !FileManager.default.fileExists(atPath: $0) }) ?? true { controlsWatch.path = findControls() }
            guard let path = controlsWatch.path, let data = FileManager.default.contents(atPath: path) else { return replyHandler(nil, nil) }
            return replyHandler(data.base64EncodedString(), nil)
        }
        // The file's stamp, once it isn't the one the editor has (ControlsWatch).
        if cmd == "controls-wait" {
            if controlsWatch.path == nil { controlsWatch.path = findControls() }
            controlsWatch.wait(have: body["stamp"] as? String) { replyHandler($0, nil) }
            return
        }
        if cmd == "controls-write" {
            guard let text = body["data"] as? String, let data = Data(base64Encoded: text), data.starts(with: Array("GVAS".utf8)) else { return replyHandler(nil, "Unknown request.") }
            let file = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".sav")
            defer { try? FileManager.default.removeItem(at: file) }
            do { try data.write(to: file) } catch { return replyHandler(nil, error.localizedDescription) }
            let r = sh(["controls", file.path])
            return r.ok ? replyHandler("ok", nil) : replyHandler(nil, L(r.out))
        }
        // The app's buttons in the page. They run once this message is answered for (they
        // may show a panel or an alert), then answer: Choose Game Folder… "done" (the game
        // was found there) or "cancelled"; Record Logs "on" or "off" (after it, as the
        // toolbar button did).
        switch cmd {
        case "choose-folder": DispatchQueue.main.async { replyHandler(self.pickGame() ? "done" : "cancelled", nil) }; return
        case "show-folder": showFolder(); return replyHandler(nil, nil)
        case "find-game": findGameAutomatically(); return replyHandler(nil, nil)
        case "record": DispatchQueue.main.async { self.recordLogs(); replyHandler(self.info["recording"] == "yes" ? "on" : "off", nil) }; return
        default: break
        }
        // The mod's buttons in the page: install (Install/Update/Reinstall Mod), toggle
        // (Disable/Enable) or uninstall, as the toolbar's buttons were. Answered once it's all
        // over: nothing when it worked or was cancelled, the error's text when it failed
        // ("busy" while something else is being done).
        if cmd == "mod", let name = body["name"] as? String, ["install", "toggle", "uninstall"].contains(name) {
            if busy { return replyHandler(nil, "busy") }
            let done: Done = { replyHandler(nil, $0) }
            DispatchQueue.main.async {
                switch name {
                case "install": self.runInstall(force: false, done: done)
                case "toggle": self.enableDisable(done: done)
                default: self.uninstall(done: done)
                }
            }
            return
        }
        // A menu for a button in the page (the Game folder menu), shown with its top left at
        // x, y in the page: items as in popupMenu below. Answers the chosen item's id, or
        // nothing when the menu is closed without one.
        if cmd == "menu", let items = body["items"] as? [Any] {
            let x = (body["x"] as? NSNumber)?.doubleValue ?? 0, y = (body["y"] as? NSNumber)?.doubleValue ?? 0
            DispatchQueue.main.async { replyHandler(self.popupMenu(items, x: x, y: y), nil) }
            return
        }
        // A file from the editor (a layout to keep or share): "saved" or "cancelled".
        if cmd == "export", let name = body["name"] as? String, let text = body["text"] as? String {
            let panel = NSSavePanel()
            panel.nameFieldStringValue = (name as NSString).lastPathComponent
            panel.allowedContentTypes = [.plainText]
            #if SELFTEST
            // Test builds: WASDMOD_TEST_EXPORT=cancel closes the panel as Cancel would, or
            // =<folder> saves into that folder as Save would.
            if let to = ProcessInfo.processInfo.environment["WASDMOD_TEST_EXPORT"] {
                if to != "cancel" { panel.directoryURL = URL(fileURLWithPath: to) }
                DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self.window.endSheet(panel, returnCode: to == "cancel" ? .cancel : .OK) }
            }
            #endif
            panel.beginSheetModal(for: window) { r in
                guard r == .OK, let url = panel.url else { return replyHandler("cancelled", nil) }
                do { try Data(text.utf8).write(to: url, options: .atomic); replyHandler("saved", nil) }
                catch { replyHandler(nil, error.localizedDescription) }
            }
            return
        }
        guard cmd == "save", let name = body["name"] as? String, let text = body["text"] as? String else { return replyHandler(nil, "Unknown request.") }
        // wasdmod.sh load keeps the editor's names and validates the file.
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let file = dir.appendingPathComponent((name as NSString).lastPathComponent)
        defer { try? FileManager.default.removeItem(at: dir) }
        do {
            try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            try Data(text.utf8).write(to: file)
        } catch { return replyHandler(nil, error.localizedDescription) }
        let r = sh(["load", file.path])
        refresh()
        if !r.ok { return replyHandler(nil, L(r.out)) }
        let state = info["state"] ?? ""
        // Saved but not in use yet: "saved:" tells the editor it isn't an error.
        if state == "off" { return replyHandler(nil, "saved:" + L("Saved, but wasdmod is disabled: click Enable at the top.")) }
        if state != "current" && state != "older" { return replyHandler(nil, "saved:" + L("Saved, but the mod isn't installed yet: click Install Mod at the top.")) }
        replyHandler(fields(r.out)["layout"] ?? name, nil)
    }

    var menuPick: String?
    @objc func menuPicked(_ item: NSMenuItem) { menuPick = item.representedObject as? String }
    // The page's menu items: { id, title, enabled, icon, state, header, items }. id "-" is a
    // line; header: true a section title (not pickable); items: [...] a submenu (its own id
    // isn't used: the item picked in it answers); icon an SF Symbol name; state "on" a
    // checkmark. Anything that isn't an item is left out; submenus go 3 menus deep at most.
    func menuItems(_ items: [Any], depth: Int = 1) -> NSMenu {
        let menu = NSMenu()
        menu.autoenablesItems = false
        for case let i as [String: Any] in items.prefix(200) {
            let id = (i["id"] as? String) ?? (i["id"] as? NSNumber)?.stringValue ?? ""
            if id == "-" { menu.addItem(.separator()); continue }
            let title = (i["title"] as? String) ?? id
            if title.isEmpty { continue }
            if i["header"] as? Bool == true {
                if #available(macOS 14.0, *) { menu.addItem(.sectionHeader(title: title)); continue }
                let item = NSMenuItem(title: title, action: nil, keyEquivalent: "")
                item.attributedTitle = NSAttributedString(string: title, attributes: [
                    .font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize, weight: .semibold),
                    .foregroundColor: NSColor.secondaryLabelColor])
                item.isEnabled = false
                menu.addItem(item)
                continue
            }
            let sub = i["items"] as? [Any]
            if sub != nil && depth >= 3 { continue } // too deep
            let item = NSMenuItem(title: title, action: sub == nil ? #selector(menuPicked) : nil, keyEquivalent: "")
            if let sub = sub {
                item.submenu = menuItems(sub, depth: depth + 1)
                item.isEnabled = (i["enabled"] as? Bool ?? true) && item.submenu!.numberOfItems > 0
            } else {
                item.target = self; item.representedObject = id
                item.isEnabled = i["enabled"] as? Bool ?? true
            }
            if let name = i["icon"] as? String, !name.isEmpty, name.count < 100,
               let image = NSImage(systemSymbolName: name, accessibilityDescription: nil) {
                image.isTemplate = true
                item.image = image
            }
            if i["state"] as? String == "on" { item.state = .on }
            menu.addItem(item)
        }
        return menu
    }
    func popupMenu(_ items: [Any], x: Double, y: Double) -> String? {
        let menu = menuItems(items)
        // Page (CSS) pixels to the web view's points: times the zoom, from the top.
        var zoom = Double(web.magnification)
        if #available(macOS 11.0, *) { zoom *= Double(web.pageZoom) }
        let top = web.isFlipped ? y * zoom : Double(web.bounds.height) - y * zoom
        menuPick = nil
        #if SELFTEST
        // WASDMOD_TEST_MENU=<n>: three seconds later, item n is picked (-1: the menu is closed);
        // a path such as 3/2 picks item 2 of item 3's submenu. An item that can't be picked
        // (off, a header, a line, a submenu) is logged and the menu closed without a pick.
        // Test builds stay in the background, where a menu can't stay open: there the item
        // is picked from the same menu without showing it.
        if let path = ProcessInfo.processInfo.environment["WASDMOD_TEST_MENU"] {
            func describe(_ m: NSMenu) -> [String] {
                m.items.map { i in
                    if i.isSeparatorItem { return "-" }
                    var s = i.action == nil && i.submenu == nil ? "#" + i.title : i.title
                    if i.image != nil { s += " [icon]" }
                    if i.state == .on { s += " [on]" }
                    if !i.isEnabled { s += " (off)" }
                    if let sub = i.submenu { s += " > \(describe(sub))" }
                    return s
                }
            }
            testLog("menu open: \(describe(menu)) at \(x),\(y)")
            let pick = {
                let steps = path.split(separator: "/").map { Int($0) ?? -1 }
                var m: NSMenu? = menu
                for s in steps.dropLast() { m = (s >= 0 && s < m?.numberOfItems ?? 0) ? m?.item(at: s)?.submenu : nil }
                if let m = m, let last = steps.last, last >= 0, last < m.numberOfItems {
                    let item = m.item(at: last)!
                    if item.isEnabled && item.submenu == nil && item.action != nil { m.performActionForItem(at: last) }
                    else { testLog("menu: \(path) can't be picked (\(item.isSeparatorItem ? "line" : item.title))") }
                }
            }
            if !NSApp.isActive {
                pick()
                testLog("menu closed (in the background, not shown): \(menuPick ?? "(nothing picked)")")
                return menuPick
            }
            let t = Timer(timeInterval: 3, repeats: false) { _ in pick(); menu.cancelTracking() }
            RunLoop.main.add(t, forMode: .common)
        }
        #endif
        menu.popUp(positioning: nil, at: NSPoint(x: x * zoom, y: top), in: web)
        testLog("menu closed: \(menuPick ?? "(nothing picked)")")
        return menuPick
    }

    // Keys go to the editor (for picking keys), not to the buttons at the top.
    func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) { window.makeFirstResponder(webView) }

    // The editor fades in once it's drawn, instead of flashing an empty page.
    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        showEditor()
        pushedStatus = nil; setStatus() // (in case it changed while the page loaded)
        #if SELFTEST
        // WASDMOD_TEST_PICK=<code>: a second later, that language is picked in the toolbar.
        if let code = ProcessInfo.processInfo.environment["WASDMOD_TEST_PICK"], let i = languages.order.firstIndex(of: code) {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self.languagePicker.selectItem(at: i); self.languagePicker.sendAction(self.languagePicker.action, to: self.languagePicker.target) }
        }
        selfTest()
        #endif
    }
    func showEditor() {
        guard web.alphaValue < 1 else { return }
        NSAnimationContext.runAnimationGroup { $0.duration = 0.15; web.animator().alphaValue = 1 }
    }

    #if SELFTEST
    // Test build only: ControlsWatch on a copy of the settings file (WASDMOD_TEST_WATCH). The
    // file is written in place, then replaced (as a save that writes a new file does), then
    // left alone; each answer is logged with how long it took and how many stamps were read.
    func watchTest(_ path: String) {
        let w = controlsWatch
        w.path = path
        w.timeout = 3
        let t0 = Date()
        func log(_ s: String) { testLog(String(format: "watch %.2fs stats=%d %@", Date().timeIntervalSince(t0), w.stats, s)) }
        func later(_ s: Double, _ f: @escaping () -> Void) { DispatchQueue.global().asyncAfter(deadline: .now() + s, execute: f) }
        let s0 = w.stamp
        log("start \(s0)")
        w.wait(have: "old") { log("changed-stamp answered at once: \($0 != "old")") }
        w.wait(have: s0) { s1 in
            log("in-place write seen: \(s1 != s0)")
            w.wait(have: s1) { s2 in
                log("replaced file seen: \(s2 != s1)")
                let before = w.stats
                w.wait(have: s2) { s3 in
                    log("no change: answered at the timeout with the same stamp: \(s3 == s2), stamps read while idle: \(w.stats - before)")
                    NSApp.terminate(nil)
                }
            }
            later(1) { // replaced: a new file renamed over it
                let tmp = path + ".tmp"
                if let d = FileManager.default.contents(atPath: path) { try? (d + Data([0])).write(to: URL(fileURLWithPath: tmp)); rename(tmp, path); log("replaced") }
            }
        }
        later(1) { // written in place
            if let h = FileHandle(forWritingAtPath: path) { h.seekToEndOfFile(); h.write(Data([0])); h.closeFile(); log("wrote in place") }
        }
    }

    // Test build only (build with -D SELFTEST): runs WASDMOD_TEST_JS once the editor
    // has loaded, prints the result and the window number, and quits.
    func selfTest() {
        guard let js = ProcessInfo.processInfo.environment["WASDMOD_TEST_JS"] else { return }
        print("window=\(window.windowNumber)"); fflush(stdout)
        web.callAsyncJavaScript(js, arguments: [:], in: nil, in: .page) { r in
            switch r {
            case .success(let v): print("result=\(v)")
            case .failure(let e): print("error=\(e)")
            }
            fflush(stdout)
            DispatchQueue.main.asyncAfter(deadline: .now() + 3) { NSApp.terminate(nil) }
        }
    }
    #endif

    // Links open in the browser.
    func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        if let url = action.request.url, url.scheme == "http" || url.scheme == "https", action.navigationType == .linkActivated {
            NSWorkspace.shared.open(url); return decisionHandler(.cancel)
        }
        decisionHandler(.allow)
    }
    func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration, for action: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
        if let url = action.request.url { NSWorkspace.shared.open(url) }
        return nil
    }

    // ------------------------------------------------------------ window

    // The app's text in the current language (again when another is picked).
    func applyTexts() {
        window?.title = L("wasdmod for Minecraft Dungeons II") // (the app's own name stays wasdmod)
        showLanguage()
        chooseButton.title = L("Choose Game Folder…")
        setupButton.title = L("Get MCD2 Crossover")
        buildMenu()
        #if !NEXUS
        updater.show() // the update card's text too
        #endif
        refresh()
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Dark, like the editor and the website: the page's own near-black under a seamless toolbar.
        NSApp.appearance = NSAppearance(named: .darkAqua)
        let ink = NSColor(srgbRed: 11 / 255, green: 11 / 255, blue: 13 / 255, alpha: 1)
        let config = WKWebViewConfiguration()
        config.userContentController.addScriptMessageHandler(self, contentWorld: .page, name: "wasdmod")
        web = WKWebView(frame: .zero, configuration: config)
        web.navigationDelegate = self
        web.uiDelegate = self
        web.allowsLinkPreview = false
        web.alphaValue = 0
        if #available(macOS 12.0, *) { web.underPageBackgroundColor = ink }

        // The bar under the toolbar.
        bannerText.font = .systemFont(ofSize: 13)
        bannerText.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        bannerText.setContentHuggingPriority(.defaultLow, for: .horizontal)
        bannerIcon.symbolConfiguration = .init(pointSize: 15, weight: .regular)
        bannerIcon.setContentHuggingPriority(.required, for: .horizontal)
        for v in [bannerIcon, bannerText, setupButton, chooseButton, actionButton] { banner.addArrangedSubview(v) }
        banner.wantsLayer = true
        banner.orientation = .horizontal
        banner.alignment = .centerY
        banner.spacing = 10
        banner.edgeInsets = NSEdgeInsets(top: 9, left: 16, bottom: 9, right: 16)
        bannerLine.boxType = .separator
        var top: [NSView] = []
        #if !NEXUS
        top = [updater.card, updater.line] // the update card, while there's an update to tell about
        #endif
        let content = NSStackView(views: top + [banner, bannerLine, web])
        content.orientation = .vertical
        content.spacing = 0
        content.alignment = .leading
        for v in top + [banner, bannerLine, web] as [NSView] { v.widthAnchor.constraint(equalTo: content.widthAnchor).isActive = true }

        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1200, height: 860),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.minSize = NSSize(width: 760, height: 520)
        let toolbar = NSToolbar(identifier: "wasdmod")
        toolbar.delegate = self
        toolbar.displayMode = .iconOnly
        toolbar.allowsUserCustomization = false
        window.toolbar = toolbar
        window.toolbarStyle = .unified
        window.titlebarAppearsTransparent = true
        window.backgroundColor = ink
        window.contentView = content
        window.center()
        window.setFrameAutosaveName("wasdmod")
        applyTexts()
        #if SELFTEST
        if let path = ProcessInfo.processInfo.environment["WASDMOD_TEST_WATCH"] { return watchTest(path) }
        #endif
        #if !NEXUS
        updater.start() // finishes an update this app was started for, or checks (once a day)
        #endif
        if testHeadless { return } // (test builds: the update flow only, no window)
        // window.wasdmodHost, made now that refresh() has found the mod's state.
        web.configuration.userContentController.addUserScript(WKUserScript(source: hostScript(), injectionTime: .atDocumentStart, forMainFrameOnly: true))
        web.loadFileURL(res.appendingPathComponent("Key Layout Editor.html"), allowingReadAccessTo: res)
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.showEditor() }
        #if SELFTEST
        window.orderBack(nil) // test builds stay in the background: they never take the focus
        #else
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        #endif
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    // Back in the app after playing: the game folder may have changed.
    func applicationDidBecomeActive(_ notification: Notification) { if window != nil && !busy { refresh() } }

    // Check for Updates… (the GitHub build), or Get Updates on Nexus Mods… (the Nexus Mods build).
    var updatesMenuItem: (String, Selector?, String) {
        #if NEXUS
        return (L("Get Updates on Nexus Mods…"), #selector(openNexusPage), "")
        #else
        return (L("Check for Updates…"), #selector(checkNow), "")
        #endif
    }
    #if !NEXUS
    @objc func checkNow() { updater.checkNow() }
    #endif

    func buildMenu() {
        let main = NSMenu()
        func menu(_ title: String, _ items: [(String, Selector?, String)]) {
            let item = NSMenuItem(); item.submenu = NSMenu(title: title)
            for (t, action, key) in items {
                if t == "-" { item.submenu!.addItem(.separator()); continue }
                let mi = NSMenuItem(title: t, action: action, keyEquivalent: key)
                if key == "Z" { mi.keyEquivalent = "z"; mi.keyEquivalentModifierMask = [.command, .shift] }
                item.submenu!.addItem(mi)
            }
            main.addItem(item)
        }
        menu("wasdmod", [(L("About wasdmod"), #selector(NSApplication.orderFrontStandardAboutPanel(_:)), ""),
                         updatesMenuItem, ("-", nil, ""),
                         (L("Choose Game Folder…"), #selector(chooseGame), "o"), (L("Find Game Automatically"), #selector(findGameAutomatically), ""), ("-", nil, ""),
                         (L("Hide wasdmod"), #selector(NSApplication.hide(_:)), "h"), ("-", nil, ""),
                         (L("Quit wasdmod"), #selector(NSApplication.terminate(_:)), "q")])
        menu(L("Edit"), [(L("Undo"), Selector(("undo:")), "z"), (L("Redo"), Selector(("redo:")), "Z"), ("-", nil, ""),
                      (L("Cut"), #selector(NSText.cut(_:)), "x"), (L("Copy"), #selector(NSText.copy(_:)), "c"),
                      (L("Paste"), #selector(NSText.paste(_:)), "v"), (L("Select All"), #selector(NSText.selectAll(_:)), "a")])
        menu(L("Window"), [(L("Minimize"), #selector(NSWindow.performMiniaturize(_:)), "m"), (L("Close"), #selector(NSWindow.performClose(_:)), "w")])
        NSApp.mainMenu = main
    }
}

let app = NSApplication.shared
let delegate = App()
app.delegate = delegate
app.setActivationPolicy(testHeadless ? .prohibited : .regular)
app.run()

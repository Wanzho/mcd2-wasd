// wasdmod for Mac: installs the Minecraft Dungeons II controller mod into the
// CrossOver bottle that has the game, and edits its key layout.
//
// The install work is done by wasdmod.sh (in the app's Resources). The key layout
// editor is Key Layout Editor.html in a web view; window.wasdmodHost (below) keeps
// its layouts in ~/Library/Application Support/wasdmod and saves straight into
// the game folder.
import Cocoa
import WebKit

// ------------------------------------------------------------ languages
// Every language the game has (lang.json in Resources, made by lang.py from
// lang/*.json). English is the key: L("Install") is that text in the current
// language, or the English if it has none. The key layout editor picks the language.
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
// icon (updater.swift), logged to WASDMOD_TEST_LOG.
let testHeadless = ProcessInfo.processInfo.environment["WASDMOD_TEST_UPDATE"] != nil
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

extension NSToolbarItem.Identifier {
    static let folder = Self("folder"), record = Self("record"), power = Self("power"), uninstall = Self("uninstall"), install = Self("install")
}

final class App: NSObject, NSApplicationDelegate, NSToolbarDelegate, WKScriptMessageHandlerWithReply, WKNavigationDelegate, WKUIDelegate {
    var window: NSWindow!
    var web: WKWebView!
    // The window's toolbar: the mod's state in the subtitle, its buttons on the right.
    var tools: [NSToolbarItem.Identifier: NSToolbarItem] = [:]
    lazy var installButton = button(#selector(install))
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
        set { prefs.set(newValue, forKey: "game") }
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
        // Short in the subtitle; the whole story (and what to click) in the bar.
        var subtitle: String, text: String?, symbol = "info.circle.fill", tint = NSColor.controlAccentColor
        if !found {
            subtitle = L("Game not found")
            text = L("Minecraft Dungeons II wasn't found in a CrossOver bottle. Set it up with MCD2 Crossover (Steam in CrossOver), or if it's somewhere else (a Minecraft Launcher copy…), click Choose Game Folder.")
            symbol = "exclamationmark.triangle.fill"; tint = .systemOrange
        } else if state == "current" {
            subtitle = L("Installed")
        } else if state == "older" {
            // Never "up to date" while the game folder has an older mod than this app.
            subtitle = L("Mod in the game is out of date")
            let v = info["modversion"] ?? "", running = info["running"] == "yes"
            text = v.isEmpty ? (running ? L("The mod in the game is an older version. Quit the game, then click Update.") : L("The mod in the game is an older version. Click Update to update it."))
                : (running ? L("The mod in the game is still {version}; this app is {app}. Quit the game, then click Update.", ["version": v, "app": version])
                           : L("The mod in the game is still {version}; this app is {app}. Click Update to update it.", ["version": v, "app": version]))
            symbol = "arrow.down.circle.fill"
        } else if state == "off" {
            subtitle = L("Turned off")
            text = L("Turned off: the game starts without wasdmod. Your layouts are kept; click Turn On to use it again.")
            symbol = "power.circle.fill"; tint = .secondaryLabelColor
        } else if state == "other" {
            subtitle = L("Not installed")
            text = L("The game folder has a different xinput1_4.dll (another mod?). Install replaces it and keeps a copy.")
            symbol = "exclamationmark.triangle.fill"; tint = .systemOrange
        } else {
            subtitle = L("Not installed")
            text = L("Not installed yet. Quit the game, then click Install.")
        }
        if installed { subtitle += "  ·  " + L("Layout: {layout}", ["layout": layoutName(info["layout"] ?? "")]) }
        #if !NEXUS
        if updater.coversModState { text = nil } // the update card says it, as step 2
        #endif
        if !busy { window?.subtitle = subtitle }
        bannerText.stringValue = text ?? ""
        bannerIcon.image = NSImage(systemSymbolName: symbol, accessibilityDescription: nil)
        bannerIcon.contentTintColor = tint
        banner.tint = tint
        banner.isHidden = text == nil
        bannerLine.isHidden = text == nil
        chooseButton.isHidden = found
        setupButton.isHidden = found

        installButton.title = state == "current" || state == "off" ? L("Reinstall") : state == "older" ? L("Update") : L("Install")
        // The bar's own button does what it says to do (blue, Return).
        actionButton.title = state == "off" ? L("Turn On") : state == "older" ? L("Update") : L("Install")
        actionButton.isHidden = !found || state == "current"
        actionButton.isEnabled = !busy
        installButton.isEnabled = !busy
        let recording = info["recording"] == "yes"
        tool(.folder, L("Game Folder"), "folder", shown: true)
        (tools[.folder] as? NSMenuToolbarItem)?.menu = folderMenu()
        tool(.record, recording ? L("Stop & Save Logs") : L("Record Logs"), recording ? "stop.circle" : "record.circle", shown: found)
        tool(.power, state == "off" ? L("Turn On") : L("Turn Off"), "power", shown: installed)
        tool(.uninstall, L("Uninstall"), "trash", shown: installed)
        tool(.install, installButton.title, nil, shown: found)
    }

    // A toolbar button's text, icon, and whether it's there (greyed out before macOS 15).
    func tool(_ id: NSToolbarItem.Identifier, _ label: String, _ symbol: String?, shown: Bool) {
        guard let item = tools[id] else { return }
        item.label = label; item.paletteLabel = label; item.toolTip = label
        if let symbol { item.image = NSImage(systemSymbolName: symbol, accessibilityDescription: label) }
        if #available(macOS 15.0, *) { item.isHidden = !shown; item.isEnabled = !busy }
        else { item.isEnabled = shown && !busy }
        installButton.isEnabled = !busy
    }

    func folderMenu() -> NSMenu {
        let m = NSMenu()
        m.autoenablesItems = false
        let found = !(info["game"] ?? "").isEmpty
        for (title, action, on) in [(L("Show in Finder"), #selector(showFolder), found), (L("Choose Game Folder…"), #selector(chooseGame), true),
                                    (L("Find Game Automatically"), #selector(findGameAutomatically), gameDir != nil || bottle != nil)] {
            let mi = NSMenuItem(title: title, action: action, keyEquivalent: "")
            mi.target = self; mi.isEnabled = on && !busy
            m.addItem(mi)
        }
        return m
    }

    func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] { [.folder, .record, .power, .uninstall, .space, .install] }
    func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] { toolbarDefaultItemIdentifiers(toolbar) }
    func toolbar(_ toolbar: NSToolbar, itemForItemIdentifier id: NSToolbarItem.Identifier, willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
        // The folder button is a menu: show the game folder, or pick another one.
        let item = id == .folder ? NSMenuToolbarItem(itemIdentifier: id) : NSToolbarItem(itemIdentifier: id)
        item.autovalidates = false
        item.target = self
        switch id {
        case .folder: (item as? NSMenuToolbarItem)?.menu = folderMenu()
        case .record: item.action = #selector(recordLogs)
        case .power: item.action = #selector(turnOnOff)
        case .uninstall: item.action = #selector(uninstall)
        case .install: item.view = installButton
        default: return nil
        }
        item.isBordered = true
        tools[id] = item
        return item
    }

    // Runs wasdmod.sh off the main thread (the CrossOver settings take a few seconds).
    func work(_ label: String, _ args: [String], _ extra: [String: String] = [:], done: @escaping (Bool, String) -> Void) {
        busy = true; refresh(); window.subtitle = label
        DispatchQueue.global().async {
            let r = self.sh(args, extra)
            DispatchQueue.main.async { self.busy = false; self.refresh(); done(r.ok, r.out) }
        }
    }

    func alert(_ title: String, _ text: String, style: NSAlert.Style = .informational, buttons: [String] = []) -> NSApplication.ModalResponse {
        #if SELFTEST
        if testHeadless { testLog("alert: \(title) | \(text)"); return .alertFirstButtonReturn }
        #endif
        let a = NSAlert()
        a.messageText = title; a.informativeText = text; a.alertStyle = style
        for b in buttons.isEmpty ? [L("OK")] : buttons { a.addButton(withTitle: b) }
        return a.runModal()
    }

    @objc func install() { runInstall(force: false) }
    @objc func bannerAction() { info["state"] == "off" ? turnOnOff() : install() }
    func runInstall(force: Bool) {
        work(L("Installing…"), ["install"], force ? ["FORCE": "1"] : [:]) { ok, out in
            if ok {
                _ = self.alert(L("Installed. Start (or restart) Minecraft Dungeons II."),
                               L("Key layout: {layout}. Pick another one below and click Save to game.\n\nIn game: Tab opens the menu wheel (the game's own key, S, moves you now), F9 shows the key list, hold Option for the cursor, T starts typing (Esc ends it), and the backtick key (`) turns the mod off and on.",
                                 ["layout": self.layoutName(self.fields(out)["layout"] ?? "")]))
            } else if out.contains("different xinput1_4.dll") {
                if self.alert(L(out), L("Replace it? A copy is kept as xinput1_4.dll.other."), style: .warning, buttons: [L("Replace"), L("Cancel")]) == .alertFirstButtonReturn {
                    self.runInstall(force: true)
                }
            } else {
                _ = self.alert(L("Couldn't install"), L(out), style: .warning)
            }
        }
    }

    @objc func uninstall() {
        let r = alert(L("Uninstall the mod?"), L("The game goes back to click-to-move. Your saved layouts (author.txt, wasdmod*.txt) can stay for next time."),
                      buttons: [L("Uninstall"), L("Cancel"), L("Delete Layouts Too")])
        if r == .alertSecondButtonReturn { return }
        work(L("Uninstalling…"), ["uninstall"], r == .alertThirdButtonReturn ? ["ALL": "1"] : [:]) { ok, out in
            _ = ok ? self.alert(L("Uninstalled."), L("Restart the game to go back to click-to-move."))
                   : self.alert(L("Couldn't uninstall"), L(out), style: .warning)
        }
    }

    @objc func showFolder() { _ = sh(["folder"]) }

    // Record Logs: while on, the mod logs every key and button it handles; Stop &
    // Save Logs puts one report file on the Desktop for a bug report.
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
            _ = alert(L("Recording logs."), L("Play until the problem happens, then come back here and click Stop & Save Logs.\n\nIf the game is running, recording starts within a second; otherwise it starts with the game. Nothing you type is recorded."))
        }
    }

    // Off: the game starts without the mod (its file is renamed); layouts stay.
    @objc func turnOnOff() {
        let on = info["state"] == "off"
        let r = sh([on ? "on" : "off"])
        refresh()
        if !r.ok { _ = alert(on ? L("Couldn't turn it on") : L("Couldn't turn it off"), L(r.out), style: .warning); return }
        _ = on ? alert(L("Turned on."), L("Start (or restart) the game to use wasdmod again."))
               : alert(L("Turned off."), L("From the next game start, the game runs without wasdmod. Your layouts are kept; click Turn On to use it again.\n\nIn a running game, the backtick key (`) turns it off right away."))
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
    @objc func chooseGame() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = true
        panel.message = L("Pick Minecraft Dungeons II's folder, its Dungeons-Win64-Shipping.exe, or the CrossOver bottle it's in")
        panel.prompt = L("Use This")
        panel.directoryURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/CrossOver/Bottles")
        guard panel.runModal() == .OK, let url = panel.url else { return }
        window.subtitle = L("Looking for the game in {folder}…", ["folder": url.lastPathComponent])
        let r = sh(["locate", url.path])
        guard r.ok, let dir = fields(r.out)["game"], !dir.isEmpty else {
            refresh(); _ = alert(L("Minecraft Dungeons II wasn't found there."), L(r.out), style: .warning); return
        }
        gameDir = dir
        refresh()
    }

    // Back to finding the game by itself (Steam in a CrossOver bottle).
    @objc func findGameAutomatically() { gameDir = nil; bottle = nil; refresh() }

    // ------------------------------------------------------------ the editor

    // window.wasdmodHost: the editor's saved layouts, and saving into the game.
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
            game: JSON.parse(new TextDecoder().decode(Uint8Array.from(atob("\(game.base64EncodedString())"), c => c.charCodeAt(0)))),
            set(k, v) { store[k] = v; post({ cmd: "store", value: JSON.stringify(store) }); },
            setLang(code) { post({ cmd: "lang", value: code }); },
            gameControls() { return post({ cmd: "controls" }); },
            writeGameControls(data) { return post({ cmd: "controls-write", data }); },
            save(name, text) { return post({ cmd: "save", name, text }); }
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
        // A language picked in the editor: the app's own text follows (and the system's
        // panels from the next start).
        if cmd == "lang", let value = body["value"] as? String, let code = languages.match(value) {
            lang = code
            prefs.set(code, forKey: "lang")
            prefs.set([code == "pt" ? "pt-BR" : code], forKey: "AppleLanguages")
            applyTexts()
            return replyHandler(nil, nil)
        }
        // The game's own keyboard settings file, as base64 (nil when there's none).
        if cmd == "controls" {
            let r = sh(["controls"])
            guard r.ok, let path = fields(r.out)["controls"], let data = FileManager.default.contents(atPath: path) else { return replyHandler(nil, nil) }
            return replyHandler(data.base64EncodedString(), nil)
        }
        if cmd == "controls-write" {
            guard let text = body["data"] as? String, let data = Data(base64Encoded: text), data.starts(with: Array("GVAS".utf8)) else { return replyHandler(nil, "Unknown request.") }
            let file = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".sav")
            defer { try? FileManager.default.removeItem(at: file) }
            do { try data.write(to: file) } catch { return replyHandler(nil, error.localizedDescription) }
            let r = sh(["controls", file.path])
            return r.ok ? replyHandler("ok", nil) : replyHandler(nil, L(r.out))
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
        if state == "off" { return replyHandler(nil, "saved:" + L("Saved, but wasdmod is turned off: click Turn On at the top.")) }
        if state != "current" && state != "older" { return replyHandler(nil, "saved:" + L("Saved, but the mod isn't installed yet: click Install at the top.")) }
        replyHandler(fields(r.out)["layout"] ?? name, nil)
    }

    // Keys go to the editor (for picking keys), not to the buttons at the top.
    func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) { window.makeFirstResponder(webView) }

    // The editor fades in once it's drawn, instead of flashing an empty page.
    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        showEditor()
        #if SELFTEST
        selfTest()
        #endif
    }
    func showEditor() {
        guard web.alphaValue < 1 else { return }
        NSAnimationContext.runAnimationGroup { $0.duration = 0.15; web.animator().alphaValue = 1 }
    }

    #if SELFTEST
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

    // The app's text in the current language (again when the editor picks another).
    func applyTexts() {
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
        config.userContentController.addUserScript(WKUserScript(source: hostScript(), injectionTime: .atDocumentStart, forMainFrameOnly: true))
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
        window.title = "wasdmod"
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
        #if !NEXUS
        updater.start() // finishes an update this app was started for, or checks (once a day)
        #endif
        if testHeadless { return } // (test builds: the update flow only, no window)
        web.loadFileURL(res.appendingPathComponent("Key Layout Editor.html"), allowingReadAccessTo: res)
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.showEditor() }
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
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

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
let languages = Languages()
var lang = languages.match(UserDefaults.standard.string(forKey: "lang") ?? "")
    ?? Locale.preferredLanguages.lazy.compactMap { languages.match($0) }.first ?? "en"
func L(_ s: String, _ vars: [String: String] = [:]) -> String {
    var out = languages.text[lang]?[s].flatMap { $0.isEmpty ? nil : $0 } ?? s
    for (k, v) in vars { out = out.replacingOccurrences(of: "{\(k)}", with: v) }
    return out
}

final class App: NSObject, NSApplicationDelegate, WKScriptMessageHandlerWithReply, WKNavigationDelegate, WKUIDelegate {
    var window: NSWindow!
    var web: WKWebView!
    let status = NSTextField(wrappingLabelWithString: "")
    lazy var installButton = button(#selector(install))
    lazy var uninstallButton = button(#selector(uninstall))
    lazy var onOffButton = button(#selector(turnOnOff))
    lazy var recordButton = button(#selector(recordLogs))
    let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"
    lazy var folderButton = button(#selector(showFolder))
    lazy var bottleButton = button(#selector(chooseGame))
    lazy var setupButton = button(#selector(openSetupHelp))
    let res = Bundle.main.resourceURL!
    let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("wasdmod")
    var storeURL: URL { support.appendingPathComponent("editor.json") }
    var info: [String: String] = [:]
    var busy = false

    // The game's folder, picked by hand (wins over the automatic search while it exists).
    var gameDir: String? {
        get { UserDefaults.standard.string(forKey: "game") }
        set { UserDefaults.standard.set(newValue, forKey: "game") }
    }
    // A bottle picked by hand in earlier versions.
    var bottle: String? {
        get { UserDefaults.standard.string(forKey: "bottle") }
        set { UserDefaults.standard.set(newValue, forKey: "bottle") }
    }

    func button(_ action: Selector) -> NSButton {
        let b = NSButton(title: "", target: self, action: action)
        b.bezelStyle = .rounded
        return b
    }

    // ------------------------------------------------------------ wasdmod.sh

    func sh(_ args: [String], _ extra: [String: String] = [:]) -> (ok: Bool, out: String) {
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
        case "default.txt": return L("Default (the game's own keys)")
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
        var text: String
        if !found {
            text = L("Minecraft Dungeons II wasn't found in a CrossOver bottle. Set it up with MCD2 Crossover (Steam in CrossOver), or if it's somewhere else (a Minecraft Launcher copy…), click Choose Game Folder.")
        } else if state == "current" {
            text = L("Installed and up to date.")
        } else if state == "older" {
            text = L("An older version is installed. Click Update.")
        } else if state == "off" {
            text = L("Turned off: the game starts without wasdmod. Your layouts are kept; click Turn On to use it again.")
        } else if state == "other" {
            text = L("The game folder has a different xinput1_4.dll (another mod?). Install replaces it and keeps a copy.")
        } else {
            text = L("Not installed yet. Quit the game, then click Install.")
        }
        if installed { text += "  " + L("Key layout in the game: {layout}.", ["layout": layoutName(info["layout"] ?? "")]) }
        status.stringValue = text
        installButton.title = state == "current" || state == "off" ? L("Reinstall") : state == "older" ? L("Update") : L("Install")
        onOffButton.title = state == "off" ? L("Turn On") : L("Turn Off")
        onOffButton.isHidden = !installed
        recordButton.title = info["recording"] == "yes" ? L("Stop & Save Logs") : L("Record Logs")
        recordButton.isHidden = !found
        installButton.isHidden = !found
        installButton.isEnabled = !busy
        uninstallButton.isHidden = !installed
        uninstallButton.isEnabled = !busy
        folderButton.isHidden = !found
        bottleButton.isHidden = found
        setupButton.isHidden = found
        installButton.keyEquivalent = installed ? "" : "\r"
    }

    // Runs wasdmod.sh off the main thread (the CrossOver settings take a few seconds).
    func work(_ label: String, _ args: [String], _ extra: [String: String] = [:], done: @escaping (Bool, String) -> Void) {
        busy = true; refresh(); status.stringValue = label
        DispatchQueue.global().async {
            let r = self.sh(args, extra)
            DispatchQueue.main.async { self.busy = false; self.refresh(); done(r.ok, r.out) }
        }
    }

    func alert(_ title: String, _ text: String, style: NSAlert.Style = .informational, buttons: [String] = []) -> NSApplication.ModalResponse {
        let a = NSAlert()
        a.messageText = title; a.informativeText = text; a.alertStyle = style
        for b in buttons.isEmpty ? [L("OK")] : buttons { a.addButton(withTitle: b) }
        return a.runModal()
    }

    @objc func install() { runInstall(force: false) }
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
        status.stringValue = L("Looking for the game in {folder}…", ["folder": url.lastPathComponent])
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
        return """
        window.wasdmodHost = (() => {
          let store = {};
          try { store = JSON.parse(new TextDecoder().decode(Uint8Array.from(atob("\(saved.base64EncodedString())"), c => c.charCodeAt(0)))) || {}; } catch (e) {}
          const post = m => window.webkit.messageHandlers.wasdmod.postMessage(m);
          return {
            store,
            lang: "\(lang)",
            set(k, v) { store[k] = v; post({ cmd: "store", value: JSON.stringify(store) }); },
            setLang(code) { post({ cmd: "lang", value: code }); },
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
            UserDefaults.standard.set(code, forKey: "lang")
            UserDefaults.standard.set([code == "pt" ? "pt-BR" : code], forKey: "AppleLanguages")
            applyTexts()
            return replyHandler(nil, nil)
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

    #if SELFTEST
    // Test build only (build with -D SELFTEST): runs WASDMOD_TEST_JS once the editor
    // has loaded, prints the result and the window number, and quits.
    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        guard let js = ProcessInfo.processInfo.environment["WASDMOD_TEST_JS"] else { return }
        print("window=\(window.windowNumber)"); fflush(stdout)
        webView.callAsyncJavaScript(js, arguments: [:], in: nil, in: .page) { r in
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
        uninstallButton.title = L("Uninstall")
        folderButton.title = L("Game Folder")
        bottleButton.title = L("Choose Game Folder…")
        setupButton.title = L("Get MCD2 Crossover")
        window?.subtitle = L("Minecraft Dungeons II controller mod")
        buildMenu()
        refresh()
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        let config = WKWebViewConfiguration()
        config.userContentController.addScriptMessageHandler(self, contentWorld: .page, name: "wasdmod")
        config.userContentController.addUserScript(WKUserScript(source: hostScript(), injectionTime: .atDocumentStart, forMainFrameOnly: true))
        web = WKWebView(frame: .zero, configuration: config)
        web.navigationDelegate = self
        web.uiDelegate = self

        status.font = .systemFont(ofSize: 13)
        status.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        let bar = NSStackView(views: [status, setupButton, bottleButton, folderButton, recordButton, onOffButton, uninstallButton, installButton])
        bar.orientation = .horizontal
        bar.spacing = 8
        bar.edgeInsets = NSEdgeInsets(top: 10, left: 16, bottom: 10, right: 16)
        let line = NSBox(); line.boxType = .separator
        let content = NSStackView(views: [bar, line, web])
        content.orientation = .vertical
        content.spacing = 0
        content.alignment = .leading
        for v in [bar, line, web] as [NSView] { v.widthAnchor.constraint(equalTo: content.widthAnchor).isActive = true }

        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1200, height: 860),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "wasdmod"
        window.minSize = NSSize(width: 760, height: 520)
        window.contentView = content
        window.center()
        window.setFrameAutosaveName("wasdmod")
        applyTexts()
        web.loadFileURL(res.appendingPathComponent("Key Layout Editor.html"), allowingReadAccessTo: res)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    // Back in the app after playing: the game folder may have changed.
    func applicationDidBecomeActive(_ notification: Notification) { if window != nil && !busy { refresh() } }

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
        menu("wasdmod", [(L("About wasdmod"), #selector(NSApplication.orderFrontStandardAboutPanel(_:)), ""), ("-", nil, ""),
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
app.setActivationPolicy(.regular)
app.run()

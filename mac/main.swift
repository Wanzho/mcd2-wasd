// wasdmod for Mac: installs the Minecraft Dungeons II controller mod into the
// CrossOver bottle that has the game, and edits its key layout.
//
// The install work is done by wasdmod.sh (in the app's Resources). The key layout
// editor is Key Layout Editor.html in a web view; window.wasdmodHost (below) keeps
// its layouts in ~/Library/Application Support/wasdmod and saves straight into
// the game folder.
import Cocoa
import WebKit

final class App: NSObject, NSApplicationDelegate, WKScriptMessageHandlerWithReply, WKNavigationDelegate, WKUIDelegate {
    var window: NSWindow!
    var web: WKWebView!
    let status = NSTextField(wrappingLabelWithString: "")
    lazy var installButton = button("Install", #selector(install))
    lazy var uninstallButton = button("Uninstall", #selector(uninstall))
    lazy var onOffButton = button("Turn Off", #selector(turnOnOff))
    lazy var recordButton = button("Record Logs", #selector(recordLogs))
    let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"
    lazy var folderButton = button("Game Folder", #selector(showFolder))
    lazy var bottleButton = button("Choose Bottle…", #selector(chooseBottle))
    lazy var setupButton = button("Get MCD2 Crossover", #selector(openSetupHelp))
    let res = Bundle.main.resourceURL!
    let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("wasdmod")
    var storeURL: URL { support.appendingPathComponent("editor.json") }
    var info: [String: String] = [:]
    var busy = false

    // A bottle picked by hand when the game isn't in the usual place.
    var bottle: String? {
        get { UserDefaults.standard.string(forKey: "bottle") }
        set { UserDefaults.standard.set(newValue, forKey: "bottle") }
    }

    func button(_ title: String, _ action: Selector) -> NSButton {
        let b = NSButton(title: title, target: self, action: action)
        b.bezelStyle = .rounded
        return b
    }

    // ------------------------------------------------------------ wasdmod.sh

    func sh(_ args: [String], _ extra: [String: String] = [:]) -> (ok: Bool, out: String) {
        let p = Process()
        p.executableURL = URL(fileURLWithPath: "/bin/sh")
        p.arguments = [res.appendingPathComponent("wasdmod.sh").path] + args
        var env = ProcessInfo.processInfo.environment
        if let b = bottle { env["BOTTLE"] = b }
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
        case "default.txt": return "Default (the game's own keys)"
        case "author.txt": return "Recommended"
        case "": return "none"
        default: return "your own (\(f))"
        }
    }

    func refresh() {
        info = fields(sh(["status"]).out)
        let state = info["state"] ?? ""
        let found = !(info["game"] ?? "").isEmpty
        let installed = state == "current" || state == "older" || state == "off"
        var text: String
        if !found {
            text = "Minecraft Dungeons II wasn't found in a CrossOver bottle. Set it up with MCD2 Crossover (Steam in CrossOver), or choose the bottle it's in."
        } else if state == "current" {
            text = "Installed and up to date."
        } else if state == "older" {
            text = "An older version is installed. Click Update."
        } else if state == "off" {
            text = "Turned off: the game starts without wasdmod. Your layouts are kept; click Turn On to use it again."
        } else if state == "other" {
            text = "The game folder has a different xinput1_4.dll (another mod?). Install replaces it and keeps a copy."
        } else {
            text = "Not installed yet. Quit the game, then click Install."
        }
        if installed { text += "  Key layout in the game: \(layoutName(info["layout"] ?? ""))." }
        status.stringValue = text
        installButton.title = state == "current" || state == "off" ? "Reinstall" : state == "older" ? "Update" : "Install"
        onOffButton.title = state == "off" ? "Turn On" : "Turn Off"
        onOffButton.isHidden = !installed
        recordButton.title = info["recording"] == "yes" ? "Stop & Save Logs" : "Record Logs"
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

    func alert(_ title: String, _ text: String, style: NSAlert.Style = .informational, buttons: [String] = ["OK"]) -> NSApplication.ModalResponse {
        let a = NSAlert()
        a.messageText = title; a.informativeText = text; a.alertStyle = style
        for b in buttons { a.addButton(withTitle: b) }
        return a.runModal()
    }

    @objc func install() { runInstall(force: false) }
    func runInstall(force: Bool) {
        work("Installing…", ["install"], force ? ["FORCE": "1"] : [:]) { ok, out in
            if ok {
                _ = self.alert("Installed. Start (or restart) Minecraft Dungeons II.",
                               "Key layout: \(self.layoutName(self.fields(out)["layout"] ?? "")). Pick another one below and click Save to game.\n\nIn game: Tab opens the menu wheel (the game's own key, S, moves you now), F9 shows the key list, hold Option for the cursor, T starts typing (Esc ends it), and the backtick key (`) turns the mod off and on.")
            } else if out.contains("different xinput1_4.dll") {
                if self.alert(out, "Replace it? A copy is kept as xinput1_4.dll.other.", style: .warning, buttons: ["Replace", "Cancel"]) == .alertFirstButtonReturn {
                    self.runInstall(force: true)
                }
            } else {
                _ = self.alert("Couldn't install", out, style: .warning)
            }
        }
    }

    @objc func uninstall() {
        let r = alert("Uninstall the mod?", "The game goes back to click-to-move. Your saved layouts (author.txt, wasdmod*.txt) can stay for next time.",
                      buttons: ["Uninstall", "Cancel", "Delete Layouts Too"])
        if r == .alertSecondButtonReturn { return }
        work("Uninstalling…", ["uninstall"], r == .alertThirdButtonReturn ? ["ALL": "1"] : [:]) { ok, out in
            _ = ok ? self.alert("Uninstalled.", "Restart the game to go back to click-to-move.")
                   : self.alert("Couldn't uninstall", out, style: .warning)
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
            guard r.ok, let path = fields(r.out)["report"] else { _ = alert("Couldn't save the logs", r.out, style: .warning); return }
            let url = URL(fileURLWithPath: path)
            NSWorkspace.shared.activateFileViewerSelecting([url])
            if alert("Saved \(url.lastPathComponent) on your Desktop.",
                     "It has the mod's log, your key layout, and your Mac and CrossOver versions. Nothing you type is recorded. Attach it to a report on GitHub.",
                     buttons: ["Report on GitHub", "Done"]) == .alertFirstButtonReturn {
                NSWorkspace.shared.open(URL(string: "https://github.com/Wanzho/mcd2-wasd/issues/new")!)
            }
        } else {
            let r = sh(["record", "start"])
            refresh()
            if !r.ok { _ = alert("Couldn't start recording", r.out, style: .warning); return }
            _ = alert("Recording logs.", "Play until the problem happens, then come back here and click Stop & Save Logs.\n\nIf the game is running, recording starts within a second; otherwise it starts with the game. Nothing you type is recorded.")
        }
    }

    // Off: the game starts without the mod (its file is renamed); layouts stay.
    @objc func turnOnOff() {
        let on = info["state"] == "off"
        let r = sh([on ? "on" : "off"])
        refresh()
        if !r.ok { _ = alert("Couldn't turn it \(on ? "on" : "off")", r.out, style: .warning); return }
        _ = on ? alert("Turned on.", "Start (or restart) the game to use wasdmod again.")
               : alert("Turned off.", "From the next game start, the game runs without wasdmod. Your layouts are kept; click Turn On to use it again.\n\nIn a running game, the backtick key (`) turns it off right away.")
    }

    // Getting the game itself to run in CrossOver (Steam, sign-in) is MCD2 Crossover's job.
    @objc func openSetupHelp() { NSWorkspace.shared.open(URL(string: "https://github.com/Wanzho/mcd2-crossover")!) }

    @objc func chooseBottle() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false
        panel.message = "Pick the CrossOver bottle that has Minecraft Dungeons II"
        panel.directoryURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/CrossOver/Bottles")
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let old = bottle
        bottle = url.path
        refresh()
        if (info["game"] ?? "").isEmpty {
            bottle = old; refresh()
            _ = alert("Minecraft Dungeons II isn't in that bottle.", "Pick the bottle folder (in CrossOver › Bottles) where the game is installed.", style: .warning)
        }
    }

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
            set(k, v) { store[k] = v; post({ cmd: "store", value: JSON.stringify(store) }); },
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
        if !r.ok { return replyHandler(nil, r.out) }
        let state = info["state"] ?? ""
        if state == "off" { return replyHandler(nil, "Saved, but wasdmod is turned off: click Turn On at the top.") }
        if state != "current" && state != "older" { return replyHandler(nil, "Saved, but the mod isn't installed yet: click Install at the top.") }
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

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
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
        window.subtitle = "Minecraft Dungeons II controller mod"
        window.minSize = NSSize(width: 760, height: 520)
        window.contentView = content
        window.center()
        window.setFrameAutosaveName("wasdmod")
        refresh()
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
        menu("wasdmod", [("About wasdmod", #selector(NSApplication.orderFrontStandardAboutPanel(_:)), ""), ("-", nil, ""),
                         ("Hide wasdmod", #selector(NSApplication.hide(_:)), "h"), ("-", nil, ""),
                         ("Quit wasdmod", #selector(NSApplication.terminate(_:)), "q")])
        menu("Edit", [("Undo", Selector(("undo:")), "z"), ("Redo", Selector(("redo:")), "Z"), ("-", nil, ""),
                      ("Cut", #selector(NSText.cut(_:)), "x"), ("Copy", #selector(NSText.copy(_:)), "c"),
                      ("Paste", #selector(NSText.paste(_:)), "v"), ("Select All", #selector(NSText.selectAll(_:)), "a")])
        menu("Window", [("Minimize", #selector(NSWindow.performMiniaturize(_:)), "m"), ("Close", #selector(NSWindow.performClose(_:)), "w")])
        NSApp.mainMenu = main
    }
}

let app = NSApplication.shared
let delegate = App()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()

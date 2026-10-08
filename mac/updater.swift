// wasdmod for Mac: the one-click update (the GitHub build only; the Nexus Mods build
// is compiled with -D NEXUS, which leaves this whole file out).
//
// Checking: GitHub's "latest release" for this repository, when the app starts (at
// most once a day) and from Check for Updates. A newer version shows a card at the
// top of the window: What's New, Update Now. Nothing is downloaded until the player
// clicks Update Now.
//
// Updating has two steps, shown as such so it's clear what's done:
//  1. The app. Downloads the release's disk image (wasdmod-VERSION.dmg; wasdmod-Mac.dmg
//     before 1.4) and SHA256SUMS (only from this
//     repository's release files and GitHub's own download hosts), checks the disk
//     image against its checksum, opens it without Finder (read-only), checks the app
//     in it (its bundle id, version and signature), copies it next to this one, swaps
//     the two (this one is kept until the new one is running), and starts the new
//     one. A file URLSession downloads gets no quarantine flag, so Gatekeeper doesn't
//     stop the new app the way it stops a downloaded one. When this app's folder
//     can't be changed, the checked disk image is opened in Finder instead.
//  2. The mod in the game. The new app installs its mod into the game folder (Install
//     keeps the layouts and settings). While the game is running, this step waits and
//     finishes by itself once the game is closed (or with Finish Update).
#if !NEXUS
import Cocoa
import CryptoKit

// ------------------------------------------------------------ where releases come from

struct ReleaseSource {
    var api = URL(string: "https://api.github.com/repos/Wanzho/mcd2-wasd/releases/latest")!
    var downloads = "https://github.com/Wanzho/mcd2-wasd/releases/download/"
    var pages = "https://github.com/Wanzho/mcd2-wasd/releases/"
    let hosts: Set<String> = ["github.com", "api.github.com", "objects.githubusercontent.com",
                              "release-assets.githubusercontent.com", "github-releases.githubusercontent.com"]
    var testHost: String? // a test build's local server ("127.0.0.1:8123"), over http
    init() {
        #if SELFTEST
        // Test builds only: WASDMOD_TEST_API and WASDMOD_TEST_DOWNLOADS point it at a local server.
        let env = ProcessInfo.processInfo.environment
        if let a = env["WASDMOD_TEST_API"], let u = URL(string: a) { api = u; testHost = Self.hostPort(u) }
        if let d = env["WASDMOD_TEST_DOWNLOADS"] { downloads = d }
        #endif
    }
    static func hostPort(_ u: URL) -> String { (u.host ?? "").lowercased() + (u.port.map { ":\($0)" } ?? "") }
    // A hop GitHub may redirect to: its own hosts, over https.
    func allowedHop(_ u: URL) -> Bool {
        if u.user != nil || u.password != nil { return false }
        if let t = testHost { return u.scheme == "http" && Self.hostPort(u) == t }
        return u.scheme == "https" && u.port == nil && hosts.contains((u.host ?? "").lowercased())
    }
    // Where a download may start: this repository's release files.
    func allowedStart(_ u: URL) -> Bool {
        let s = u.absoluteString
        return s.hasPrefix(downloads) && !s.dropFirst(downloads.count).contains { "?#\\".contains($0) } && !s.contains("..") && allowedHop(u)
    }
}

// "1.10.0" > "1.9.2"; a test build ("dev") is never out of date.
func isNewer(_ a: String, than b: String) -> Bool {
    let x = a.split(separator: ".").map { Int($0) ?? 0 }, y = b.split(separator: ".").compactMap { Int($0) }
    if y.isEmpty { return false }
    for i in 0..<max(x.count, y.count) {
        let p = i < x.count ? x[i] : 0, q = i < y.count ? y[i] : 0
        if p != q { return p > q }
    }
    return false
}

struct Release {
    var version: String, page: URL, summary: String, dmg: URL?, dmgName = "", sums: URL?
    init?(json: Data, source: ReleaseSource) {
        guard let o = try? JSONSerialization.jsonObject(with: json) as? [String: Any], let tag = o["tag_name"] as? String else { return nil }
        let v = tag.hasPrefix("v") || tag.hasPrefix("V") ? String(tag.dropFirst()) : tag
        guard let first = v.first, first.isNumber, v.allSatisfy({ $0.isASCII && ($0.isNumber || $0 == ".") }) else { return nil }
        version = v
        let html = o["html_url"] as? String ?? ""
        page = (html.hasPrefix(source.pages) ? URL(string: html) : nil) ?? URL(string: source.pages)! // only ever this repository's page
        summary = notesSummary(o["body"] as? String ?? "")
        // The disk image: wasdmod-*.dmg, by preference wasdmod-<version>.dmg (older releases had wasdmod-Mac.dmg).
        var images: [(String, URL)] = []
        for asset in o["assets"] as? [[String: Any]] ?? [] {
            guard let name = asset["name"] as? String, let url = (asset["browser_download_url"] as? String).flatMap(URL.init(string:)) else { continue }
            if name.hasPrefix("wasdmod-"), name.hasSuffix(".dmg"), name.count > 12, name.allSatisfy({ $0.isASCII && ($0.isLetter || $0.isNumber || "._-".contains($0)) }) { images.append((name, url)) }
            if name == "SHA256SUMS" { sums = url }
        }
        if let image = images.first(where: { $0.0 == "wasdmod-\(v).dmg" }) ?? images.first { dmgName = image.0; dmg = image.1 }
    }
}

// The release notes in one line: the bold lead of each point of the first list
// ("- **Drag keys around:** ..." -> "Drag keys around"), else its first sentence, up
// to four, joined with " · ". (installer.c's release.inc does the same.)
func notesSummary(_ notes: String) -> String {
    func item(_ s: Substring) -> String {
        var t = String(s).replacingOccurrences(of: "\r", with: "")
        if t.hasPrefix("**"), let end = t.dropFirst(2).range(of: "**") { t = String(t[t.index(t.startIndex, offsetBy: 2)..<end.lowerBound]) }
        else if let r = t.range(of: #"[.!?](\s|$)"#, options: .regularExpression) { t = String(t[..<r.lowerBound]) }
        t = t.replacingOccurrences(of: #"\[([^\]]*)\]\([^)]*\)"#, with: "$1", options: .regularExpression)
        t = t.filter { !"*`_[]".contains($0) }.trimmingCharacters(in: CharacterSet(charactersIn: " :,"))
        if t.count > 70 { t = String(t.prefix(70)); if let sp = t.lastIndex(of: " ") { t = String(t[..<sp]) }; t += "…" }
        return t
    }
    var items: [String] = [], inList = false, first: Substring?
    // (GitHub's notes end lines with \r\n, which Swift counts as one character.)
    for raw in notes.replacingOccurrences(of: "\r\n", with: "\n").split(separator: "\n", omittingEmptySubsequences: false) {
        if items.count >= 4 { break }
        let line = raw.drop { $0 == " " || $0 == "\t" }, blank = line.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        if let m = line.first, "-*+".contains(m), line.dropFirst().first == " " { inList = true; items.append(item(line.dropFirst(2))) }
        else if inList && !blank { break }
        else if first == nil && !blank && !line.hasPrefix("#") && !line.hasPrefix(">") { first = line }
    }
    if items.isEmpty, let f = first { items.append(item(f)) }
    return items.filter { !$0.isEmpty }.joined(separator: " · ")
}

// The checksum SHA256SUMS lists for `name` ("<hash>  name" or "<hash> *name").
func checksum(in sums: String, for name: String) -> String? {
    for line in sums.split(whereSeparator: \.isNewline) {
        let parts = line.split(separator: " ", maxSplits: 1)
        guard parts.count == 2, parts[0].count == 64, parts[0].allSatisfy(\.isHexDigit) else { continue }
        var file = parts[1]
        if file.first == " " || file.first == "*" { file = file.dropFirst() }
        if file.trimmingCharacters(in: .whitespaces) == name { return parts[0].lowercased() }
    }
    return nil
}

// ------------------------------------------------------------ downloading

enum UpdateError: Error, Equatable {
    case offline, rateLimited, http, refused, tooBig, cancelled, notRelease, noFile, noSums, badSum, notTheApp, cantOpen, cantReplace(String), cantLaunch
    var text: String {
        switch self {
        case .offline: return L("Couldn't reach GitHub. Check the internet connection and try again.")
        case .rateLimited: return L("GitHub is limiting update checks right now. Try again in an hour.")
        case .http: return L("GitHub answered with an error. Try again later.")
        case .refused: return L("The download was redirected to an address outside GitHub, so wasdmod stopped it.")
        case .tooBig: return L("The download is much bigger than it should be, so wasdmod stopped it.")
        case .cancelled: return L("Update cancelled.")
        case .notRelease: return L("GitHub's answer wasn't a wasdmod release.")
        case .noFile: return L("This release has no Mac download.")
        case .noSums: return L("This release has no checksum file (SHA256SUMS), so wasdmod can't check the download.")
        case .badSum: return L("The download doesn't match its checksum, so wasdmod didn't use it.")
        case .notTheApp: return L("The download doesn't contain the right app, so wasdmod didn't use it.")
        case .cantOpen: return L("Couldn't open the downloaded disk image.")
        case .cantReplace(let why): return L("Couldn't put the new version in place: {error}", ["error": why])
        case .cantLaunch: return L("Couldn't start the new version, so wasdmod kept this one.")
        }
    }
}

// One GET into a file: redirects only to GitHub's own hosts, at most `limit` bytes,
// with progress; an ephemeral session (no cookies or cache).
final class Fetch: NSObject, URLSessionDownloadDelegate {
    private let source: ReleaseSource, limit: Int64
    private let progress: ((Double) -> Void)?, done: (Result<URL, UpdateError>) -> Void
    private var session: URLSession?, task: URLSessionDownloadTask?, file: URL?, failure: UpdateError?
    private let request: URLRequest, api: Bool

    init(_ url: URL, source: ReleaseSource, limit: Int64, progress: ((Double) -> Void)? = nil, done: @escaping (Result<URL, UpdateError>) -> Void) {
        self.source = source; self.limit = limit; self.progress = progress; self.done = done
        api = url == source.api
        var r = URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 20)
        r.setValue(api ? "application/vnd.github+json" : "application/octet-stream", forHTTPHeaderField: "Accept")
        if api { r.setValue("2022-11-28", forHTTPHeaderField: "X-GitHub-Api-Version") }
        r.setValue("wasdmod-mac/" + (Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"), forHTTPHeaderField: "User-Agent")
        request = r
    }
    func start() {
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForResource = 600
        session = URLSession(configuration: config, delegate: self, delegateQueue: nil)
        task = session?.downloadTask(with: request)
        task?.resume()
    }
    func cancel() { failure = .cancelled; task?.cancel() }

    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
        if let u = newRequest.url, source.allowedHop(u) { completionHandler(newRequest) }
        else { failure = .refused; completionHandler(nil) }
    }
    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData bytesWritten: Int64,
                    totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        if totalBytesWritten > limit || totalBytesExpectedToWrite > limit { failure = .tooBig; downloadTask.cancel(); return }
        if let progress, totalBytesExpectedToWrite > 0 {
            let p = Double(totalBytesWritten) / Double(totalBytesExpectedToWrite)
            DispatchQueue.main.async { progress(p) }
        }
    }
    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {
        guard failure == nil else { return }
        let status = (downloadTask.response as? HTTPURLResponse)?.statusCode ?? 0
        guard status == 200 else { failure = api && (status == 403 || status == 429) ? .rateLimited : .http; return }
        // Kept (the system deletes `location` when this returns).
        let keep = FileManager.default.temporaryDirectory.appendingPathComponent("wasdmod-update-" + UUID().uuidString)
        do { try FileManager.default.moveItem(at: location, to: keep); file = keep } catch { failure = .http }
    }
    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        session.finishTasksAndInvalidate()
        let result: Result<URL, UpdateError>
        if let failure { result = .failure(failure) }
        else if let file, error == nil { result = .success(file) }
        else { result = .failure((error as? URLError)?.code == .cancelled ? .cancelled : .offline) }
        if case .failure = result, let file { try? FileManager.default.removeItem(at: file) }
        DispatchQueue.main.async { self.done(result) }
    }
}

// ------------------------------------------------------------ the card

// One step: its sign (or a spinner), a bold headline with grey text under it, and
// (while downloading) a progress bar.
final class StepRow: NSStackView {
    enum Sign { case available, busy, done, waiting, problem, next }
    let icon = NSImageView(), spinner = NSProgressIndicator(), text = NSTextField(wrappingLabelWithString: ""), bar = NSProgressIndicator()
    init() {
        super.init(frame: .zero)
        orientation = .horizontal; alignment = .top; spacing = 10
        icon.symbolConfiguration = .init(pointSize: 15, weight: .regular)
        icon.setContentHuggingPriority(.required, for: .horizontal)
        spinner.style = .spinning; spinner.controlSize = .small; spinner.isDisplayedWhenStopped = false
        for v in [icon, spinner] as [NSView] { v.widthAnchor.constraint(equalToConstant: 20).isActive = true } // text lines up either way
        text.isSelectable = false
        text.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        text.setContentHuggingPriority(.defaultLow, for: .horizontal)
        bar.style = .bar; bar.isIndeterminate = false; bar.minValue = 0; bar.maxValue = 1; bar.controlSize = .small
        let column = NSStackView(views: [text, bar])
        column.orientation = .vertical; column.alignment = .leading; column.spacing = 6
        bar.widthAnchor.constraint(equalTo: column.widthAnchor).isActive = true
        for v in [icon, spinner, column] as [NSView] { addArrangedSubview(v) }
    }
    required init?(coder: NSCoder) { fatalError() }

    func show(_ sign: Sign, _ headline: String, _ detail: String? = nil, progress: Double? = nil) {
        let symbols: [Sign: (String, NSColor)] = [.available: ("arrow.down.circle.fill", .controlAccentColor), .done: ("checkmark.circle.fill", .systemGreen),
                                                  .waiting: ("pause.circle.fill", .controlAccentColor), .problem: ("exclamationmark.triangle.fill", .systemOrange),
                                                  .next: ("arrow.right.circle.fill", .controlAccentColor)]
        if let (name, tint) = symbols[sign] { icon.image = NSImage(systemSymbolName: name, accessibilityDescription: nil); icon.contentTintColor = tint }
        icon.isHidden = sign == .busy; spinner.isHidden = sign != .busy // (a stopped spinner still takes room)
        if sign == .busy { spinner.startAnimation(nil) } else { spinner.stopAnimation(nil) }
        let s = NSMutableAttributedString(string: headline, attributes: [.font: NSFont.systemFont(ofSize: 13, weight: .semibold), .foregroundColor: NSColor.labelColor])
        if let detail, !detail.isEmpty {
            s.append(NSAttributedString(string: "\n" + detail, attributes: [.font: NSFont.systemFont(ofSize: 12), .foregroundColor: NSColor.secondaryLabelColor]))
        }
        text.attributedStringValue = s
        bar.isHidden = progress == nil
        if let progress { bar.doubleValue = progress }
    }
}

// ------------------------------------------------------------ the updater

final class Updater: NSObject {
    unowned let app: App
    let source = ReleaseSource()
    let card = Banner(), line = NSBox()
    let row1 = StepRow(), row2 = StepRow()
    lazy var mainButton: NSButton = { let b = button(#selector(mainAction)); b.bezelColor = .controlAccentColor; return b }()
    lazy var secondButton = button(#selector(secondAction))
    lazy var closeButton: NSButton = {
        let b = NSButton(image: NSImage(systemSymbolName: "xmark", accessibilityDescription: L("Close"))!, target: self, action: #selector(close))
        b.isBordered = false; b.contentTintColor = .secondaryLabelColor; b.toolTip = L("Close")
        return b
    }()

    enum Phase: Equatable { case none, available, updating, failed(String), fallback(String), after }
    enum Step2: Equatable { case none, busy, done, doneOff, current, waiting, failed(String), install }
    var phase = Phase.none, step2 = Step2.none
    var release: Release?
    var fetch: Fetch?
    var checking = false, manualCheck = false
    var stage = 0, progress = 0.0 // while updating: 0 downloading, 1 checking, 2 installing, 3 reopening
    var updatedSummary = "", gameVersion = ""
    var waitTimer: Timer?

    var markerURL: URL { app.support.appendingPathComponent("update.json") }
    // The state bar's "older mod in the game" text is said here instead while step 2 runs.
    var coversModState: Bool { phase == .after && (step2 == .busy || step2 == .waiting || { if case .failed = step2 { return true }; return false }()) }

    init(app: App) {
        self.app = app
        super.init()
        let rows = NSStackView(views: [row1, row2])
        rows.orientation = .vertical; rows.alignment = .leading; rows.spacing = 10
        rows.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        for v in [rows, secondButton, mainButton, closeButton] as [NSView] { card.addArrangedSubview(v) }
        card.wantsLayer = true
        card.orientation = .horizontal; card.alignment = .centerY; card.spacing = 10
        card.edgeInsets = NSEdgeInsets(top: 10, left: 16, bottom: 10, right: 12)
        line.boxType = .separator
        card.isHidden = true; line.isHidden = true
    }
    func button(_ action: Selector) -> NSButton { let b = NSButton(title: "", target: self, action: action); b.bezelStyle = .rounded; return b }

    // ------------------------------------------------------------ checking

    // At launch: finish an update this app was started for, else check (once a day).
    func start() {
        if let data = try? Data(contentsOf: markerURL), let m = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
            try? FileManager.default.removeItem(at: markerURL)
            if m["to"] as? String == app.version { return afterUpdate(m) }
        }
        let last = prefs.object(forKey: "lastUpdateCheck") as? Date
        if last == nil || abs(last!.timeIntervalSinceNow) > 24 * 3600 { check(manual: false) }
    }
    @objc func checkNow() { check(manual: true) }

    func check(manual: Bool) {
        if manual { manualCheck = true }
        guard !checking, phase != .updating else { return }
        checking = true
        fetch = Fetch(source.api, source: source, limit: 4 << 20) { result in
            self.checking = false
            let manual = self.manualCheck; self.manualCheck = false
            switch result {
            case .success(let file):
                prefs.set(Date(), forKey: "lastUpdateCheck")
                let data = (try? Data(contentsOf: file)) ?? Data()
                try? FileManager.default.removeItem(at: file)
                guard let r = Release(json: data, source: self.source) else {
                    if manual { _ = self.app.alert(L("Couldn't check for updates"), UpdateError.notRelease.text, style: .warning) }
                    return self.testCheckDone()
                }
                self.release = r
                testLog("check: latest \(r.version), this app \(self.app.version), dmg \(r.dmgName.isEmpty ? "none" : r.dmgName) at \(r.dmg?.absoluteString ?? "-"), sums \(r.sums?.absoluteString ?? "none")")
                if isNewer(r.version, than: self.app.version) {
                    if self.phase != .after || self.step2Finished { self.phase = .available; self.show() }
                } else if manual {
                    _ = self.app.alert(L("wasdmod is up to date"), L("{version} is the newest version.", ["version": self.app.version]))
                }
            case .failure(let e):
                if e != .offline { prefs.set(Date(), forKey: "lastUpdateCheck") } // GitHub answered
                testLog("check failed: \(e.text)")
                if manual { _ = self.app.alert(L("Couldn't check for updates"), e.text, style: .warning) } // an automatic check stays quiet
            }
            self.testCheckDone()
        }
        fetch?.start()
    }
    var step2Finished: Bool { [.none, .done, .doneOff, .current, .install].contains(step2) }

    // ------------------------------------------------------------ step 1: the app

    @objc func updateNow() {
        guard let r = release, !checking, phase != .updating else { return }
        guard let dmg = r.dmg, source.allowedStart(dmg) else { return fail(.noFile) }
        guard let sums = r.sums, source.allowedStart(sums) else { return fail(.noSums) }
        phase = .updating; stage = 0; progress = 0; show()
        fetch = Fetch(sums, source: source, limit: 64 << 10) { result in
            guard case .success(let file) = result else { if case .failure(let e) = result { self.fail(e) }; return }
            let text = String(decoding: (try? Data(contentsOf: file)) ?? Data(), as: UTF8.self)
            try? FileManager.default.removeItem(at: file)
            guard let want = checksum(in: text, for: r.dmgName) else { return self.fail(.noSums) }
            self.fetch = Fetch(dmg, source: self.source, limit: 200 << 20, progress: { p in
                if Int(p * 100) != Int(self.progress * 100) { self.progress = p; self.show() } // each percent
            }) { result in
                switch result {
                case .failure(.cancelled): self.phase = .available; self.show()
                case .failure(let e): self.fail(e)
                case .success(let file):
                    self.stage = 1; self.show()
                    DispatchQueue.global().async {
                        let outcome = self.install(file, want: want, version: r.version)
                        DispatchQueue.main.async { self.installed(outcome, version: r.version) }
                    }
                }
            }
            self.fetch?.start()
        }
        fetch?.start()
    }
    func fail(_ e: UpdateError) { phase = .failed(e.text); show() }

    enum Outcome { case swapped(backup: URL), openInFinder(URL), failed(UpdateError) }

    // Off the main thread: check the download, and put the app in it in this one's place.
    func install(_ file: URL, want: String, version: String) -> Outcome {
        var keepFile = false
        defer { if !keepFile { try? FileManager.default.removeItem(at: file) } }
        guard let data = try? Data(contentsOf: file, options: .mappedIfSafe) else { return .failed(.cantOpen) }
        let got = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
        testLog("sha256 \(got == want ? "matches" : "DIFFERS"): \(got)")
        testLog("quarantine flag on the download: \(hasQuarantine(file) ? "yes" : "no")")
        guard got == want else { return .failed(.badSum) }
        let current = Bundle.main.bundleURL.resolvingSymlinksInPath(), folder = current.deletingLastPathComponent()
        // When this app can't be replaced, the checked disk image goes to Downloads, for Finder.
        func openInFinder() -> Outcome {
            var downloads = FileManager.default.urls(for: .downloadsDirectory, in: .userDomainMask).first ?? FileManager.default.temporaryDirectory
            #if SELFTEST
            if let d = ProcessInfo.processInfo.environment["WASDMOD_TEST_SAVE_DIR"] { downloads = URL(fileURLWithPath: d) } // tests: not the real Downloads
            #endif
            let dest = downloads.appendingPathComponent("wasdmod-\(version).dmg") // the release's own name
            try? FileManager.default.removeItem(at: dest)
            guard (try? FileManager.default.moveItem(at: file, to: dest)) != nil else { return .failed(.cantReplace(folder.path)) }
            keepFile = true
            return .openInFinder(dest)
        }
        if !canReplace(current) { return openInFinder() }
        DispatchQueue.main.async { self.stage = 2; self.show() }
        // Opened read-only, without a Finder window or a Finder sidebar entry.
        let mountDir = FileManager.default.temporaryDirectory.appendingPathComponent("wasdmod-mount-" + UUID().uuidString)
        try? FileManager.default.createDirectory(at: mountDir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: mountDir) }
        let attach = run("/usr/bin/hdiutil", ["attach", "-nobrowse", "-readonly", "-noautoopen", "-plist", "-mountrandom", mountDir.path, file.path])
        guard attach.ok, let plist = try? PropertyListSerialization.propertyList(from: Data(attach.out.utf8), format: nil) as? [String: Any],
              let mount = (plist["system-entities"] as? [[String: Any]])?.compactMap({ $0["mount-point"] as? String }).first else { return .failed(.cantOpen) }
        defer { if !run("/usr/bin/hdiutil", ["detach", mount, "-quiet"]).ok { _ = run("/usr/bin/hdiutil", ["detach", mount, "-quiet", "-force"]) } }
        testLog("mounted at \(mount)")
        // The app in it: ours (bundle id), the version the release says, and intact.
        let apps = (try? FileManager.default.contentsOfDirectory(at: URL(fileURLWithPath: mount), includingPropertiesForKeys: nil)) ?? []
        guard let new = apps.first(where: { $0.pathExtension == "app" && Bundle(url: $0)?.bundleIdentifier == Bundle.main.bundleIdentifier }),
              let info = NSDictionary(contentsOf: new.appendingPathComponent("Contents/Info.plist")),
              info["CFBundleShortVersionString"] as? String == version,
              run("/usr/bin/codesign", ["--verify", "--deep", "--strict", new.path]).ok else { return .failed(.notTheApp) }
        // Copied next to this one (without any quarantine flag), then the two swap places.
        let staged = folder.appendingPathComponent(".wasdmod-update-\(UUID().uuidString).app")
        let copy = run("/usr/bin/ditto", ["--noqtn", new.path, staged.path])
        guard copy.ok else { try? FileManager.default.removeItem(at: staged); return .failed(.cantReplace(copy.out)) }
        // (macOS may refuse to move an app it protects: then the disk image opens in Finder instead.)
        guard let backup = swapBundles(staged, current) else {
            let why = String(cString: strerror(errno))
            try? FileManager.default.removeItem(at: staged)
            testLog("swap refused (\(why)): the new version goes to Finder instead")
            return openInFinder()
        }
        testLog("swapped: \(current.path) is \(version) now, the old one is \(backup.lastPathComponent)")
        testLog("quarantine flag on the new app: \(hasQuarantine(current) ? "yes" : "no")")
        return .swapped(backup: backup)
    }

    // Back on the main thread: start the new app (this one quits once it runs), or say why not.
    func installed(_ outcome: Outcome, version: String) {
        switch outcome {
        case .failed(let e): fail(e)
        case .openInFinder(let dmg):
            phase = .fallback(dmg.path); show()
            #if SELFTEST
            if testHeadless { return testLog("would open \(dmg.path) in Finder") }
            #endif
            NSWorkspace.shared.open(dmg)
        case .swapped(let backup):
            stage = 3; show()
            let current = Bundle.main.bundleURL.resolvingSymlinksInPath()
            let marker: [String: Any] = ["to": version, "from": app.version, "summary": release?.summary ?? "", "backup": backup.path, "pid": Int(getpid())]
            try? FileManager.default.createDirectory(at: app.support, withIntermediateDirectories: true)
            try? JSONSerialization.data(withJSONObject: marker).write(to: markerURL, options: .atomic)
            let config = NSWorkspace.OpenConfiguration()
            config.createsNewApplicationInstance = true
            config.arguments = ["--updated"]
            #if SELFTEST
            if testHeadless {
                config.activates = false
                var env = ProcessInfo.processInfo.environment.filter { $0.key.hasPrefix("WASDMOD_") || $0.key == "GAMEDIR" || $0.key == "BOTTLE" }
                env["WASDMOD_TEST_UPDATE"] = "after"
                config.environment = env
            }
            #endif
            NSWorkspace.shared.openApplication(at: current, configuration: config) { running, error in
                DispatchQueue.main.async {
                    if running != nil && error == nil { testLog("started the new app (pid \(running!.processIdentifier)); quitting"); NSApp.terminate(nil); return }
                    // It didn't start: this one goes back in its place.
                    try? FileManager.default.removeItem(at: self.markerURL)
                    if self.swapBundles(backup, current) != nil { try? FileManager.default.removeItem(at: backup) }
                    self.fail(.cantLaunch)
                }
            }
        }
    }

    func canReplace(_ app: URL) -> Bool {
        let folder = app.deletingLastPathComponent()
        if app.path.contains("/AppTranslocation/") { return false } // run straight from a download: macOS's read-only copy
        if (try? folder.resourceValues(forKeys: [.volumeIsReadOnlyKey]))?.volumeIsReadOnly == true { return false }
        return FileManager.default.isWritableFile(atPath: folder.path) && FileManager.default.isWritableFile(atPath: app.path)
    }
    // Swaps two folders in one step (APFS, HFS+), or with two renames; returns where `b`'s old contents are now.
    func swapBundles(_ a: URL, _ b: URL) -> URL? {
        if renamex_np(a.path, b.path, UInt32(RENAME_SWAP)) == 0 { return a }
        let aside = b.deletingLastPathComponent().appendingPathComponent(".wasdmod-old-\(UUID().uuidString).app")
        guard rename(b.path, aside.path) == 0 else { return nil }
        guard rename(a.path, b.path) == 0 else { _ = rename(aside.path, b.path); return nil }
        return aside
    }
    func hasQuarantine(_ u: URL) -> Bool { getxattr(u.path, "com.apple.quarantine", nil, 0, 0, 0) >= 0 }
    func run(_ tool: String, _ args: [String]) -> (ok: Bool, out: String) {
        let p = Process(), out = Pipe()
        p.executableURL = URL(fileURLWithPath: tool); p.arguments = args
        p.standardOutput = out; p.standardError = out; p.standardInput = FileHandle.nullDevice
        do { try p.run() } catch { return (false, error.localizedDescription) }
        let text = String(decoding: out.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
        p.waitUntilExit()
        return (p.terminationStatus == 0, text.trimmingCharacters(in: .whitespacesAndNewlines))
    }

    // ------------------------------------------------------------ step 2: the mod in the game

    // This app was started by the old one after the swap: step 1 is done.
    func afterUpdate(_ m: [String: Any]) {
        updatedSummary = m["summary"] as? String ?? ""
        testLog("after the update: this app is \(app.version) (from \(m["from"] as? String ?? "?")), started with \(CommandLine.arguments.dropFirst().joined(separator: " "))")
        testLog("quarantine flag on this app: \(hasQuarantine(Bundle.main.bundleURL) ? "yes" : "no")")
        // The old app, kept until now: deleted once it has quit (only a backup next to this app).
        if let path = m["backup"] as? String {
            let backup = URL(fileURLWithPath: path), pid = pid_t(m["pid"] as? Int ?? 0)
            let ours = backup.lastPathComponent.hasPrefix(".wasdmod-") && backup.pathExtension == "app"
                && backup.deletingLastPathComponent().standardizedFileURL == Bundle.main.bundleURL.resolvingSymlinksInPath().deletingLastPathComponent().standardizedFileURL
            if ours {
                DispatchQueue.global().async {
                    for _ in 0..<100 where pid > 0 && kill(pid, 0) == 0 { usleep(100_000) }
                    if (try? FileManager.default.removeItem(at: backup)) == nil { try? FileManager.default.trashItem(at: backup, resultingItemURL: nil) }
                    testLog("old app removed: \(FileManager.default.fileExists(atPath: backup.path) ? "no" : "yes")")
                }
            }
        }
        phase = .after
        runStep2()
    }

    func runStep2() {
        app.refresh()
        let st = app.info["state"] ?? "", found = !(app.info["game"] ?? "").isEmpty
        gameVersion = app.info["modversion"] ?? ""
        if !found { step2 = .none }
        else if st == "none" || st == "other" { step2 = .install }
        else if st == "current" || (st == "off" && gameVersion == app.version) { step2 = .current }
        else { // older (or turned off with an older mod): installed again, its layouts kept
            let wasOff = st == "off"
            step2 = .busy; app.refresh(); show()
            app.work(L("Updating the mod in the game…"), ["install"]) { ok, out in
                if ok && wasOff { _ = self.app.sh(["off"]); self.app.refresh() }
                self.step2 = ok ? (wasOff ? .doneOff : .done) : out.contains("is running") ? .waiting : .failed(L(out))
                if self.step2 == .waiting { self.waitForGame() } else { self.waitTimer?.invalidate(); self.waitTimer = nil }
                self.app.refresh(); self.show()
            }
            return
        }
        app.refresh(); show()
    }
    // While the game runs: every few seconds, whether it's closed; then step 2 finishes.
    func waitForGame() {
        guard waitTimer == nil else { return }
        waitTimer = Timer.scheduledTimer(withTimeInterval: 3, repeats: true) { _ in
            guard !self.app.busy else { return }
            DispatchQueue.global().async {
                let running = self.app.fields(self.app.sh(["status"]).out)["running"] == "yes"
                DispatchQueue.main.async {
                    guard !running, self.step2 == .waiting, self.waitTimer != nil else { return }
                    self.waitTimer?.invalidate(); self.waitTimer = nil
                    testLog("the game was closed: finishing step 2")
                    self.runStep2()
                }
            }
        }
    }

    // ------------------------------------------------------------ the buttons

    @objc func mainAction() {
        switch phase {
        case .available, .failed: updateNow()
        case .after: waitTimer?.invalidate(); waitTimer = nil; runStep2() // Finish Update / Try Again
        default: break
        }
    }
    @objc func secondAction() {
        switch phase {
        case .updating: fetch?.cancel()
        case .available, .failed: NSWorkspace.shared.open(release?.page ?? URL(string: source.pages)!) // What's New
        case .fallback(let path): NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: path)])
        default: break
        }
    }
    @objc func close() { phase = .none; waitTimer?.invalidate(); waitTimer = nil; show(); app.refresh() } // (the state bar says what's left)

    // ------------------------------------------------------------ what the card says

    func show() {
        var main: String?, second: String?, closable = false
        row2.isHidden = true
        let new = release?.version ?? ""
        let whatsNew = { (s: String) in s.isEmpty ? nil : L("New: {summary}", ["summary": s]) }
        switch phase {
        case .none: break
        case .available:
            row1.show(.available, L("wasdmod {version} is available", ["version": new]), whatsNew(release?.summary ?? ""))
            main = L("Update Now"); second = L("What's New"); closable = true
        case .updating:
            let detail = [L("Downloading… {percent}%", ["percent": String(Int(progress * 100))]), L("Checking the download…"), L("Installing…"), L("Reopening wasdmod…")][stage]
            row1.show(.busy, L("Step 1 of 2: updating the app to {version}.", ["version": new]), detail, progress: stage == 0 ? progress : nil)
            if stage == 0 { second = L("Cancel") }
        case .failed(let why):
            row1.show(.problem, L("The app wasn't updated; nothing was changed."), why)
            main = L("Try Again"); second = L("What's New"); closable = true
        case .fallback(let path):
            row1.show(.next, L("The new version is open in Finder."),
                      L("wasdmod can't replace itself in this folder, so it opened the new version ({file}): drag wasdmod into Applications to replace this one. Your layouts are kept.",
                        ["file": (path as NSString).lastPathComponent]))
            second = L("Show in Finder"); closable = true
        case .after:
            let two = step2 != .none && step2 != .install
            row1.show(.done, two ? L("Step 1 of 2: the app is updated to {version}.", ["version": app.version]) : L("The app is updated to {version}.", ["version": app.version]),
                      whatsNew(updatedSummary))
            row2.isHidden = step2 == .none
            switch step2 {
            case .none: closable = true
            case .busy: row2.show(.busy, L("Step 2 of 2: updating the mod in the game…"))
            case .done: row2.show(.done, L("Step 2 of 2: the mod in the game is updated."), L("Start the game to use it.")); closable = true
            case .doneOff: row2.show(.done, L("Step 2 of 2: the mod in the game is updated."), L("It's still turned off: click Turn On to use it.")); closable = true
            case .current: row2.show(.done, L("Step 2 of 2: the mod in the game is up to date.")); closable = true
            case .waiting:
                row2.show(.waiting, L("Step 2 of 2: quit the game to finish."), gameVersion.isEmpty
                          ? L("The mod in the game is still the old version. The update finishes by itself once you close the game.")
                          : L("The mod in the game is still {version}. The update finishes by itself once you close the game.", ["version": gameVersion]))
                main = L("Finish Update")
            case .failed(let why): row2.show(.problem, L("Step 2 of 2: the mod in the game wasn't updated."), why); main = L("Try Again"); closable = true
            case .install: row2.show(.next, L("Next: install the mod in the game."), L("Click Install at the top.")); closable = true
            }
        }
        card.tint = { if case .failed = phase { return .systemOrange }; return phase == .after && step2Finished ? .systemGreen : .controlAccentColor }()
        mainButton.title = main ?? ""; mainButton.isHidden = main == nil
        secondButton.title = second ?? ""; secondButton.isHidden = second == nil
        closeButton.isHidden = !closable
        card.isHidden = phase == .none; line.isHidden = phase == .none
        #if SELFTEST
        testShow()
        #endif
    }

    #if SELFTEST
    // Test builds: WASDMOD_TEST_UPDATE=check|update|after runs this without a window
    // (see main.swift), logs each state of the card, clicks Update Now itself, and quits
    // when there's nothing more to do.
    var lastLogged = "", clicked = false
    func testShow() {
        guard testHeadless else { return }
        let rows = [row1, row2].filter { !$0.isHidden }.map { $0.text.stringValue.replacingOccurrences(of: "\n", with: " | ") }
        let line = "card: " + (phase == .none ? "(hidden)" : rows.joined(separator: " || ")) + (mainButton.isHidden ? "" : " [\(mainButton.title)]") + (secondButton.isHidden ? "" : " [\(secondButton.title)]")
        if line != lastLogged && !(phase == .updating && stage == 0 && progress > 0 && progress < 1) { testLog(line); lastLogged = line; testSnapshot() }
        let mode = ProcessInfo.processInfo.environment["WASDMOD_TEST_UPDATE"]
        switch phase {
        case .available where mode == "update" && !clicked: clicked = true; DispatchQueue.main.async { testLog("clicking Update Now"); self.updateNow() }
        case .failed, .fallback: testQuit()
        case .after where step2Finished || { if case .failed = step2 { return true }; return false }(): testQuit()
        default: break
        }
    }
    // WASDMOD_TEST_SNAPSHOT=DIR: each state of the card as a PNG, drawn off screen
    // (the window is never shown).
    var snapshots = 0
    func testSnapshot() {
        guard let dir = ProcessInfo.processInfo.environment["WASDMOD_TEST_SNAPSHOT"], let view = app.window?.contentView else { return }
        view.layoutSubtreeIfNeeded()
        let top = card.isHidden ? 0 : card.frame.height + 1, height = min(view.bounds.height, top + (app.banner.isHidden ? 0 : app.banner.frame.height + 1) + 40)
        let rect = NSRect(x: 0, y: view.bounds.height - height, width: view.bounds.width, height: height)
        guard let rep = view.bitmapImageRepForCachingDisplay(in: rect) else { return }
        view.cacheDisplay(in: rect, to: rep)
        // over the window's own colour, which the view doesn't draw
        let image = NSImage(size: rect.size)
        image.lockFocus()
        (app.window.backgroundColor ?? .black).setFill(); NSRect(origin: .zero, size: rect.size).fill()
        rep.draw(in: NSRect(origin: .zero, size: rect.size), from: .zero, operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
        image.unlockFocus()
        snapshots += 1
        if let tiff = image.tiffRepresentation, let png = NSBitmapImageRep(data: tiff)?.representation(using: .png, properties: [:]) {
            try? png.write(to: URL(fileURLWithPath: dir).appendingPathComponent("\(getpid())-\(snapshots).png"))
        }
    }
    func testCheckDone() {
        let mode = ProcessInfo.processInfo.environment["WASDMOD_TEST_UPDATE"]
        if testHeadless && (mode == "check" || (mode == "update" && phase != .available && phase != .updating)) { testQuit() }
    }
    func testQuit() { DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { testLog("done"); NSApp.terminate(nil) } }
    #else
    func testCheckDone() {}
    #endif
}
#endif

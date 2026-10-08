// Draws a picture of the disk image's Finder window without opening Finder: the
// background, the real icons and their names at the positions stored in the
// .DS_Store, inside a plain window frame. Used by check.py; the result is a
// composite, not a screenshot. Finder treats a window with a background picture as
// light whatever the Mac's appearance, and writes the names in black: so does this.
//
//   swift preview.swift spec.json
//
// spec.json: {"background": path, "width": 660, "height": 420, "iconSize": 128,
//   "textSize": 13, "title": "wasdmod", "appearance": "light" | "dark" (the frame only),
//   "items": [{"path": path, "label": "wasdmod", "x": 165, "y": 200}], "out": path}
import AppKit

struct Item: Decodable { let path: String; let label: String; let x: Double; let y: Double }
struct Spec: Decodable {
    let background: String
    let width: Double
    let height: Double
    let iconSize: Double
    let textSize: Double
    let title: String
    let appearance: String
    let items: [Item]
    let out: String
}

let spec = try! JSONDecoder().decode(Spec.self, from: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1])))
let dark = spec.appearance == "dark"
let scale = 2.0
let titleBar = 32.0   // a Tahoe window without a toolbar
let margin = 36.0     // room for the window shadow
let radius = 16.0
let W = spec.width, H = spec.height + titleBar
let canvas = NSSize(width: W + 2 * margin, height: H + 2 * margin)

let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(canvas.width * scale), pixelsHigh: Int(canvas.height * scale),
                           bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                           colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
rep.size = canvas
let ctx = NSGraphicsContext(bitmapImageRep: rep)!
// top-left origin, like Finder's icon positions
let flipped = NSGraphicsContext(cgContext: ctx.cgContext, flipped: true)
NSGraphicsContext.current = flipped
let cg = flipped.cgContext   // already in points (rep.size)
cg.translateBy(x: 0, y: canvas.height)
cg.scaleBy(x: 1, y: -1)

NSAppearance(named: dark ? .darkAqua : .aqua)!.performAsCurrentDrawingAppearance {
    let frame = NSRect(x: margin, y: margin, width: W, height: H)
    let shape = NSBezierPath(roundedRect: frame, xRadius: radius, yRadius: radius)

    // shadow
    NSGraphicsContext.saveGraphicsState()
    let shadow = NSShadow()
    shadow.shadowColor = NSColor.black.withAlphaComponent(dark ? 0.55 : 0.3)
    shadow.shadowBlurRadius = 24
    shadow.shadowOffset = NSSize(width: 0, height: -10)
    shadow.set()
    NSColor.windowBackgroundColor.setFill()
    shape.fill()
    NSGraphicsContext.restoreGraphicsState()

    NSGraphicsContext.saveGraphicsState()
    shape.addClip()
    NSColor.windowBackgroundColor.setFill()
    frame.fill()

    // title bar: traffic lights and the volume name
    for (i, c) in [NSColor(srgbRed: 1, green: 0.373, blue: 0.341, alpha: 1),
                   NSColor(srgbRed: 0.996, green: 0.737, blue: 0.18, alpha: 1),
                   NSColor(srgbRed: 0.157, green: 0.784, blue: 0.251, alpha: 1)].enumerated() {
        c.setFill()
        NSBezierPath(ovalIn: NSRect(x: margin + 14 + Double(i) * 20, y: margin + titleBar / 2 - 6.5, width: 13, height: 13)).fill()
    }
    let titleAttrs: [NSAttributedString.Key: Any] = [.font: NSFont.titleBarFont(ofSize: 13), .foregroundColor: NSColor.labelColor]
    let t = NSAttributedString(string: spec.title, attributes: titleAttrs)
    let ts = t.size()
    t.draw(at: NSPoint(x: margin + (W - ts.width) / 2, y: margin + (titleBar - ts.height) / 2))

    // content: the background at its own point size from the top-left, clipped to the window
    let content = NSRect(x: margin, y: margin + titleBar, width: W, height: spec.height)
    NSGraphicsContext.saveGraphicsState()
    NSBezierPath(rect: content).addClip()
    if let bg = NSImage(contentsOfFile: spec.background) {
        bg.draw(in: NSRect(origin: content.origin, size: bg.size), from: .zero, operation: .sourceOver, fraction: 1,
                respectFlipped: true, hints: [.interpolation: NSImageInterpolation.high])
    }

    // icons and names, as Finder lays them out: the stored point is the icon's centre
    let para = NSMutableParagraphStyle()
    para.alignment = .center
    para.lineBreakMode = .byTruncatingMiddle
    let labelAttrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: spec.textSize),
                                                    .foregroundColor: NSColor.black, .paragraphStyle: para]
    for item in spec.items {
        let s = spec.iconSize
        let r = NSRect(x: content.minX + item.x - s / 2, y: content.minY + item.y - s / 2, width: s, height: s)
        let icon = NSWorkspace.shared.icon(forFile: item.path)
        icon.size = NSSize(width: s, height: s)
        icon.draw(in: r, from: .zero, operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
        let lw = max(s + 32, 140.0)
        NSAttributedString(string: item.label, attributes: labelAttrs)
            .draw(in: NSRect(x: r.midX - lw / 2, y: r.maxY + 4, width: lw, height: spec.textSize * 1.5))
    }
    NSGraphicsContext.restoreGraphicsState()
    NSGraphicsContext.restoreGraphicsState()

    // hairline edge
    NSColor(white: dark ? 1 : 0, alpha: dark ? 0.14 : 0.12).setStroke()
    let edge = NSBezierPath(roundedRect: frame.insetBy(dx: 0.25, dy: 0.25), xRadius: radius, yRadius: radius)
    edge.lineWidth = 0.5
    edge.stroke()
}
NSGraphicsContext.current = nil
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: spec.out))
print("wrote \(spec.out)")

import AppKit
import Darwin
import Foundation

private let panelCounts = ["1", "2", "3", "4"]
private let panelSizes = ["32x32", "64x32", "64x64", "32x16"]
private let panelArrangements = ["Row", "Column", "Grid"]
private let panelRotations = ["0", "90", "180", "270"]
private let baudRates = ["9600", "19200", "38400", "57600", "115200", "230400"]

private let loadToolbarItem = NSToolbarItem.Identifier("LoadSketch")
private let verifyToolbarItem = NSToolbarItem.Identifier("VerifySketch")
private let uploadToolbarItem = NSToolbarItem.Identifier("UploadSketch")
private let stopToolbarItem = NSToolbarItem.Identifier("StopProcess")
private let portsToolbarItem = NSToolbarItem.Identifier("RefreshPorts")
private let openToolbarItem = NSToolbarItem.Identifier("OpenSketch")

private enum DestinationMode {
    case simulator
    case hardware
}

private struct PanelLayoutEntry {
    let sourceX: Int
    let sourceY: Int
    let width: Int
    let height: Int
    let x: Int
    let y: Int
    let rotation: Int
}

private struct SimulatorFrame {
    let width: Int
    let height: Int
    let rgb: [UInt8]
}

private func sketchName(_ path: String) -> String {
    let url = URL(fileURLWithPath: path)
    return url.deletingPathExtension().lastPathComponent
}

private func sketchTitle(_ path: String) -> String {
    let words = sketchName(path).split(separator: "_")
    return words.map { word in
        word.uppercased() == "M4" ? "M4" : word.capitalized
    }.joined(separator: " ")
}

private func parsePanelSize(_ value: String) -> (width: Int, height: Int) {
    let parts = value.split(separator: "x")
    if parts.count == 2, let width = Int(parts[0]), let height = Int(parts[1]) {
        return (width, height)
    }
    return (32, 32)
}

private func baudConstant(_ baud: String) -> speed_t {
    switch Int(baud) ?? 115200 {
    case 9600: return speed_t(B9600)
    case 19200: return speed_t(B19200)
    case 38400: return speed_t(B38400)
    case 57600: return speed_t(B57600)
    case 115200: return speed_t(B115200)
    case 230400: return speed_t(B230400)
    default: return speed_t(B115200)
    }
}

private func containsWorkspace(_ url: URL) -> Bool {
    return FileManager.default.fileExists(atPath: url.appendingPathComponent("Makefile").path) &&
        FileManager.default.fileExists(atPath: url.appendingPathComponent("Arduino").path)
}

private func resolveWorkspaceRoot() -> URL {
    let current = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
    if containsWorkspace(current) {
        return current
    }

    let bundle = Bundle.main.bundleURL
    let candidates = [
        bundle.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent(),
        bundle.deletingLastPathComponent(),
        bundle
    ]
    return candidates.first(where: containsWorkspace) ?? current
}

private final class SketchCellView: NSTableCellView {
    private let symbolView = NSImageView()
    private let titleLabel = NSTextField(labelWithString: "")
    private let pathLabel = NSTextField(labelWithString: "")

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setup()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setup()
    }

    private func setup() {
        symbolView.image = NSImage(systemSymbolName: "rectangle.grid.2x2", accessibilityDescription: "Sketch")
        symbolView.contentTintColor = .secondaryLabelColor
        symbolView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(symbolView)

        titleLabel.font = NSFont.systemFont(ofSize: 13, weight: .semibold)
        titleLabel.lineBreakMode = .byTruncatingTail
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        addSubview(titleLabel)

        pathLabel.font = NSFont.systemFont(ofSize: 11)
        pathLabel.textColor = .secondaryLabelColor
        pathLabel.lineBreakMode = .byTruncatingMiddle
        pathLabel.translatesAutoresizingMaskIntoConstraints = false
        addSubview(pathLabel)

        NSLayoutConstraint.activate([
            symbolView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10),
            symbolView.centerYAnchor.constraint(equalTo: centerYAnchor),
            symbolView.widthAnchor.constraint(equalToConstant: 18),
            symbolView.heightAnchor.constraint(equalToConstant: 18),

            titleLabel.leadingAnchor.constraint(equalTo: symbolView.trailingAnchor, constant: 9),
            titleLabel.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -10),
            titleLabel.topAnchor.constraint(equalTo: topAnchor, constant: 6),

            pathLabel.leadingAnchor.constraint(equalTo: titleLabel.leadingAnchor),
            pathLabel.trailingAnchor.constraint(equalTo: titleLabel.trailingAnchor),
            pathLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 1)
        ])
    }

    func configure(path: String) {
        titleLabel.stringValue = sketchTitle(path)
        pathLabel.stringValue = path
    }
}

private final class PanelLayoutPreviewView: NSView {
    var entries: [PanelLayoutEntry] = []
    var physicalWidth = 128
    var physicalHeight = 32
    var liveFrame: SimulatorFrame?

    override var isFlipped: Bool {
        return true
    }

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        guard let context = NSGraphicsContext.current?.cgContext else {
            return
        }

        NSColor(calibratedRed: 0.035, green: 0.04, blue: 0.05, alpha: 1).setFill()
        bounds.fill()

        let width = max(1, liveFrame?.width ?? physicalWidth)
        let height = max(1, liveFrame?.height ?? physicalHeight)
        let inset: CGFloat = 18
        let availableWidth = max(1, bounds.width - inset * 2)
        let availableHeight = max(1, bounds.height - inset * 2)
        let pixelScale = min(availableWidth / CGFloat(width), availableHeight / CGFloat(height))
        let drawWidth = CGFloat(width) * pixelScale
        let drawHeight = CGFloat(height) * pixelScale
        let originX = (bounds.width - drawWidth) / 2
        let originY = (bounds.height - drawHeight) / 2

        let matrixRect = CGRect(x: originX, y: originY, width: drawWidth, height: drawHeight)
        context.setFillColor(NSColor(calibratedRed: 0.02, green: 0.023, blue: 0.028, alpha: 1).cgColor)
        context.fill(matrixRect)

        context.setStrokeColor(NSColor(calibratedRed: 0.15, green: 0.18, blue: 0.22, alpha: 1).cgColor)
        context.setLineWidth(1)
        if pixelScale >= 3 {
            for x in 0...width {
                let px = originX + CGFloat(x) * pixelScale
                context.move(to: CGPoint(x: px, y: originY))
                context.addLine(to: CGPoint(x: px, y: originY + drawHeight))
            }
            for y in 0...height {
                let py = originY + CGFloat(y) * pixelScale
                context.move(to: CGPoint(x: originX, y: py))
                context.addLine(to: CGPoint(x: originX + drawWidth, y: py))
            }
            context.strokePath()
        }

        if let liveFrame, liveFrame.rgb.count >= width * height * 3 {
            for y in 0..<height {
                for x in 0..<width {
                    let offset = (y * width + x) * 3
                    let r = CGFloat(liveFrame.rgb[offset]) / 255.0
                    let g = CGFloat(liveFrame.rgb[offset + 1]) / 255.0
                    let b = CGFloat(liveFrame.rgb[offset + 2]) / 255.0
                    context.setFillColor(NSColor(calibratedRed: r, green: g, blue: b, alpha: 1).cgColor)
                    context.fill(CGRect(x: originX + CGFloat(x) * pixelScale,
                                        y: originY + CGFloat(y) * pixelScale,
                                        width: max(CGFloat(1), pixelScale),
                                        height: max(CGFloat(1), pixelScale)))
                }
            }
        } else {
            for (index, entry) in entries.enumerated() {
                let panelWidth = entry.rotation == 90 || entry.rotation == 270 ? entry.height : entry.width
                let panelHeight = entry.rotation == 90 || entry.rotation == 270 ? entry.width : entry.height
                let rect = CGRect(x: originX + CGFloat(entry.x) * pixelScale,
                                  y: originY + CGFloat(entry.y) * pixelScale,
                                  width: CGFloat(panelWidth) * pixelScale,
                                  height: CGFloat(panelHeight) * pixelScale)
                let hue = CGFloat(index) / CGFloat(max(1, entries.count))
                NSColor(calibratedHue: hue, saturation: 0.55, brightness: 0.22, alpha: 0.55).setFill()
                context.fill(rect.insetBy(dx: 1, dy: 1))
            }
        }

        for (index, entry) in entries.enumerated() {
            let panelWidth = entry.rotation == 90 || entry.rotation == 270 ? entry.height : entry.width
            let panelHeight = entry.rotation == 90 || entry.rotation == 270 ? entry.width : entry.height
            let rect = CGRect(x: originX + CGFloat(entry.x) * pixelScale,
                              y: originY + CGFloat(entry.y) * pixelScale,
                              width: CGFloat(panelWidth) * pixelScale,
                              height: CGFloat(panelHeight) * pixelScale)
            let hue = CGFloat(index) / CGFloat(max(1, entries.count))
            context.setStrokeColor(NSColor(calibratedHue: hue, saturation: 0.65, brightness: 0.82, alpha: 1).cgColor)
            context.setLineWidth(2)
            context.stroke(rect.insetBy(dx: 1, dy: 1))
        }
    }
}

private final class AppDelegate: NSObject,
                                 NSApplicationDelegate,
                                 NSWindowDelegate,
                                 NSTableViewDataSource,
                                 NSTableViewDelegate,
                                 NSToolbarDelegate,
                                 NSToolbarItemValidation,
                                 NSMenuItemValidation {
    private var window: NSWindow!
    private var sketchTable: NSTableView!
    private var modeControl: NSSegmentedControl!
    private var panelCountPopup: NSPopUpButton!
    private var panelSizePopup: NSPopUpButton!
    private var arrangementPopup: NSPopUpButton!
    private var rotationPopup: NSPopUpButton!
    private var geometryLabel: NSTextField!
    private var portPopup: NSPopUpButton!
    private var baudPopup: NSPopUpButton!
    private var serialOutput: NSTextView!
    private var serialInput: NSTextField!
    private var serialStatusLabel: NSTextField!
    private var codeTextView: NSTextView!
    private var editorTitleLabel: NSTextField!
    private var editorPathLabel: NSTextField!
    private var layoutPreviewView: PanelLayoutPreviewView!
    private var selectedSketchLabel: NSTextField!
    private var statusLabel: NSTextField!
    private var simUpButton: NSButton!
    private var simDownButton: NSButton!
    private var simAutoTiltButton: NSButton!
    private var accelXSlider: NSSlider!
    private var accelYSlider: NSSlider!
    private var analogA0Slider: NSSlider!
    private var sensorControls: [NSControl] = []

    private var destinationMode: DestinationMode = .simulator
    private var sketches: [String] = []
    private var ports: [String] = []
    private var serialRxBuffer = ""
    private var serialFd: Int32 = -1
    private var serialConnectedPort = ""
    private var serialConnectedBaud = ""
    private var serialTimer: Timer?
    private var layoutMonitorTimer: Timer?
    private var previewFrameTimer: Timer?
    private var activeProcess: Process?
    private var activeInputPipe: Pipe?
    private var runningSimulatorSketch: String?
    private var pendingSimulatorRestartSketch: String?
    private var layoutFileSnapshot = ""
    private var previewFrameSignature = ""
    private var loadedCustomPanelLayout = false
    private var workspaceRoot = resolveWorkspaceRoot()
    private var scale = 8
    private var layoutPath = "Arduino/sign_common/PanelLayout.h"
    private var simControlPath = "build/sim/sim-control.env"
    private var previewFramePath = "build/sim/live-preview.ppm"

    func applicationDidFinishLaunching(_ notification: Notification) {
        parseArguments()
        loadLayoutHeaderSettings()
        buildMenu()
        buildWindow()
        refreshSketches()
        refreshPorts(openHardwareSerial: false)
        updateModeUI()
        updateGeometryLabel()
        if layoutFileSnapshot.isEmpty || !loadedCustomPanelLayout {
            writePanelLayoutHeader(restartIfNeeded: false)
        }
        writeSimulatorControlState()
        appendConsoleLine("Simulator serial ready")
        serialTimer = Timer.scheduledTimer(timeInterval: 0.05,
                                           target: self,
                                           selector: #selector(pollSerial),
                                           userInfo: nil,
                                           repeats: true)
        layoutMonitorTimer = Timer.scheduledTimer(timeInterval: 0.75,
                                                  target: self,
                                                  selector: #selector(pollLayoutFile),
                                                  userInfo: nil,
                                                  repeats: true)
        previewFrameTimer = Timer.scheduledTimer(timeInterval: 1.0 / 30.0,
                                                 target: self,
                                                 selector: #selector(pollPreviewFrame),
                                                 userInfo: nil,
                                                 repeats: true)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationWillTerminate(_ notification: Notification) {
        activeProcess?.terminate()
        serialTimer?.invalidate()
        layoutMonitorTimer?.invalidate()
        previewFrameTimer?.invalidate()
        closeSerial(announce: false)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }

    private func parseArguments() {
        var iterator = CommandLine.arguments.dropFirst().makeIterator()
        while let arg = iterator.next() {
            switch arg {
            case "--scale":
                if let value = iterator.next(), let parsed = Int(value) {
                    scale = max(1, parsed)
                }
            case "--panel":
                if let value = iterator.next(), let index = panelSizes.firstIndex(of: value) {
                    pendingPanelSizeIndex = index
                }
            case "--panel-count":
                if let value = iterator.next(), let index = panelCounts.firstIndex(of: value) {
                    pendingPanelCountIndex = index
                }
            case "--layout":
                if let value = iterator.next() {
                    layoutPath = value
                }
            case "--control":
                if let value = iterator.next() {
                    simControlPath = value
                }
            case "--frame-file":
                if let value = iterator.next() {
                    previewFramePath = value
                }
            default:
                break
            }
        }
    }

    private var pendingPanelCountIndex = 3
    private var pendingPanelSizeIndex = 0
    private var pendingArrangementIndex = 0
    private var pendingRotationIndex = 0

    private func buildMenu() {
        let mainMenu = NSMenu()

        let appMenuItem = NSMenuItem()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "About Matrix Portal IDE", action: #selector(showAbout), keyEquivalent: "")
        appMenu.addItem(NSMenuItem.separator())
        appMenu.addItem(withTitle: "Quit Matrix Portal IDE", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        mainMenu.addItem(appMenuItem)

        let fileItem = NSMenuItem()
        let fileMenu = NSMenu(title: "File")
        fileMenu.addItem(withTitle: "Open Sketch", action: #selector(openSelectedSketch), keyEquivalent: "o")
        fileMenu.addItem(withTitle: "Clear Console", action: #selector(clearConsole), keyEquivalent: "k")
        fileItem.submenu = fileMenu
        mainMenu.addItem(fileItem)

        let runItem = NSMenuItem()
        let runMenu = NSMenu(title: "Run")
        runMenu.addItem(withTitle: "Run", action: #selector(loadSelectedSketch), keyEquivalent: "r")
        runMenu.addItem(withTitle: "Verify", action: #selector(verifySelectedSketch), keyEquivalent: "b")
        runMenu.addItem(withTitle: "Upload", action: #selector(uploadSelectedSketch), keyEquivalent: "u")
        runMenu.addItem(NSMenuItem.separator())
        runMenu.addItem(withTitle: "Stop", action: #selector(stopActiveProcess), keyEquivalent: ".")
        runItem.submenu = runMenu
        mainMenu.addItem(runItem)

        let deviceItem = NSMenuItem()
        let deviceMenu = NSMenu(title: "Device")
        deviceMenu.addItem(withTitle: "Refresh Ports", action: #selector(refreshPortsAction), keyEquivalent: "p")
        deviceItem.submenu = deviceMenu
        mainMenu.addItem(deviceItem)

        NSApp.mainMenu = mainMenu
    }

    private func buildWindow() {
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1500, height: 900),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Matrix Portal M4 IDE"
        window.minSize = NSSize(width: 1180, height: 760)
        window.center()
        window.delegate = self
        window.toolbar = buildToolbar()

        let contentView = NSView()
        window.contentView = contentView

        let browser = buildSketchBrowser()
        let editor = buildEditor()
        let inspector = buildInspector()
        browser.translatesAutoresizingMaskIntoConstraints = false
        editor.translatesAutoresizingMaskIntoConstraints = false
        inspector.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(browser)
        contentView.addSubview(editor)
        contentView.addSubview(inspector)

        let leftSeparator = NSView()
        leftSeparator.wantsLayer = true
        leftSeparator.layer?.backgroundColor = NSColor.separatorColor.cgColor
        leftSeparator.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(leftSeparator)

        let rightSeparator = NSView()
        rightSeparator.wantsLayer = true
        rightSeparator.layer?.backgroundColor = NSColor.separatorColor.cgColor
        rightSeparator.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(rightSeparator)

        let statusBar = buildStatusBar()
        statusBar.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(statusBar)
        statusBar.heightAnchor.constraint(equalToConstant: 28).isActive = true

        NSLayoutConstraint.activate([
            browser.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            browser.topAnchor.constraint(equalTo: contentView.topAnchor),
            browser.bottomAnchor.constraint(equalTo: statusBar.topAnchor),
            browser.widthAnchor.constraint(equalToConstant: 300),

            leftSeparator.leadingAnchor.constraint(equalTo: browser.trailingAnchor),
            leftSeparator.topAnchor.constraint(equalTo: contentView.topAnchor),
            leftSeparator.bottomAnchor.constraint(equalTo: statusBar.topAnchor),
            leftSeparator.widthAnchor.constraint(equalToConstant: 1),

            editor.leadingAnchor.constraint(equalTo: leftSeparator.trailingAnchor),
            editor.topAnchor.constraint(equalTo: contentView.topAnchor),
            editor.bottomAnchor.constraint(equalTo: statusBar.topAnchor),
            editor.widthAnchor.constraint(greaterThanOrEqualToConstant: 480),

            rightSeparator.leadingAnchor.constraint(equalTo: editor.trailingAnchor),
            rightSeparator.topAnchor.constraint(equalTo: contentView.topAnchor),
            rightSeparator.bottomAnchor.constraint(equalTo: statusBar.topAnchor),
            rightSeparator.widthAnchor.constraint(equalToConstant: 1),

            inspector.leadingAnchor.constraint(equalTo: rightSeparator.trailingAnchor),
            inspector.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
            inspector.topAnchor.constraint(equalTo: contentView.topAnchor),
            inspector.bottomAnchor.constraint(equalTo: statusBar.topAnchor),
            inspector.widthAnchor.constraint(equalToConstant: 410),

            statusBar.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            statusBar.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
            statusBar.bottomAnchor.constraint(equalTo: contentView.bottomAnchor)
        ])
    }

    private func buildToolbar() -> NSToolbar {
        let toolbar = NSToolbar(identifier: "MatrixPortalToolbar")
        toolbar.delegate = self
        toolbar.displayMode = .iconAndLabel
        toolbar.allowsUserCustomization = true
        return toolbar
    }

    private func buildStatusBar() -> NSView {
        let stack = NSStackView()
        stack.orientation = .horizontal
        stack.alignment = .centerY
        stack.spacing = 8
        stack.edgeInsets = NSEdgeInsets(top: 3, left: 12, bottom: 3, right: 12)

        selectedSketchLabel = NSTextField(labelWithString: "No sketch selected")
        selectedSketchLabel.lineBreakMode = .byTruncatingTail
        selectedSketchLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(selectedSketchLabel)

        let spacer = NSView()
        spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)
        stack.addArrangedSubview(spacer)

        statusLabel = NSTextField(labelWithString: "Ready")
        statusLabel.lineBreakMode = .byTruncatingTail
        statusLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(statusLabel)
        statusLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 260).isActive = true
        return stack
    }

    private func buildSketchBrowser() -> NSView {
        let sidebar = NSVisualEffectView()
        sidebar.material = .sidebar
        sidebar.blendingMode = .behindWindow
        sidebar.state = .active

        let container = NSStackView()
        container.orientation = .vertical
        container.alignment = .width
        container.spacing = 6
        container.edgeInsets = NSEdgeInsets(top: 12, left: 10, bottom: 10, right: 10)
        container.translatesAutoresizingMaskIntoConstraints = false
        sidebar.addSubview(container)

        let header = NSTextField(labelWithString: "Sketches")
        header.font = NSFont.boldSystemFont(ofSize: 13)
        header.textColor = .secondaryLabelColor
        container.addArrangedSubview(header)

        sketchTable = NSTableView()
        sketchTable.headerView = nil
        sketchTable.usesAlternatingRowBackgroundColors = false
        sketchTable.style = .sourceList
        sketchTable.backgroundColor = .clear
        sketchTable.intercellSpacing = NSSize(width: 0, height: 0)
        sketchTable.allowsEmptySelection = false
        sketchTable.rowHeight = 44
        sketchTable.delegate = self
        sketchTable.dataSource = self
        sketchTable.target = self
        sketchTable.doubleAction = #selector(loadSelectedSketch)

        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("sketch"))
        column.title = "Sketch"
        column.width = 260
        column.minWidth = 240
        column.resizingMask = .autoresizingMask
        sketchTable.addTableColumn(column)

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.documentView = sketchTable
        scroll.borderType = .noBorder
        scroll.drawsBackground = false
        container.addArrangedSubview(scroll)
        scroll.heightAnchor.constraint(greaterThanOrEqualToConstant: 520).isActive = true

        NSLayoutConstraint.activate([
            container.leadingAnchor.constraint(equalTo: sidebar.leadingAnchor),
            container.trailingAnchor.constraint(equalTo: sidebar.trailingAnchor),
            container.topAnchor.constraint(equalTo: sidebar.topAnchor),
            container.bottomAnchor.constraint(equalTo: sidebar.bottomAnchor)
        ])

        return sidebar
    }

    private func buildEditor() -> NSView {
        let container = NSView()

        let header = NSVisualEffectView()
        header.material = .headerView
        header.blendingMode = .withinWindow
        header.state = .active
        header.translatesAutoresizingMaskIntoConstraints = false
        container.addSubview(header)

        editorTitleLabel = NSTextField(labelWithString: "No Sketch")
        editorTitleLabel.font = NSFont.systemFont(ofSize: 14, weight: .semibold)
        editorTitleLabel.translatesAutoresizingMaskIntoConstraints = false
        header.addSubview(editorTitleLabel)

        editorPathLabel = NSTextField(labelWithString: "")
        editorPathLabel.font = NSFont.systemFont(ofSize: 11)
        editorPathLabel.textColor = .secondaryLabelColor
        editorPathLabel.lineBreakMode = .byTruncatingMiddle
        editorPathLabel.translatesAutoresizingMaskIntoConstraints = false
        header.addSubview(editorPathLabel)

        NSLayoutConstraint.activate([
            editorTitleLabel.leadingAnchor.constraint(equalTo: header.leadingAnchor, constant: 14),
            editorTitleLabel.trailingAnchor.constraint(equalTo: header.trailingAnchor, constant: -14),
            editorTitleLabel.topAnchor.constraint(equalTo: header.topAnchor, constant: 8),

            editorPathLabel.leadingAnchor.constraint(equalTo: editorTitleLabel.leadingAnchor),
            editorPathLabel.trailingAnchor.constraint(equalTo: editorTitleLabel.trailingAnchor),
            editorPathLabel.topAnchor.constraint(equalTo: editorTitleLabel.bottomAnchor, constant: 2)
        ])

        let scroll = NSScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.hasVerticalScroller = true
        scroll.hasHorizontalScroller = true
        scroll.autohidesScrollers = true
        scroll.borderType = .noBorder
        scroll.drawsBackground = true
        scroll.backgroundColor = .textBackgroundColor

        codeTextView = NSTextView()
        codeTextView.isEditable = false
        codeTextView.isSelectable = true
        codeTextView.isRichText = false
        codeTextView.importsGraphics = false
        codeTextView.font = NSFont.monospacedSystemFont(ofSize: 12.5, weight: .regular)
        codeTextView.textColor = .textColor
        codeTextView.backgroundColor = .textBackgroundColor
        codeTextView.textContainerInset = NSSize(width: 14, height: 12)
        codeTextView.isHorizontallyResizable = true
        codeTextView.autoresizingMask = [.width]
        codeTextView.textContainer?.containerSize = NSSize(width: CGFloat.greatestFiniteMagnitude,
                                                           height: CGFloat.greatestFiniteMagnitude)
        codeTextView.textContainer?.widthTracksTextView = false
        scroll.documentView = codeTextView
        container.addSubview(scroll)

        NSLayoutConstraint.activate([
            header.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            header.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            header.topAnchor.constraint(equalTo: container.topAnchor),
            header.heightAnchor.constraint(equalToConstant: 52),

            scroll.leadingAnchor.constraint(equalTo: container.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: container.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: header.bottomAnchor),
            scroll.bottomAnchor.constraint(equalTo: container.bottomAnchor)
        ])

        return container
    }

    private func buildInspector() -> NSView {
        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.autohidesScrollers = true
        scroll.drawsBackground = false
        scroll.borderType = .noBorder

        let document = NSView()
        document.translatesAutoresizingMaskIntoConstraints = false
        scroll.documentView = document

        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 12
        stack.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        stack.translatesAutoresizingMaskIntoConstraints = false
        document.addSubview(stack)

        stack.addArrangedSubview(section("Preview", buildPreviewSection()))
        stack.addArrangedSubview(section("Destination", buildDestinationSection()))
        stack.addArrangedSubview(section("Display Geometry", buildGeometrySection()))
        stack.addArrangedSubview(section("Serial I/O", buildSerialSection()))
        stack.addArrangedSubview(section("Simulated Device I/O", buildSensorSection()))

        NSLayoutConstraint.activate([
            document.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor),
            document.heightAnchor.constraint(greaterThanOrEqualTo: scroll.contentView.heightAnchor),
            stack.leadingAnchor.constraint(equalTo: document.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: document.trailingAnchor),
            stack.topAnchor.constraint(equalTo: document.topAnchor),
            stack.bottomAnchor.constraint(equalTo: document.bottomAnchor)
        ])

        return scroll
    }

    private func buildPreviewSection() -> NSView {
        layoutPreviewView = PanelLayoutPreviewView()
        layoutPreviewView.wantsLayer = true
        layoutPreviewView.layer?.cornerRadius = 8
        layoutPreviewView.layer?.masksToBounds = true
        layoutPreviewView.heightAnchor.constraint(equalToConstant: 220).isActive = true
        layoutPreviewView.widthAnchor.constraint(greaterThanOrEqualToConstant: 360).isActive = true
        return layoutPreviewView
    }

    private func section(_ title: String, _ view: NSView) -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 7

        let label = NSTextField(labelWithString: title)
        label.font = NSFont.boldSystemFont(ofSize: 12)
        stack.addArrangedSubview(label)
        stack.addArrangedSubview(view)
        return stack
    }

    private func labeledRow(_ title: String, _ control: NSView) -> NSView {
        let row = NSStackView()
        row.orientation = .horizontal
        row.alignment = .centerY
        row.spacing = 8

        let label = NSTextField(labelWithString: title)
        label.alignment = .right
        label.textColor = .secondaryLabelColor
        label.widthAnchor.constraint(equalToConstant: 92).isActive = true
        row.addArrangedSubview(label)
        row.addArrangedSubview(control)
        return row
    }

    private func buildDestinationSection() -> NSView {
        modeControl = NSSegmentedControl(labels: ["Simulator", "Hardware"],
                                         trackingMode: .selectOne,
                                         target: self,
                                         action: #selector(modeChanged))
        modeControl.selectedSegment = 0
        return modeControl
    }

    private func buildGeometrySection() -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 8

        panelCountPopup = NSPopUpButton(frame: .zero, pullsDown: false)
        panelCountPopup.addItems(withTitles: panelCounts.map { "\($0) panels" })
        panelCountPopup.selectItem(at: pendingPanelCountIndex)
        panelCountPopup.target = self
        panelCountPopup.action = #selector(geometryChanged)
        stack.addArrangedSubview(labeledRow("Panels", panelCountPopup))

        panelSizePopup = NSPopUpButton(frame: .zero, pullsDown: false)
        panelSizePopup.addItems(withTitles: panelSizes)
        panelSizePopup.selectItem(at: pendingPanelSizeIndex)
        panelSizePopup.target = self
        panelSizePopup.action = #selector(geometryChanged)
        stack.addArrangedSubview(labeledRow("Panel Size", panelSizePopup))

        arrangementPopup = NSPopUpButton(frame: .zero, pullsDown: false)
        arrangementPopup.addItems(withTitles: panelArrangements)
        arrangementPopup.selectItem(at: pendingArrangementIndex)
        arrangementPopup.target = self
        arrangementPopup.action = #selector(geometryChanged)
        stack.addArrangedSubview(labeledRow("Arrangement", arrangementPopup))

        rotationPopup = NSPopUpButton(frame: .zero, pullsDown: false)
        rotationPopup.addItems(withTitles: panelRotations.map { "\($0) deg" })
        rotationPopup.selectItem(at: pendingRotationIndex)
        rotationPopup.target = self
        rotationPopup.action = #selector(geometryChanged)
        stack.addArrangedSubview(labeledRow("Rotation", rotationPopup))

        geometryLabel = NSTextField(labelWithString: "")
        geometryLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(labeledRow("Matrix", geometryLabel))
        return stack
    }

    private func buildSerialSection() -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 8

        portPopup = NSPopUpButton(frame: .zero, pullsDown: false)
        portPopup.target = self
        portPopup.action = #selector(portChanged)
        stack.addArrangedSubview(labeledRow("USB Port", portPopup))

        baudPopup = NSPopUpButton(frame: .zero, pullsDown: false)
        baudPopup.addItems(withTitles: baudRates)
        baudPopup.selectItem(withTitle: "115200")
        baudPopup.target = self
        baudPopup.action = #selector(baudChanged)
        stack.addArrangedSubview(labeledRow("Baud", baudPopup))

        serialOutput = NSTextView()
        serialOutput.isEditable = false
        serialOutput.isSelectable = true
        serialOutput.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        serialOutput.textColor = .textColor
        serialOutput.backgroundColor = .textBackgroundColor

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.documentView = serialOutput
        scroll.borderType = .bezelBorder
        stack.addArrangedSubview(scroll)
        scroll.heightAnchor.constraint(equalToConstant: 170).isActive = true

        serialInput = NSTextField()
        serialInput.placeholderString = "Serial TX"
        serialInput.target = self
        serialInput.action = #selector(sendSerialInput)
        stack.addArrangedSubview(serialInput)

        serialStatusLabel = NSTextField(labelWithString: "Simulator")
        serialStatusLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(labeledRow("Session", serialStatusLabel))
        return stack
    }

    private func buildSensorSection() -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .width
        stack.spacing = 8

        simAutoTiltButton = NSButton(checkboxWithTitle: "Automatic tilt", target: self, action: #selector(simControlChanged))
        simAutoTiltButton.state = .on
        stack.addArrangedSubview(labeledRow("Tilt", simAutoTiltButton))
        sensorControls.append(simAutoTiltButton)

        let buttonRow = NSStackView()
        buttonRow.orientation = .horizontal
        buttonRow.spacing = 8
        simUpButton = NSButton(checkboxWithTitle: "UP", target: self, action: #selector(simControlChanged))
        simDownButton = NSButton(checkboxWithTitle: "DOWN", target: self, action: #selector(simControlChanged))
        buttonRow.addArrangedSubview(simUpButton)
        buttonRow.addArrangedSubview(simDownButton)
        stack.addArrangedSubview(labeledRow("Buttons", buttonRow))
        sensorControls.append(contentsOf: [simUpButton, simDownButton])

        accelXSlider = sensorSlider(min: -10, max: 10, value: 0)
        stack.addArrangedSubview(labeledRow("Accel X", accelXSlider))
        sensorControls.append(accelXSlider)

        accelYSlider = sensorSlider(min: -10, max: 10, value: 0)
        stack.addArrangedSubview(labeledRow("Accel Y", accelYSlider))
        sensorControls.append(accelYSlider)

        analogA0Slider = sensorSlider(min: 0, max: 1023, value: 512)
        stack.addArrangedSubview(labeledRow("Analog A0", analogA0Slider))
        sensorControls.append(analogA0Slider)
        return stack
    }

    private func sensorSlider(min: Double, max: Double, value: Double) -> NSSlider {
        let slider = NSSlider(value: value, minValue: min, maxValue: max, target: self, action: #selector(simControlChanged))
        slider.numberOfTickMarks = 0
        return slider
    }

    func numberOfRows(in tableView: NSTableView) -> Int {
        return sketches.count
    }

    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let identifier = NSUserInterfaceItemIdentifier("SketchCell")
        let cell = tableView.makeView(withIdentifier: identifier, owner: self) as? SketchCellView ?? SketchCellView()
        cell.identifier = identifier
        cell.configure(path: sketches[row])
        return cell
    }

    func tableViewSelectionDidChange(_ notification: Notification) {
        updateSelectedSketchLabel()
        loadSelectedSketchSource()
    }

    private func selectedSketchPath() -> String? {
        guard !sketches.isEmpty else {
            return nil
        }
        let row = sketchTable.selectedRow
        if row >= 0 && row < sketches.count {
            return sketches[row]
        }
        return sketches[0]
    }

    private func selectedPanelCount() -> String {
        return panelCounts[max(0, min(panelCountPopup.indexOfSelectedItem, panelCounts.count - 1))]
    }

    private func selectedPanelSize() -> String {
        return panelSizes[max(0, min(panelSizePopup.indexOfSelectedItem, panelSizes.count - 1))]
    }

    private func selectedArrangement() -> String {
        return panelArrangements[max(0, min(arrangementPopup.indexOfSelectedItem, panelArrangements.count - 1))]
    }

    private func selectedRotation() -> Int {
        return Int(panelRotations[max(0, min(rotationPopup.indexOfSelectedItem, panelRotations.count - 1))]) ?? 0
    }

    private func selectedBaudRate() -> String {
        return baudRates[max(0, min(baudPopup.indexOfSelectedItem, baudRates.count - 1))]
    }

    private func selectedPortPath() -> String? {
        guard destinationMode == .hardware, !ports.isEmpty else {
            return nil
        }
        let index = portPopup.indexOfSelectedItem
        if index >= 0 && index < ports.count {
            return ports[index]
        }
        return ports[0]
    }

    private func geometryAssignments() -> [String] {
        let size = parsePanelSize(selectedPanelSize())
        return [
            "PANEL_COUNT=\(selectedPanelCount())",
            "PANEL_WIDTH=\(size.width)",
            "PANEL_HEIGHT=\(size.height)",
            "PANEL_LAYOUT_FILE=\(layoutPath)",
            "SIM_PANEL=\(selectedPanelSize())",
            "SIM_LAYOUT=\(layoutPath)",
            "SIM_CONTROL=\(simControlPath)"
        ]
    }

    private func updateGeometryLabel() {
        guard geometryLabel != nil else {
            return
        }
        let count = Int(selectedPanelCount()) ?? 1
        let size = parsePanelSize(selectedPanelSize())
        let metrics = panelLayoutMetrics(count: count, size: size, rotation: selectedRotation(), arrangement: selectedArrangement())
        geometryLabel.stringValue = "\(metrics.physicalWidth)x\(metrics.physicalHeight), chain \(metrics.sourceWidth)x\(metrics.sourceHeight)"
        updateLayoutPreview()
    }

    private func updateLayoutPreview() {
        guard layoutPreviewView != nil else {
            return
        }
        let count = Int(selectedPanelCount()) ?? 1
        let size = parsePanelSize(selectedPanelSize())
        let metrics = panelLayoutMetrics(count: count, size: size, rotation: selectedRotation(), arrangement: selectedArrangement())
        layoutPreviewView.entries = panelLayoutEntries()
        layoutPreviewView.physicalWidth = metrics.physicalWidth
        layoutPreviewView.physicalHeight = metrics.physicalHeight
        if runningSimulatorSketch == nil {
            layoutPreviewView.liveFrame = nil
        }
        layoutPreviewView.needsDisplay = true
    }

    private func layoutFileURL() -> URL {
        return layoutPath.hasPrefix("/")
            ? URL(fileURLWithPath: layoutPath)
            : workspaceRoot.appendingPathComponent(layoutPath)
    }

    private func previewFrameURL() -> URL {
        return previewFramePath.hasPrefix("/")
            ? URL(fileURLWithPath: previewFramePath)
            : workspaceRoot.appendingPathComponent(previewFramePath)
    }

    private func parsePPMFrame(_ data: Data) -> SimulatorFrame? {
        let bytes = [UInt8](data)
        var index = 0

        func isSpace(_ byte: UInt8) -> Bool {
            return byte == 9 || byte == 10 || byte == 13 || byte == 32
        }

        func skipWhitespaceAndComments() {
            while index < bytes.count {
                if isSpace(bytes[index]) {
                    index += 1
                    continue
                }
                if bytes[index] == 35 {
                    while index < bytes.count && bytes[index] != 10 {
                        index += 1
                    }
                    continue
                }
                break
            }
        }

        func nextToken() -> String? {
            skipWhitespaceAndComments()
            let start = index
            while index < bytes.count && !isSpace(bytes[index]) {
                index += 1
            }
            guard index > start else {
                return nil
            }
            return String(bytes: bytes[start..<index], encoding: .utf8)
        }

        guard nextToken() == "P6",
              let widthText = nextToken(),
              let heightText = nextToken(),
              let maxText = nextToken(),
              let width = Int(widthText),
              let height = Int(heightText),
              Int(maxText) == 255,
              width > 0,
              height > 0 else {
            return nil
        }
        if index < bytes.count && isSpace(bytes[index]) {
            index += 1
        }
        let expected = width * height * 3
        guard index + expected <= bytes.count else {
            return nil
        }
        return SimulatorFrame(width: width,
                              height: height,
                              rgb: Array(bytes[index..<(index + expected)]))
    }

    @objc private func pollPreviewFrame() {
        guard destinationMode == .simulator, runningSimulatorSketch != nil else {
            return
        }
        let url = previewFrameURL()
        guard let attributes = try? FileManager.default.attributesOfItem(atPath: url.path),
              let modificationDate = attributes[.modificationDate] as? Date,
              let size = attributes[.size] as? NSNumber else {
            return
        }
        let signature = "\(modificationDate.timeIntervalSinceReferenceDate):\(size.int64Value)"
        guard signature != previewFrameSignature else {
            return
        }
        guard let data = try? Data(contentsOf: url),
              let frame = parsePPMFrame(data) else {
            return
        }
        previewFrameSignature = signature
        layoutPreviewView.liveFrame = frame
        layoutPreviewView.physicalWidth = frame.width
        layoutPreviewView.physicalHeight = frame.height
        layoutPreviewView.needsDisplay = true
    }

    private func panelLayoutMetrics(count: Int,
                                    size: (width: Int, height: Int),
                                    rotation: Int,
                                    arrangement: String) -> (sourceWidth: Int, sourceHeight: Int, physicalWidth: Int, physicalHeight: Int) {
        let rotatedWidth = rotation == 90 || rotation == 270 ? size.height : size.width
        let rotatedHeight = rotation == 90 || rotation == 270 ? size.width : size.height
        let sourceWidth = max(1, count) * size.width
        let sourceHeight = size.height

        switch arrangement {
        case "Column":
            return (sourceWidth, sourceHeight, rotatedWidth, max(1, count) * rotatedHeight)
        case "Grid":
            let columns = max(1, Int(ceil(sqrt(Double(max(1, count))))))
            let rows = Int(ceil(Double(max(1, count)) / Double(columns)))
            return (sourceWidth, sourceHeight, columns * rotatedWidth, rows * rotatedHeight)
        default:
            return (sourceWidth, sourceHeight, max(1, count) * rotatedWidth, rotatedHeight)
        }
    }

    private func panelLayoutEntries() -> [PanelLayoutEntry] {
        let count = max(1, Int(selectedPanelCount()) ?? 1)
        let size = parsePanelSize(selectedPanelSize())
        let rotation = selectedRotation()
        let rotatedWidth = rotation == 90 || rotation == 270 ? size.height : size.width
        let rotatedHeight = rotation == 90 || rotation == 270 ? size.width : size.height
        let arrangement = selectedArrangement()
        let columns = arrangement == "Grid" ? max(1, Int(ceil(sqrt(Double(count))))) : count

        return (0..<count).map { index in
            let x: Int
            let y: Int
            switch arrangement {
            case "Column":
                x = 0
                y = index * rotatedHeight
            case "Grid":
                x = (index % columns) * rotatedWidth
                y = (index / columns) * rotatedHeight
            default:
                x = index * rotatedWidth
                y = 0
            }
            return PanelLayoutEntry(sourceX: index * size.width,
                                    sourceY: 0,
                                    width: size.width,
                                    height: size.height,
                                    x: x,
                                    y: y,
                                    rotation: rotation)
        }
    }

    private func panelLayoutHeaderText() -> String {
        let count = max(1, Int(selectedPanelCount()) ?? 1)
        let size = parsePanelSize(selectedPanelSize())
        let rotation = selectedRotation()
        let arrangement = selectedArrangement()
        let metrics = panelLayoutMetrics(count: count, size: size, rotation: rotation, arrangement: arrangement)
        let entries = panelLayoutEntries()
        let rows = entries.map { entry in
            "  {\(entry.sourceX), \(entry.sourceY), \(entry.width), \(entry.height), \(entry.x), \(entry.y), \(entry.rotation)},"
        }.joined(separator: "\n")

        return """
        #pragma once

        #include <stdint.h>

        struct MatrixPortalPanelLayoutEntry {
          int16_t sourceX;
          int16_t sourceY;
          int16_t width;
          int16_t height;
          int16_t x;
          int16_t y;
          uint16_t rotation;
        };

        #define PANEL_LAYOUT_PANEL_COUNT \(count)
        #define PANEL_LAYOUT_PANEL_WIDTH \(size.width)
        #define PANEL_LAYOUT_PANEL_HEIGHT \(size.height)
        #define PANEL_LAYOUT_SOURCE_WIDTH \(metrics.sourceWidth)
        #define PANEL_LAYOUT_SOURCE_HEIGHT \(metrics.sourceHeight)
        #define PANEL_LAYOUT_PHYSICAL_WIDTH \(metrics.physicalWidth)
        #define PANEL_LAYOUT_PHYSICAL_HEIGHT \(metrics.physicalHeight)
        #define PANEL_LAYOUT_IDE_LAYOUT_MODE "\(arrangement)"
        #define PANEL_LAYOUT_IDE_ROTATION \(rotation)

        static const MatrixPortalPanelLayoutEntry PANEL_LAYOUT[] = {
        \(rows)
        };
        """
    }

    private func writePanelLayoutHeader(restartIfNeeded: Bool) {
        guard panelCountPopup != nil else {
            return
        }
        let url = layoutFileURL()
        let text = panelLayoutHeaderText() + "\n"
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                                 withIntermediateDirectories: true)
        do {
            try text.write(to: url, atomically: true, encoding: .utf8)
            layoutFileSnapshot = text
            loadedCustomPanelLayout = false
            layoutPreviewView?.liveFrame = nil
            previewFrameSignature = ""
            if restartIfNeeded {
                restartRunningSimulatorForLayoutChange()
            }
        } catch {
            appendConsoleLine("LAYOUT SAVE FAILED \(url.path): \(error.localizedDescription)")
            setStatus("Could not save panel layout")
        }
    }

    private func defineInt(_ name: String, in text: String) -> Int? {
        let pattern = "#define \(name) "
        guard let line = text.components(separatedBy: .newlines).first(where: { $0.hasPrefix(pattern) }) else {
            return nil
        }
        return Int(line.dropFirst(pattern.count).trimmingCharacters(in: .whitespacesAndNewlines))
    }

    private func defineString(_ name: String, in text: String) -> String? {
        let pattern = "#define \(name) "
        guard let line = text.components(separatedBy: .newlines).first(where: { $0.hasPrefix(pattern) }) else {
            return nil
        }
        return line.dropFirst(pattern.count)
            .trimmingCharacters(in: CharacterSet(charactersIn: "\" "))
    }

    private func loadLayoutHeaderSettings() {
        let url = layoutFileURL()
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            return
        }
        layoutFileSnapshot = text

        if let count = defineInt("PANEL_LAYOUT_PANEL_COUNT", in: text),
           let index = panelCounts.firstIndex(of: "\(count)") {
            pendingPanelCountIndex = index
        }
        if let width = defineInt("PANEL_LAYOUT_PANEL_WIDTH", in: text),
           let height = defineInt("PANEL_LAYOUT_PANEL_HEIGHT", in: text),
           let index = panelSizes.firstIndex(of: "\(width)x\(height)") {
            pendingPanelSizeIndex = index
        }
        if let arrangement = defineString("PANEL_LAYOUT_IDE_LAYOUT_MODE", in: text) {
            if let index = panelArrangements.firstIndex(of: arrangement) {
                pendingArrangementIndex = index
                loadedCustomPanelLayout = false
            } else {
                loadedCustomPanelLayout = true
            }
        }
        if let rotation = defineInt("PANEL_LAYOUT_IDE_ROTATION", in: text),
           let index = panelRotations.firstIndex(of: "\(rotation)") {
            pendingRotationIndex = index
        }
    }

    @objc private func pollLayoutFile() {
        let url = layoutFileURL()
        guard let text = try? String(contentsOf: url, encoding: .utf8), text != layoutFileSnapshot else {
            return
        }
        layoutFileSnapshot = text
        appendConsoleLine("PANEL LAYOUT UPDATED \(layoutPath)")
        restartRunningSimulatorForLayoutChange()
    }

    private func sketchUsesPanelLayout(_ sketch: String) -> Bool {
        let url = workspaceRoot.appendingPathComponent(sketch)
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            return true
        }
        let markers = ["SignDisplay.h", "PanelLayout.h", "PANEL_LAYOUT", "SIGN_WIDTH", "SIGN_HEIGHT", "PANEL_COUNT", "PANEL_WIDTH", "PANEL_HEIGHT"]
        return markers.contains(where: { text.contains($0) })
    }

    private func restartRunningSimulatorForLayoutChange() {
        guard destinationMode == .simulator,
              let sketch = runningSimulatorSketch,
              sketchUsesPanelLayout(sketch) else {
            return
        }

        pendingSimulatorRestartSketch = sketch
        setStatus("Restarting \(sketchName(sketch)) for panel layout")
        appendConsoleLine("PANEL LAYOUT changed; rebuilding \(sketch)")
        if let process = activeProcess {
            process.terminate()
        } else {
            restartPendingSimulatorIfNeeded()
        }
    }

    private func restartPendingSimulatorIfNeeded() {
        guard let sketch = pendingSimulatorRestartSketch else {
            return
        }
        pendingSimulatorRestartSketch = nil
        runningSimulatorSketch = nil
        buildAndRunSimulator(sketch, status: "Updating \(sketchName(sketch))")
    }

    private func updateSelectedSketchLabel() {
        selectedSketchLabel?.stringValue = selectedSketchPath().map { "Selected: \(sketchTitle($0))" } ?? "No sketch selected"
    }

    private func loadSelectedSketchSource() {
        guard codeTextView != nil else {
            return
        }
        guard let sketch = selectedSketchPath() else {
            editorTitleLabel.stringValue = "No Sketch"
            editorPathLabel.stringValue = ""
            codeTextView.string = ""
            return
        }

        editorTitleLabel.stringValue = sketchTitle(sketch)
        editorPathLabel.stringValue = sketch
        let url = workspaceRoot.appendingPathComponent(sketch)
        let source = (try? String(contentsOf: url, encoding: .utf8)) ?? ""
        codeTextView.textStorage?.setAttributedString(colorizedCppSource(source))
        codeTextView.scrollToBeginningOfDocument(nil)
    }

    private func colorizedCppSource(_ source: String) -> NSAttributedString {
        let font = NSFont.monospacedSystemFont(ofSize: 12.5, weight: .regular)
        let fullRange = NSRange(location: 0, length: (source as NSString).length)
        let result = NSMutableAttributedString(string: source, attributes: [
            .font: font,
            .foregroundColor: NSColor.textColor
        ])

        func apply(_ pattern: String, color: NSColor, options: NSRegularExpression.Options = []) {
            guard let regex = try? NSRegularExpression(pattern: pattern, options: options) else {
                return
            }
            regex.enumerateMatches(in: source, options: [], range: fullRange) { match, _, _ in
                guard let match else {
                    return
                }
                result.addAttribute(.foregroundColor, value: color, range: match.range)
            }
        }

        let keywordPattern = "\\b(alignas|alignof|asm|auto|bool|break|case|catch|char|class|const|constexpr|continue|default|delete|do|double|else|enum|explicit|extern|false|float|for|goto|if|inline|int|int16_t|int32_t|int64_t|int8_t|long|namespace|new|nullptr|private|protected|public|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typename|uint16_t|uint32_t|uint64_t|uint8_t|union|unsigned|using|virtual|void|volatile|while)\\b"
        apply(keywordPattern, color: NSColor.systemBlue)
        apply("\\b([0-9]+(\\.[0-9]+)?f?|0x[0-9a-fA-F]+)\\b", color: NSColor.systemOrange)
        apply("^\\s*#.*$", color: NSColor.systemPurple, options: [.anchorsMatchLines])
        apply("\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'", color: NSColor.systemRed)
        apply("//.*$|/\\*[\\s\\S]*?\\*/", color: NSColor.systemGreen, options: [.anchorsMatchLines])
        return result
    }

    private func updateModeUI(openHardwareSerial: Bool = true) {
        guard portPopup != nil else {
            return
        }
        let hardware = destinationMode == .hardware
        portPopup.isEnabled = hardware
        baudPopup.isEnabled = hardware
        for control in sensorControls {
            control.isEnabled = !hardware
        }

        if hardware {
            serialStatusLabel.stringValue = "Hardware"
            if openHardwareSerial {
                ensureSerialOpen()
            }
        } else {
            closeSerial(announce: false)
            serialStatusLabel.stringValue = activeProcess == nil ? "Simulator" : "Simulator process"
        }
    }

    private func refreshSketches() {
        let root = workspaceRoot.appendingPathComponent("Arduino")
        let directories = (try? FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: [.isDirectoryKey])) ?? []
        sketches = directories.compactMap { directory -> String? in
            let values = try? directory.resourceValues(forKeys: [.isDirectoryKey])
            guard values?.isDirectory == true else {
                return nil
            }
            let expected = directory.appendingPathComponent(directory.lastPathComponent).appendingPathExtension("ino")
            if FileManager.default.fileExists(atPath: expected.path) {
                return "Arduino/\(directory.lastPathComponent)/\(expected.lastPathComponent)"
            }
            let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
            return files.first(where: { $0.pathExtension == "ino" }).map { "Arduino/\(directory.lastPathComponent)/\($0.lastPathComponent)" }
        }.sorted()

        sketchTable?.reloadData()
        if !sketches.isEmpty && sketchTable.selectedRow < 0 {
            sketchTable.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
        }
        updateSelectedSketchLabel()
        loadSelectedSketchSource()
    }

    private func refreshPorts(openHardwareSerial: Bool = true) {
        let current = selectedPortPath()
        let devEntries = (try? FileManager.default.contentsOfDirectory(atPath: "/dev")) ?? []
        ports = devEntries
            .filter { $0.hasPrefix("cu.usbmodem") || $0.hasPrefix("cu.usbserial") || $0.hasPrefix("cu.") }
            .map { "/dev/\($0)" }
            .sorted()

        portPopup?.removeAllItems()
        if ports.isEmpty {
            portPopup?.addItem(withTitle: "No USB device")
        } else {
            portPopup?.addItems(withTitles: ports)
            if let current, let index = ports.firstIndex(of: current) {
                portPopup?.selectItem(at: index)
            }
        }
        closeSerial(announce: false)
        if destinationMode == .hardware && openHardwareSerial {
            ensureSerialOpen()
        }
    }

    private func setStatus(_ message: String) {
        statusLabel.stringValue = message
    }

    private func appendConsole(_ text: String) {
        guard let storage = serialOutput?.textStorage else {
            return
        }
        storage.append(NSAttributedString(string: text))
        let overflow = storage.length - 200_000
        if overflow > 0 {
            storage.deleteCharacters(in: NSRange(location: 0, length: overflow))
        }
        serialOutput?.scrollToEndOfDocument(nil)
    }

    private func appendConsoleLine(_ line: String) {
        appendConsole(line + "\n")
    }

    private func runMake(_ arguments: [String],
                         status: String,
                         connectInput: Bool = false,
                         completion: ((Int32) -> Void)? = nil) {
        guard activeProcess == nil else {
            setStatus("A command is already running")
            return
        }

        setStatus(status)
        appendConsoleLine("$ make \(arguments.joined(separator: " "))")

        let process = Process()
        process.currentDirectoryURL = workspaceRoot
        process.executableURL = URL(fileURLWithPath: "/usr/bin/make")
        process.arguments = ["--no-print-directory"] + arguments

        let outputPipe = Pipe()
        let errorPipe = Pipe()
        let inputPipe = Pipe()
        process.standardOutput = outputPipe
        process.standardError = errorPipe
        process.standardInput = inputPipe

        let appendData: (FileHandle) -> Void = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty, let text = String(data: data, encoding: .utf8) else {
                return
            }
            DispatchQueue.main.async {
                self?.appendConsole(text)
            }
        }
        outputPipe.fileHandleForReading.readabilityHandler = appendData
        errorPipe.fileHandleForReading.readabilityHandler = appendData

        process.terminationHandler = { [weak self] proc in
            outputPipe.fileHandleForReading.readabilityHandler = nil
            errorPipe.fileHandleForReading.readabilityHandler = nil
            DispatchQueue.main.async {
                guard let self else {
                    return
                }
                self.activeProcess = nil
                self.activeInputPipe = nil
                self.setStatus(proc.terminationStatus == 0 ? "\(status) finished" : "\(status) failed")
                if self.destinationMode == .simulator {
                    self.serialStatusLabel.stringValue = "Simulator"
                }
                completion?(proc.terminationStatus)
            }
        }

        do {
            try process.run()
            activeProcess = process
            activeInputPipe = connectInput ? inputPipe : nil
            if connectInput {
                serialStatusLabel.stringValue = "Simulator process"
            }
        } catch {
            setStatus("Could not start command: \(error.localizedDescription)")
            appendConsoleLine("Could not start command: \(error.localizedDescription)")
        }
    }

    private func buildAndRunSimulator(_ sketch: String, status: String? = nil) {
        if !loadedCustomPanelLayout {
            writePanelLayoutHeader(restartIfNeeded: false)
        }
        writeSimulatorControlState()
        let arguments = ["_sim-build", "SKETCH=\(sketch)"] + geometryAssignments()
        runMake(arguments, status: status ?? "Building \(sketchName(sketch))", completion: { [weak self] buildStatus in
            if buildStatus == 0 {
                self?.startPreviewProcess(for: sketch)
            }
        })
    }

    private func previewBinaryPath(for sketch: String) -> String {
        return "build/sim/\(sketchName(sketch))-sim"
    }

    private func startPreviewProcess(for sketch: String) {
        guard activeProcess == nil else {
            setStatus("A command is already running")
            return
        }

        let executable = previewBinaryPath(for: sketch)
        try? FileManager.default.removeItem(at: previewFrameURL())
        previewFrameSignature = ""
        layoutPreviewView.liveFrame = nil
        layoutPreviewView.needsDisplay = true
        let arguments = [
            "--scale", "\(scale)",
            "--max-frames", "0",
            "--panel", selectedPanelSize(),
            "--layout", layoutPath,
            "--control", simControlPath,
            "--frame-file", previewFramePath,
            "--no-window"
        ]
        appendConsoleLine("$ \(executable) \(arguments.joined(separator: " "))")
        setStatus("Running \(sketchName(sketch))")

        let process = Process()
        process.currentDirectoryURL = workspaceRoot
        process.executableURL = workspaceRoot.appendingPathComponent(executable)
        process.arguments = arguments

        let outputPipe = Pipe()
        let errorPipe = Pipe()
        let inputPipe = Pipe()
        process.standardOutput = outputPipe
        process.standardError = errorPipe
        process.standardInput = inputPipe

        let appendData: (FileHandle) -> Void = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty, let text = String(data: data, encoding: .utf8) else {
                return
            }
            DispatchQueue.main.async {
                self?.appendConsole(text)
            }
        }
        outputPipe.fileHandleForReading.readabilityHandler = appendData
        errorPipe.fileHandleForReading.readabilityHandler = appendData

        process.terminationHandler = { [weak self] proc in
            outputPipe.fileHandleForReading.readabilityHandler = nil
            errorPipe.fileHandleForReading.readabilityHandler = nil
            DispatchQueue.main.async {
                guard let self else {
                    return
                }
                self.activeProcess = nil
                self.activeInputPipe = nil
                self.serialStatusLabel.stringValue = "Simulator"
                if self.pendingSimulatorRestartSketch != nil {
                    self.restartPendingSimulatorIfNeeded()
                } else {
                    self.runningSimulatorSketch = nil
                    self.previewFrameSignature = ""
                    self.updateLayoutPreview()
                    self.setStatus(proc.terminationStatus == 0 ? "\(sketchName(sketch)) stopped" : "\(sketchName(sketch)) failed")
                }
            }
        }

        do {
            try process.run()
            activeProcess = process
            activeInputPipe = inputPipe
            runningSimulatorSketch = sketch
            serialStatusLabel.stringValue = "Simulator process"
        } catch {
            setStatus("Could not start preview: \(error.localizedDescription)")
            appendConsoleLine("Could not start preview: \(error.localizedDescription)")
        }
    }

    private func configureSerial(fd: Int32, baud: String) -> Bool {
        var options = termios()
        if tcgetattr(fd, &options) != 0 {
            return false
        }
        cfmakeraw(&options)
        let speed = baudConstant(baud)
        cfsetispeed(&options, speed)
        cfsetospeed(&options, speed)
        options.c_cflag &= ~tcflag_t(CSIZE)
        options.c_cflag |= tcflag_t(CS8 | CLOCAL | CREAD)
        options.c_cflag &= ~tcflag_t(PARENB | CSTOPB)
        options.c_cflag &= ~tcflag_t(CRTSCTS)
        options.c_iflag &= ~tcflag_t(IXON | IXOFF | IXANY)
        return tcsetattr(fd, TCSANOW, &options) == 0
    }

    private func ensureSerialOpen() {
        guard destinationMode == .hardware else {
            return
        }
        guard let port = selectedPortPath() else {
            serialStatusLabel.stringValue = "No USB device"
            return
        }
        let baud = selectedBaudRate()
        if serialFd >= 0 && serialConnectedPort == port && serialConnectedBaud == baud {
            return
        }

        closeSerial(announce: true)
        let fd = Darwin.open(port, O_RDWR | O_NOCTTY | O_NONBLOCK)
        if fd < 0 {
            appendConsoleLine("OPEN FAILED \(port): \(String(cString: strerror(errno)))")
            serialStatusLabel.stringValue = "Disconnected"
            return
        }
        if !configureSerial(fd: fd, baud: baud) {
            appendConsoleLine("CONFIG FAILED \(port): \(String(cString: strerror(errno)))")
            Darwin.close(fd)
            serialStatusLabel.stringValue = "Disconnected"
            return
        }
        tcflush(fd, TCIOFLUSH)
        serialFd = fd
        serialConnectedPort = port
        serialConnectedBaud = baud
        serialStatusLabel.stringValue = "Connected \(baud)"
        appendConsoleLine("CONNECTED \(port) @ \(baud)")
    }

    private func closeSerial(announce: Bool) {
        if serialFd >= 0 {
            Darwin.close(serialFd)
            serialFd = -1
            if announce && !serialConnectedPort.isEmpty {
                appendConsoleLine("DISCONNECTED \(serialConnectedPort)")
            }
        }
        serialConnectedPort = ""
        serialConnectedBaud = ""
        serialRxBuffer = ""
        serialStatusLabel?.stringValue = destinationMode == .hardware ? "Disconnected" : "Simulator"
    }

    @objc private func pollSerial() {
        guard destinationMode == .hardware else {
            return
        }
        ensureSerialOpen()
        guard serialFd >= 0 else {
            return
        }

        var buffer = [UInt8](repeating: 0, count: 512)
        while true {
            let count = buffer.withUnsafeMutableBytes { rawBuffer in
                Darwin.read(serialFd, rawBuffer.baseAddress, rawBuffer.count)
            }
            if count > 0 {
                appendSerialBytes(Array(buffer.prefix(count)))
            } else if count == 0 {
                break
            } else if errno == EAGAIN || errno == EWOULDBLOCK {
                break
            } else {
                appendConsoleLine("READ FAILED: \(String(cString: strerror(errno)))")
                closeSerial(announce: true)
                break
            }
        }
    }

    private func appendSerialBytes(_ bytes: [UInt8]) {
        for byte in bytes {
            if byte == 13 {
                continue
            }
            if byte == 10 {
                appendConsoleLine("RX \(serialRxBuffer)")
                serialRxBuffer = ""
                continue
            }
            if byte >= 32 && byte < 127 {
                serialRxBuffer.append(Character(UnicodeScalar(byte)))
            } else {
                serialRxBuffer.append(".")
            }
            if serialRxBuffer.count >= 120 {
                appendConsoleLine("RX \(serialRxBuffer)")
                serialRxBuffer = ""
            }
        }
    }

    private func writeSimulatorControlState() {
        guard simAutoTiltButton != nil else {
            return
        }
        let url = simControlPath.hasPrefix("/")
            ? URL(fileURLWithPath: simControlPath)
            : workspaceRoot.appendingPathComponent(simControlPath)
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                                 withIntermediateDirectories: true)
        let text = [
            "auto_tilt=\(simAutoTiltButton.state == .on ? 1 : 0)",
            "button_up=\(simUpButton.state == .on ? 1 : 0)",
            "button_down=\(simDownButton.state == .on ? 1 : 0)",
            "accel_x=\(String(format: "%.3f", accelXSlider.doubleValue))",
            "accel_y=\(String(format: "%.3f", accelYSlider.doubleValue))",
            "analog0=\(Int(analogA0Slider.doubleValue.rounded()))"
        ].joined(separator: "\n") + "\n"
        try? text.write(to: url, atomically: true, encoding: .utf8)
    }

    private func makeToolbarItem(_ identifier: NSToolbarItem.Identifier,
                                 label: String,
                                 symbol: String,
                                 action: Selector) -> NSToolbarItem {
        let item = NSToolbarItem(itemIdentifier: identifier)
        item.label = label
        item.paletteLabel = label
        item.toolTip = label
        item.target = self
        item.action = action
        item.image = NSImage(systemSymbolName: symbol, accessibilityDescription: label)
        return item
    }

    func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [
            loadToolbarItem, verifyToolbarItem, uploadToolbarItem, stopToolbarItem,
            portsToolbarItem, openToolbarItem, .flexibleSpace
        ]
    }

    func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
        return [
            loadToolbarItem, verifyToolbarItem, uploadToolbarItem,
            .flexibleSpace,
            stopToolbarItem, portsToolbarItem, openToolbarItem
        ]
    }

    func toolbar(_ toolbar: NSToolbar,
                 itemForItemIdentifier itemIdentifier: NSToolbarItem.Identifier,
                 willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
        switch itemIdentifier {
        case loadToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Run", symbol: "play.fill", action: #selector(loadSelectedSketch))
        case verifyToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Verify", symbol: "checkmark.circle", action: #selector(verifySelectedSketch))
        case uploadToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Upload", symbol: "arrow.up.circle", action: #selector(uploadSelectedSketch))
        case stopToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Stop", symbol: "stop.fill", action: #selector(stopActiveProcess))
        case portsToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Ports", symbol: "cable.connector", action: #selector(refreshPortsAction))
        case openToolbarItem:
            return makeToolbarItem(itemIdentifier, label: "Open", symbol: "doc.text", action: #selector(openSelectedSketch))
        default:
            return nil
        }
    }

    func validateToolbarItem(_ item: NSToolbarItem) -> Bool {
        if item.action == #selector(stopActiveProcess) {
            return activeProcess != nil
        }
        if item.action == #selector(uploadSelectedSketch) {
            return activeProcess == nil && selectedSketchPath() != nil
        }
        return activeProcess == nil || item.action == #selector(refreshPortsAction)
    }

    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(stopActiveProcess) {
            return activeProcess != nil
        }
        if menuItem.action == #selector(loadSelectedSketch) ||
            menuItem.action == #selector(verifySelectedSketch) ||
            menuItem.action == #selector(uploadSelectedSketch) {
            return activeProcess == nil && selectedSketchPath() != nil
        }
        return true
    }

    @objc private func showAbout() {
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName: "Matrix Portal IDE",
            .applicationVersion: "Local Build"
        ])
    }

    @objc private func clearConsole() {
        serialOutput.string = ""
    }

    @objc private func modeChanged() {
        destinationMode = modeControl.selectedSegment == 0 ? .simulator : .hardware
        updateModeUI()
        setStatus(destinationMode == .simulator ? "Simulator mode" : "Hardware mode")
    }

    @objc private func geometryChanged() {
        updateGeometryLabel()
        writePanelLayoutHeader(restartIfNeeded: true)
    }

    @objc private func simControlChanged(_ sender: Any?) {
        if let slider = sender as? NSSlider,
           slider === accelXSlider || slider === accelYSlider {
            simAutoTiltButton.state = .off
        }
        writeSimulatorControlState()
    }

    @objc private func portChanged() {
        closeSerial(announce: false)
        if destinationMode == .hardware {
            ensureSerialOpen()
        }
    }

    @objc private func baudChanged() {
        closeSerial(announce: false)
        if destinationMode == .hardware {
            ensureSerialOpen()
        }
    }

    @objc private func refreshPortsAction() {
        refreshPorts()
        setStatus("Refreshed serial ports")
    }

    @objc private func loadSelectedSketch() {
        guard let sketch = selectedSketchPath() else {
            setStatus("No sketch selected")
            return
        }
        if destinationMode == .hardware {
            uploadSelectedSketch()
            return
        }
        buildAndRunSimulator(sketch)
    }

    @objc private func verifySelectedSketch() {
        guard let sketch = selectedSketchPath() else {
            setStatus("No sketch selected")
            return
        }
        if !loadedCustomPanelLayout {
            writePanelLayoutHeader(restartIfNeeded: false)
        }
        let arguments = ["build", "SKETCH=\(sketch)"] + geometryAssignments()
        runMake(arguments, status: "Verifying \(sketchName(sketch))")
    }

    @objc private func uploadSelectedSketch() {
        guard let sketch = selectedSketchPath() else {
            setStatus("No sketch selected")
            return
        }
        destinationMode = .hardware
        modeControl.selectedSegment = 1
        updateModeUI(openHardwareSerial: false)
        refreshPorts(openHardwareSerial: false)
        guard let port = selectedPortPath() else {
            setStatus("No USB device selected")
            return
        }
        closeSerial(announce: false)
        if !loadedCustomPanelLayout {
            writePanelLayoutHeader(restartIfNeeded: false)
        }
        let arguments = ["upload", "SKETCH=\(sketch)", "PORT=\(port)"] + geometryAssignments()
        runMake(arguments, status: "Uploading \(sketchName(sketch))", completion: { [weak self] status in
            if status == 0 {
                self?.ensureSerialOpen()
            }
        })
    }

    @objc private func openSelectedSketch() {
        guard let sketch = selectedSketchPath() else {
            setStatus("No sketch selected")
            return
        }
        NSWorkspace.shared.open(workspaceRoot.appendingPathComponent(sketch))
        setStatus("Opened \(sketchName(sketch))")
    }

    @objc private func stopActiveProcess() {
        guard let process = activeProcess else {
            return
        }
        process.terminate()
        setStatus("Stopping")
    }

    @objc private func sendSerialInput() {
        let line = serialInput.stringValue
        guard !line.isEmpty else {
            return
        }
        serialInput.stringValue = ""
        appendConsoleLine("TX \(line)")

        if destinationMode == .simulator {
            guard let input = activeInputPipe else {
                appendConsoleLine("TX not sent: no simulator process")
                return
            }
            input.fileHandleForWriting.write(Data((line + "\n").utf8))
            return
        }

        ensureSerialOpen()
        guard serialFd >= 0 else {
            appendConsoleLine("TX failed: serial not connected")
            return
        }

        let payload = Array((line + "\n").utf8)
        payload.withUnsafeBytes { rawBuffer in
            _ = Darwin.write(serialFd, rawBuffer.baseAddress, rawBuffer.count)
        }
    }
}

private let app = NSApplication.shared
private let delegate = AppDelegate()
app.setActivationPolicy(.regular)
app.delegate = delegate
app.run()

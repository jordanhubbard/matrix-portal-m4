import AppKit
import Darwin
import Foundation

private let panelCounts = ["1", "2", "3", "4"]
private let panelSizes = ["32x32", "64x32", "64x64", "32x16"]
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

private func sketchName(_ path: String) -> String {
    let url = URL(fileURLWithPath: path)
    return url.deletingPathExtension().lastPathComponent
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
    private var geometryLabel: NSTextField!
    private var portPopup: NSPopUpButton!
    private var baudPopup: NSPopUpButton!
    private var serialOutput: NSTextView!
    private var serialInput: NSTextField!
    private var serialStatusLabel: NSTextField!
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
    private var activeProcess: Process?
    private var activeInputPipe: Pipe?
    private var scale = 8
    private var layoutPath = "sim-panel-layout.txt"
    private var simControlPath = "build/sim/sim-control.env"

    func applicationDidFinishLaunching(_ notification: Notification) {
        parseArguments()
        buildMenu()
        buildWindow()
        refreshSketches()
        refreshPorts(openHardwareSerial: false)
        updateModeUI()
        updateGeometryLabel()
        writeSimulatorControlState()
        appendConsoleLine("Simulator serial ready")
        serialTimer = Timer.scheduledTimer(timeInterval: 0.05,
                                           target: self,
                                           selector: #selector(pollSerial),
                                           userInfo: nil,
                                           repeats: true)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationWillTerminate(_ notification: Notification) {
        activeProcess?.terminate()
        serialTimer?.invalidate()
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
            default:
                break
            }
        }
    }

    private var pendingPanelCountIndex = 3
    private var pendingPanelSizeIndex = 0

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
        runMenu.addItem(withTitle: "Load", action: #selector(loadSelectedSketch), keyEquivalent: "r")
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
            contentRect: NSRect(x: 0, y: 0, width: 1180, height: 780),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Matrix Portal M4 IDE"
        window.center()
        window.delegate = self
        window.toolbar = buildToolbar()

        let contentView = NSView()
        window.contentView = contentView

        let root = NSStackView()
        root.orientation = .vertical
        root.spacing = 0
        root.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(root)

        let split = NSSplitView()
        split.isVertical = true
        split.dividerStyle = .thin
        split.translatesAutoresizingMaskIntoConstraints = false
        root.addArrangedSubview(split)

        let browser = buildSketchBrowser()
        let inspector = buildInspector()
        split.addArrangedSubview(browser)
        split.addArrangedSubview(inspector)
        browser.widthAnchor.constraint(greaterThanOrEqualToConstant: 560).isActive = true
        inspector.widthAnchor.constraint(greaterThanOrEqualToConstant: 420).isActive = true

        let statusBar = buildStatusBar()
        root.addArrangedSubview(statusBar)
        statusBar.heightAnchor.constraint(equalToConstant: 28).isActive = true

        NSLayoutConstraint.activate([
            root.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            root.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
            root.topAnchor.constraint(equalTo: contentView.topAnchor),
            root.bottomAnchor.constraint(equalTo: contentView.bottomAnchor)
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
        let container = NSStackView()
        container.orientation = .vertical
        container.spacing = 8
        container.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)

        let header = NSTextField(labelWithString: "Sketches")
        header.font = NSFont.boldSystemFont(ofSize: 13)
        container.addArrangedSubview(header)

        sketchTable = NSTableView()
        sketchTable.headerView = nil
        sketchTable.usesAlternatingRowBackgroundColors = false
        sketchTable.style = .sourceList
        sketchTable.allowsEmptySelection = false
        sketchTable.rowHeight = 26
        sketchTable.delegate = self
        sketchTable.dataSource = self
        sketchTable.target = self
        sketchTable.doubleAction = #selector(loadSelectedSketch)

        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("sketch"))
        column.title = "Sketch"
        column.width = 560
        sketchTable.addTableColumn(column)

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.documentView = sketchTable
        scroll.borderType = .noBorder
        container.addArrangedSubview(scroll)
        scroll.heightAnchor.constraint(greaterThanOrEqualToConstant: 520).isActive = true

        return container
    }

    private func buildInspector() -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
        stack.spacing = 14
        stack.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)

        stack.addArrangedSubview(section("Destination", buildDestinationSection()))
        stack.addArrangedSubview(section("Display Geometry", buildGeometrySection()))
        stack.addArrangedSubview(section("Serial I/O", buildSerialSection()))
        stack.addArrangedSubview(section("Simulated Device I/O", buildSensorSection()))

        return stack
    }

    private func section(_ title: String, _ view: NSView) -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
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

        geometryLabel = NSTextField(labelWithString: "")
        geometryLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(labeledRow("Matrix", geometryLabel))
        return stack
    }

    private func buildSerialSection() -> NSView {
        let stack = NSStackView()
        stack.orientation = .vertical
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
        scroll.heightAnchor.constraint(equalToConstant: 220).isActive = true

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
        let cell = tableView.makeView(withIdentifier: identifier, owner: self) as? NSTableCellView ?? NSTableCellView()
        cell.identifier = identifier

        let textField: NSTextField
        if let existing = cell.textField {
            textField = existing
        } else {
            textField = NSTextField(labelWithString: "")
            textField.translatesAutoresizingMaskIntoConstraints = false
            cell.addSubview(textField)
            cell.textField = textField
            NSLayoutConstraint.activate([
                textField.leadingAnchor.constraint(equalTo: cell.leadingAnchor, constant: 8),
                textField.trailingAnchor.constraint(equalTo: cell.trailingAnchor, constant: -8),
                textField.centerYAnchor.constraint(equalTo: cell.centerYAnchor)
            ])
        }

        let path = sketches[row]
        textField.stringValue = "\(sketchName(path))    \(path)"
        textField.lineBreakMode = .byTruncatingMiddle
        return cell
    }

    func tableViewSelectionDidChange(_ notification: Notification) {
        updateSelectedSketchLabel()
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
            "SIM_PANEL=\(selectedPanelSize())",
            "SIM_CONTROL=\(simControlPath)"
        ]
    }

    private func updateGeometryLabel() {
        guard geometryLabel != nil else {
            return
        }
        let count = Int(selectedPanelCount()) ?? 1
        let size = parsePanelSize(selectedPanelSize())
        geometryLabel.stringValue = "\(count * size.width)x\(size.height)"
    }

    private func updateSelectedSketchLabel() {
        selectedSketchLabel?.stringValue = selectedSketchPath().map { "Selected: \(sketchName($0))" } ?? "No sketch selected"
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
        let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("Arduino")
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
        process.currentDirectoryURL = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
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

    private func previewBinaryPath(for sketch: String) -> String {
        return "build/sim/\(sketchName(sketch))-sim"
    }

    private func startPreviewProcess(for sketch: String) {
        guard activeProcess == nil else {
            setStatus("A command is already running")
            return
        }

        let executable = previewBinaryPath(for: sketch)
        let arguments = [
            "--scale", "\(scale)",
            "--max-frames", "0",
            "--panel", selectedPanelSize(),
            "--layout", layoutPath,
            "--control", simControlPath
        ]
        appendConsoleLine("$ \(executable) \(arguments.joined(separator: " "))")
        setStatus("Running \(sketchName(sketch))")

        let process = Process()
        let workingDirectory = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        process.currentDirectoryURL = workingDirectory
        process.executableURL = workingDirectory.appendingPathComponent(executable)
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
                self?.activeProcess = nil
                self?.activeInputPipe = nil
                self?.serialStatusLabel.stringValue = "Simulator"
                self?.setStatus(proc.terminationStatus == 0 ? "\(sketchName(sketch)) stopped" : "\(sketchName(sketch)) failed")
            }
        }

        do {
            try process.run()
            activeProcess = process
            activeInputPipe = inputPipe
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
        let url = URL(fileURLWithPath: simControlPath)
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
            return makeToolbarItem(itemIdentifier, label: "Load", symbol: "play.fill", action: #selector(loadSelectedSketch))
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
        writeSimulatorControlState()
        let arguments = ["_sim-build", "SKETCH=\(sketch)"] + geometryAssignments()
        runMake(arguments, status: "Building \(sketchName(sketch))", completion: { [weak self] status in
            if status == 0 {
                self?.startPreviewProcess(for: sketch)
            }
        })
    }

    @objc private func verifySelectedSketch() {
        guard let sketch = selectedSketchPath() else {
            setStatus("No sketch selected")
            return
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
        NSWorkspace.shared.open(URL(fileURLWithPath: sketch))
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

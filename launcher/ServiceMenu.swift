import SwiftUI
import AppKit

struct Settings: Codable, Equatable {
    var width = 1280
    var scale = 3
    var fullscreen = 0
    var filtering = "nearest"
    var antialiasing = true
    var vsync = "on"
    var lowLatency = true
    var nativeScene = false
    var nativeFps = 60
    var nativeAspect = "4:3"
    var nativeWidth = 1280
    var nativeHeight = 960
    var perspective = true
    var frameGraph = false
    var fullScene = true
    var nativeCarDistance = 0
    var rewind = false
}

@MainActor final class ServiceModel: ObservableObject {
    @Published var settings = Settings()
    @Published var running = false
    @Published var message = ""
    @Published var error = ""
    let root: URL
    private var process: Process?
    init() {
        // The runtime stages game.toml beside the app. It is not sufficient
        // to identify the source project; require the actual bridge as well.
        func isProject(_ location: URL) -> Bool {
            ["game.toml", "launcher/settings.py", "CMakeLists.txt"].allSatisfy {
                FileManager.default.fileExists(atPath: location.appendingPathComponent($0).path)
            }
        }
        var location = (Bundle.main.executableURL ?? URL(fileURLWithPath: CommandLine.arguments[0]))
            .resolvingSymlinksInPath().deletingLastPathComponent()
        if let override = ProcessInfo.processInfo.environment["REVOLUTION_PROJECT_ROOT"] {
            location = URL(fileURLWithPath: override).standardizedFileURL
        } else {
            while !isProject(location) && location.path != "/" {
                location.deleteLastPathComponent()
            }
        }
        root = location
        guard isProject(location) else {
            error = "Cannot find the Ridge Racer Revolution project. Keep this app in its build-macos folder, or set REVOLUTION_PROJECT_ROOT to the folder containing launcher/settings.py and game.toml."
            return
        }
        reload()
    }
    func helper(_ action: String) -> Process {
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/opt/homebrew/bin/python3.13")
        task.arguments = [root.appendingPathComponent("launcher/settings.py").path, action, "--root", root.path]
        task.currentDirectoryURL = root
        return task
    }
    func execute(_ action: String, input: Data? = nil) throws -> Data {
        let task = helper(action)
        let output = Pipe(), errors = Pipe(), stdin = Pipe()
        task.standardOutput = output; task.standardError = errors; task.standardInput = stdin
        try task.run()
        if let input { stdin.fileHandleForWriting.write(input) }
        try? stdin.fileHandleForWriting.close()
        let data = output.fileHandleForReading.readDataToEndOfFile()
        let failure = errors.fileHandleForReading.readDataToEndOfFile()
        task.waitUntilExit()
        guard task.terminationStatus == 0 else {
            throw NSError(domain: "ServiceMenu", code: Int(task.terminationStatus), userInfo: [NSLocalizedDescriptionKey: String(decoding: failure, as: UTF8.self)])
        }
        return data
    }
    func reload() {
        do { settings = try JSONDecoder().decode(Settings.self, from: execute("read")) }
        catch { self.error = error.localizedDescription }
    }
    @discardableResult func save() -> Bool {
        do {
            _ = try execute("save", input: JSONEncoder().encode(settings))
            message = "Settings saved for your next launch."
            return true
        } catch { self.error = error.localizedDescription; return false }
    }
    func launch(advanced: Bool = false) {
        guard save() else { return }
        let task = helper(advanced ? "advanced" : "launch")
        let errors = Pipe()
        task.standardError = errors
        task.standardOutput = FileHandle.nullDevice
        task.terminationHandler = { [weak self] task in
            let text = String(decoding: errors.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self)
            Task { @MainActor in
                guard let self else { return }
                self.running = false
                self.process = nil
                if task.terminationStatus != 0 { self.error = text }
                self.reload()
                self.message = "Game closed. Settings are ready for the next launch."
                NSApp.activate(ignoringOtherApps: true)
            }
        }
        do {
            try task.run(); process = task; running = true
            message = "Game running. Close its window to return to the service menu."
        } catch { self.error = error.localizedDescription }
    }
}

struct ServiceView: View {
    @StateObject private var model = ServiceModel()
    @State private var tab = "Display"
    @State private var nativeWidthInput = ""
    private let accent = Color(red: 0.98, green: 0.35, blue: 0.17)
    func alignNativeResolution() {
        guard model.settings.nativeScene else { return }
        let a = model.settings.nativeAspect == "4:3" ? 4 : 16
        let b = model.settings.nativeAspect == "4:3" ? 3 : 9
        let units = max((240+b-1)/b, min(min(7680/a,4320/b),Int((Double(model.settings.nativeHeight)/Double(b)).rounded())))
        model.settings.nativeWidth = units*a; model.settings.nativeHeight = units*b
    }
    func applyNativeWidth() {
        guard let value = Int(nativeWidthInput) else {
            model.error = "Enter a whole-number rendering width."; return
        }
        let a = model.settings.nativeAspect == "4:3" ? 4 : 16
        let b = model.settings.nativeAspect == "4:3" ? 3 : 9
        let units = max((240+b-1)/b,min(min(7680/a,4320/b),Int((Double(value)/Double(a)).rounded())))
        model.settings.nativeWidth = units*a; model.settings.nativeHeight = units*b
        nativeWidthInput = String(model.settings.nativeWidth)
    }
    var nativeResolution: some View {
        let a = model.settings.nativeAspect == "4:3" ? 4 : 16
        let b = model.settings.nativeAspect == "4:3" ? 3 : 9
        let presets = a == 4 ? [640,800,960,1024,1280,1440,1600,1920,2560,2880,3840,5760] : [640,960,1280,1600,1920,2560,3200,3840,5120,7680]
        let widths = Array(Set(presets + [model.settings.nativeWidth])).sorted()
        let selection = Binding<Int>(get: { model.settings.nativeWidth }, set: { value in
            model.settings.nativeWidth = value; model.settings.nativeHeight = value/a*b
        })
        return VStack(alignment: .leading, spacing: 14) {
            Picker("Render resolution", selection: selection) {
                ForEach(widths, id: \.self) { width in Text("\(width) × \(width/a*b)").tag(width) }
            }
            HStack {
                Text("Custom width")
                Spacer()
                TextField("Pixels", text: $nativeWidthInput)
                    .labelsHidden().accessibilityLabel("Custom rendering width in pixels")
                    .frame(width: 100).textFieldStyle(.roundedBorder).onSubmit { applyNativeWidth() }
                Button("Apply") { applyNativeWidth() }
                Text("× \(model.settings.nativeHeight)")
            }
            note("Only \(model.settings.nativeAspect) resolutions are available. Apply a custom width to calculate a matching height. Internal rendering resolution; display scaling is separate.")
        }.onAppear { nativeWidthInput = String(model.settings.nativeWidth) }
            .onChange(of: model.settings.nativeWidth) { value in nativeWidthInput = String(value) }
    }
    var body: some View {
        VStack(spacing: 0) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 5) {
                    Text("RIDGE RACER").font(.system(size: 34, weight: .black, design: .rounded)).italic()
                    Text("REVOLUTION").font(.system(size: 22, weight: .black, design: .rounded)).foregroundStyle(accent)
                    Text("SERVICE MENU   /   USA · SLUS-00214").font(.system(size: 11, weight: .semibold, design: .monospaced)).tracking(1.7).foregroundStyle(.secondary)
                }
                Spacer()
                Label("APPLE SILICON", systemImage: "desktopcomputer").font(.system(size: 10, weight: .bold)).padding(9).background(.white.opacity(0.07), in: Capsule())
            }.padding(28)
            Rectangle().fill(accent).frame(height: 3)
            HStack(spacing: 0) {
                VStack(alignment: .leading, spacing: 9) {
                    ForEach(["Display", "Image", "Motion", "Controls"], id: \.self) { name in
                        Button { tab = name } label: {
                            HStack { Image(systemName: icon(name)).frame(width: 20); Text(name); Spacer() }
                                .padding(12).background(tab == name ? accent.opacity(0.22) : .clear, in: RoundedRectangle(cornerRadius: 8))
                        }.buttonStyle(.plain).foregroundStyle(tab == name ? .white : .secondary)
                    }
                    Spacer()
                    Text("ORIGINAL TIMING\n59.94 Hz guest clock").font(.system(size: 10, design: .monospaced)).foregroundStyle(.secondary).lineSpacing(4)
                    Text("Presentation settings never overclock the game.").font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                }.padding(18).frame(width: 190).background(.black.opacity(0.16))
                ScrollView {
                    VStack(alignment: .leading, spacing: 20) {
                        Text(tab).font(.title2.bold())
                        Group {
                            switch tab {
                            case "Display": display
                            case "Image": image
                            case "Motion": motion
                            default: controls
                            }
                        }.disabled(model.running)
                    }.padding(26).frame(maxWidth: .infinity, alignment: .leading)
                }
            }
            Divider()
            HStack {
                VStack(alignment: .leading, spacing: 3) {
                    Text(model.running ? "ON TRACK" : "READY TO RACE").font(.system(size: 10, weight: .bold, design: .monospaced)).foregroundStyle(accent)
                    Text(model.message.isEmpty ? "Settings and memory cards are private to Revolution." : model.message).font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
                Button("Save") { model.save() }.disabled(model.running)
                Button(model.running ? "Running…" : "Launch game  →") { model.launch() }
                    .buttonStyle(.borderedProminent).tint(accent).disabled(model.running).keyboardShortcut(.defaultAction)
            }.padding(22)
        }.onChange(of: model.settings.nativeAspect) { _ in alignNativeResolution() }
            .onChange(of: model.settings.nativeScene) { _ in alignNativeResolution() }
            .frame(minWidth: 820, idealWidth: 860, minHeight: 650, idealHeight: 690)
            .background(Color(red: 0.075, green: 0.085, blue: 0.105)).preferredColorScheme(.dark)
            .alert("Unable to complete action", isPresented: Binding(get: { !model.error.isEmpty }, set: { if !$0 { model.error = "" } })) {
                Button("OK") { model.error = "" }
            } message: { Text(model.error) }
    }
    func icon(_ s: String) -> String {
        ["Display":"display", "Image":"slider.horizontal.3", "Motion":"speedometer", "Controls":"gamecontroller"][s]!
    }
    func note(_ text: String) -> some View { Text(text).font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true) }
    var display: some View {
        VStack(alignment: .leading, spacing: 18) {
            if model.settings.nativeScene {
                Picker("Aspect ratio", selection: $model.settings.nativeAspect) {
                    Text("Original · 4:3").tag("4:3")
                    Text("Widescreen · 16:9").tag("16:9")
                }
                nativeResolution
                Toggle("Full course visibility", isOn: $model.settings.fullScene)
                Picker("CPU car draw distance", selection: $model.settings.nativeCarDistance) {
                    Text("All cars · unlimited").tag(0)
                    Text("1× · original").tag(1)
                    ForEach(2...5, id: \.self) { Text("\($0)×").tag($0) }
                }
            } else {
                Picker("Render resolution", selection: $model.settings.scale) {
                    Text("320 × 240 — original").tag(1)
                    Text("640 × 480 — 2×").tag(2)
                    Text("960 × 720 — 3×").tag(3)
                    Text("1280 × 960 — 4×").tag(4)
                }
                Picker("Window size", selection: $model.settings.width) {
                    ForEach([640,960,1280,1600,1920,2560], id: \.self) { w in Text("\(w) × \(w*3/4)").tag(w) }
                }
            }
            Picker("Display mode", selection: $model.settings.fullscreen) {
                Text("Windowed").tag(0); Text("Borderless fullscreen").tag(1); Text("Fullscreen").tag(2)
            }
            note("Choose Original or Enhanced rendering in Motion. Changes apply when you launch the game.")
        }
    }
    var image: some View {
        VStack(alignment: .leading, spacing: 18) {
            Picker("Texture filtering", selection: $model.settings.filtering) {
                Text("Sharp / nearest").tag("nearest"); Text("Smooth / bilinear").tag("bilinear")
            }
            if model.settings.nativeScene {
                Toggle("Perspective-correct textures", isOn: $model.settings.perspective)
                note("On: textures stay stable as surfaces recede into the distance. Off: affine mapping recreates PlayStation texture warping.")
            } else {
                Toggle("Antialiasing", isOn: $model.settings.antialiasing)
            }
        }
    }
    var motion: some View {
        VStack(alignment: .leading, spacing: 18) {
            Picker("Renderer", selection: $model.settings.nativeScene) {
                Text("Original").tag(false)
                Text("Enhanced — native rendering").tag(true)
            }
            note("Render at your chosen resolution and frame rate while retaining the original driving physics, race timers and audio timing.")
            if model.settings.nativeScene {
                HStack {
                    Text("Frames per second")
                    Spacer()
                    TextField("0 = display", value: $model.settings.nativeFps, format: .number.grouping(.never))
                        .labelsHidden().frame(width: 100).textFieldStyle(.roundedBorder)
                        .accessibilityLabel("Frames per second; zero matches display")
                }
                HStack {
                    Text("Quick target")
                    Button("Display") { model.settings.nativeFps = 0 }
                    ForEach([60, 120, 144], id: \.self) { fps in
                        Button("\(fps) fps") { model.settings.nativeFps = fps }
                    }
                }
                note("0 matches your display refresh rate. Set a custom target from 30 to 360 FPS, or choose a preset above. Aspect ratio and render resolution are in Display.")
                Toggle("Developer frame-time graph (G)", isOn: $model.settings.frameGraph)
                note("G toggles the graph while playing. Press P to save a screenshot and diagnostic snapshot; frame times are recorded automatically.")
            }
            Picker("VSync", selection: $model.settings.vsync) {
                Text("On").tag("on"); Text("Off").tag("off"); Text("Adaptive").tag("adaptive")
            }
            Toggle("Low latency input sampling", isOn: $model.settings.lowLatency)
            note("VSync caps presentation to your display refresh rate. Low latency sampling refreshes input after frame pacing without changing the physics rate.")
        }
    }
    var controls: some View {
        VStack(alignment: .leading, spacing: 18) {
            Text("Keyboard & gamepad").font(.headline)
            note("Enter → Start · Arrows → Steer\nX / Space → Accelerate · Z → Brake\nClick the enhanced window to use its keyboard controls.")
            Toggle("Rewind", isOn: $model.settings.rewind)
            note("F8 rewinds in the original runtime window (Original mode, or a visible diagnostic companion). In the enhanced window, P/F8 captures a visual issue.")
            Button("Controls & advanced settings…") { model.launch(advanced: true) }
            note("Opens PSXRecomp’s full launcher for controller bindings and advanced settings. Settings are reloaded here when it closes.")
            Divider()
            Button("Diagnostics") { NSWorkspace.shared.open(model.root.appendingPathComponent("diagnostics")) }
            Button("Show settings file") { NSWorkspace.shared.activateFileViewerSelecting([model.root.appendingPathComponent("build-macos/settings.toml")]) }
        }
    }
}
@main struct RevolutionServiceMenu: App {
    var body: some Scene {
        Window("Ridge Racer Revolution · Service Menu", id: "service") { ServiceView() }
            .windowResizability(.contentMinSize)
            .defaultSize(width: 860, height: 690)
    }
}

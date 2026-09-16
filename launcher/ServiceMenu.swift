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
                    .frame(width: 100).textFieldStyle(.roundedBorder).onSubmit { applyNativeWidth() }
                Button("Apply") { applyNativeWidth() }
                Text("× \(model.settings.nativeHeight)")
            }
            Text("Only \(model.settings.nativeAspect) resolutions are available. Apply a custom width to calculate a matching height. Internal rendering resolution; display scaling is separate.")
        }.onAppear { nativeWidthInput = String(model.settings.nativeWidth) }
            .onChange(of: model.settings.nativeWidth) { value in nativeWidthInput = String(value) }
    }
    var body: some View {
        ScrollView { VStack(alignment: .leading, spacing: 20) {
            VStack(alignment: .leading, spacing: 4) {
                Text("RIDGE RACER").font(.system(size: 30, weight: .black, design: .rounded)).italic()
                Text("REVOLUTION").font(.system(size: 24, weight: .black, design: .rounded)).foregroundStyle(accent)
                Text("SERVICE MENU  /  USA · SLUS-00214  /  APPLE SILICON").font(.system(size: 10, design: .monospaced)).foregroundStyle(.secondary)
            }
            Rectangle().fill(accent).frame(height: 3)
            Text("Enhanced rendering").font(.headline)
            Text("Render at your chosen resolution and refresh rate while retaining the original driving physics. Settings and saves remain separate from Ridge Racer.").font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Form {
                Picker("Renderer", selection: $model.settings.nativeScene) {
                    Text("Original").tag(false)
                    Text("Enhanced — native rendering").tag(true)
                }
                if model.settings.nativeScene {
                    Picker("Aspect ratio", selection: Binding(get: { model.settings.nativeAspect }, set: { aspect in
                        model.settings.nativeAspect = aspect
                        alignNativeResolution()
                    })) {
                        Text("4:3").tag("4:3"); Text("16:9").tag("16:9")
                    }
                    nativeResolution
                    HStack {
                        Text("Frame rate")
                        TextField("0 = display", value: $model.settings.nativeFps, format: .number.grouping(.never)).frame(width: 90).textFieldStyle(.roundedBorder)
                        Button("Display") { model.settings.nativeFps = 0 }
                        ForEach([60,120,144], id: \.self) { fps in
                            Button("\(fps)") { model.settings.nativeFps = fps }
                        }
                    }
                    Toggle("Perspective-correct textures", isOn: $model.settings.perspective)
                    Toggle("Full course visibility", isOn: $model.settings.fullScene)
                    Picker("CPU car draw distance", selection: $model.settings.nativeCarDistance) {
                        Text("All cars · unlimited").tag(0); Text("1× · original").tag(1)
                        ForEach(2...5, id: \.self) { Text("\($0)×").tag($0) }
                    }
                    Toggle("Developer frame-time graph (G)", isOn: $model.settings.frameGraph)
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
                    Text("Windowed").tag(0);Text("Borderless fullscreen").tag(1);Text("Fullscreen").tag(2)
                }
                Picker("Texture filtering", selection: $model.settings.filtering) {
                    Text("Sharp / nearest").tag("nearest");Text("Smooth / bilinear").tag("bilinear")
                }
                if !model.settings.nativeScene { Toggle("Antialiasing", isOn: $model.settings.antialiasing) }
                Picker("VSync", selection: $model.settings.vsync) {
                    Text("On").tag("on");Text("Off").tag("off");Text("Adaptive").tag("adaptive")
                }
                Toggle("Low latency input sampling", isOn: $model.settings.lowLatency)
                Toggle("Rewind", isOn: $model.settings.rewind)
                if model.settings.rewind { Text("F8 rewinds when the original companion window has focus. In the enhanced window, P/F8 captures a visual issue.").font(.caption).foregroundStyle(.secondary) }
            }.disabled(model.running)
            Text(model.settings.nativeScene ? "VSync caps presentation to your display refresh rate. Press P to save a screenshot and diagnostic snapshot; frame times are recorded automatically." : "Input is sampled again after frame pacing. This does not change the physics rate.").font(.caption).foregroundStyle(.secondary)
            Divider()
            HStack {
                Button("Controls & advanced settings") { model.launch(advanced: true) }
                Button("Diagnostics") { NSWorkspace.shared.open(model.root.appendingPathComponent("diagnostics")) }
                Spacer()
                Button("Save") { model.save() }
                Button(model.running ? "Game running…" : "Launch game") { model.launch() }.buttonStyle(.borderedProminent).tint(accent)
            }.disabled(model.running)
            Text(model.message.isEmpty ? "Settings and memory cards are private to Revolution." : model.message).font(.caption).foregroundStyle(.secondary)
        }.padding(28) }.frame(width: 726, height: 850).preferredColorScheme(.dark)
        .alert("Ridge Racer Revolution", isPresented: Binding(get: { !model.error.isEmpty }, set: { if !$0 { model.error = "" } })) {
            Button("OK") { model.error = "" }
        } message: { Text(model.error) }
    }
}
@main struct RevolutionServiceMenu: App {
    var body: some Scene {
        WindowGroup("Ridge Racer Revolution") { ServiceView() }
            .windowResizability(.contentSize)
    }
}

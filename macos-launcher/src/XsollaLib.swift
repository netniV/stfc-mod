import Foundation

struct DownloadAction {
  var url: String
  var size: Int
  var to: String
}

struct ExtractAction {
  var file: String
  var to: String
}

struct PatchAction {
  var binaries: String
  var patch: String
}

struct VersionAction {
  var version: Int
}

enum XsollaUpdateAction {
  case Download(DownloadAction)
  case Extract(ExtractAction)
  case Patch(PatchAction)
  case Version(VersionAction)
  case WaitActions
}

struct PatchRule: Decodable {
  var file_size: Optional<Int>
  var relative_path: String
  var rule: String
  var sha512: Optional<String>
}

class XsollaUpdateParser: NSObject, XMLParserDelegate {

  var articleNth = 0

  var gameVersion = 0

  var actions: [XsollaUpdateAction] = []

  func parser(
    _ parser: XMLParser,
    didStartElement elementName: String,
    namespaceURI: String?,
    qualifiedName qName: String?,
    attributes attributeDict: [String: String] = [:]
  ) {
    if elementName == "action" {
      switch attributeDict["type"] {
      case "torrent_download":
        actions.append(
          XsollaUpdateAction.Download(
            DownloadAction(
              url: attributeDict["alt_data_link"]!,
              size: Int(attributeDict["data_size"]!)!,
              to: attributeDict["alt_to"]!)))
        break
      case "extract":
        actions.append(
          XsollaUpdateAction.Extract(
            ExtractAction(
              file: attributeDict["file"]!,
              to: attributeDict["to"]!)))
        break
      case "patch":
        actions.append(
          XsollaUpdateAction.Patch(
            PatchAction(binaries: attributeDict["binaries"]!, patch: attributeDict["patch"]!)))
        break
      case "wait_actions":
        actions.append(XsollaUpdateAction.WaitActions)
        break
      case "extracted_size":
        break
      case "version":
        gameVersion = Int(attributeDict["version"]!)!
        actions.append(XsollaUpdateAction.Version(VersionAction(version: gameVersion)))
        break
      default:
        print("Unknown action type: \(attributeDict["type"]!)")
        break
      }
    }
  }
}

enum XsollaUpdateProgress {
  case Start(totalActions: Int)
  case Progress(currentAction: Int, totalActions: Int)
  case Extracting(currentFile: String)
  case ExtractComplete(currentFile: String)
  case Downloading(url: String)
  case DownloadComplete(url: String)
  case Patching(totalFiles: Int)
  case PatchingProgress(currentBytes: Int, totalBytes: Int)
  case PatchStepComplete
  case PatchComplete
  case Waiting
  case ApplyVersion
  case VersionApplied
  case Finalizing
  case CleaningUp
  case Complete
}

protocol XSollaUpdaterDelegate {
  func updateProgress(progress: XsollaUpdateProgress)
}

struct XsollaUpdater {
  var gameName: String

  public init(_ gameName: String) {
    self.gameName = gameName
  }

  func gamePath() throws -> String {
    let library = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first
    if let library {
      let preferences = library.appendingPathComponent("Preferences").appendingPathComponent(
        self.gameName)
      let settingsIniPath = preferences.appendingPathComponent("launcher_settings.ini")
      let settingsIni = try parseConfig(settingsIniPath.path)
      let gamePath = settingsIni["General"]?["152033..GAME_PATH"]
      if let gamePath {
        if gamePath.starts(with: "//") {
          return String(gamePath.dropFirst())
        }
        return gamePath
      }
    }
    throw NSError(domain: "XsollaUpdater", code: 1, userInfo: nil)
  }

  func gameTempPath() throws -> String {
    let library = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first
    if let library {
      let preferences = library.appendingPathComponent("Preferences").appendingPathComponent(
        self.gameName)
      let settingsIniPath = preferences.appendingPathComponent("launcher_settings.ini")
      let settingsIni = try parseConfig(settingsIniPath.path)
      let gameTempPath = settingsIni["General"]?["152033..GAME_TEMP_PATH"]
      if let gameTempPath {
        if gameTempPath.starts(with: "//") {
          return String(gameTempPath.dropFirst())
        }
        return gameTempPath
      }
    }
    throw NSError(domain: "XsollaUpdater", code: 1, userInfo: nil)
  }

  func installedVersion() -> Int {
    do {
      var gamePath = URL(fileURLWithPath: try self.gamePath())
      gamePath.appendPathComponent(".version")
      let versionFile = try String(contentsOf: gamePath, encoding: String.Encoding.utf8)
      let versionSegments = versionFile.split(separator: "=")
      return Int(versionSegments[1]) ?? 0
    } catch {
      return 0
    }
  }

  class UpdateAction {
    var type: String
    var version: Int
    var url: String

    init(type: String, version: Int, url: String) {
      self.type = type
      self.version = version
      self.url = url
    }
  }

  func latestGameVersion() async throws -> Int {
    let url = URL(
      string: String(
        format:
          "https://gus.xsolla.com/updates?version=%d&project_id=152033&region=&platform=mac_os",
        self.installedVersion()))
    if let url {
      let (data, _) = try await URLSession.shared.data(from: url)

      let xml = XMLParser(data: data)
      let parser = XsollaUpdateParser()
      xml.delegate = parser
      xml.parse()
      return parser.gameVersion
    }
    throw NSError(domain: "XsollaUpdater", code: 2, userInfo: nil)
  }

  func checkForUpdateAvailable() async -> Bool {
    do {
      let latestGameVersion = try await self.latestGameVersion()
      return self.installedVersion() < latestGameVersion
    } catch {
      return false
    }
  }

  func updateGame(delegate: XSollaUpdaterDelegate? = nil) async throws {
    defer { delegate?.updateProgress(progress: XsollaUpdateProgress.Complete) }
    let url = try URL(
      string: String(
        format:
          "https://gus.xsolla.com/updates?version=%d&project_id=152033&region=&platform=mac_os",
        self.installedVersion()))
    if url == nil {
      throw NSError(domain: "XsollaUpdater", code: 3, userInfo: nil)
    }
    let (data, _) = try await URLSession.shared.data(from: url!)

    let xml = XMLParser(data: data)
    let parser = XsollaUpdateParser()
    xml.delegate = parser
    xml.parse()

    var tempGamePath = try self.gameTempPath()
    if tempGamePath.hasSuffix("/") || tempGamePath.hasSuffix("\\") {
      tempGamePath = String(tempGamePath.dropLast())
    }
    var gamePath = try self.gamePath()
    if gamePath.hasSuffix("/") || gamePath.hasSuffix("\\") {
      gamePath = String(gamePath.dropLast())
    }
    let tempPath = TemporaryFolderURL()
    do {
      try FileManager.default.removeItem(atPath: tempGamePath)
    } catch {}
    try FileManager.default.createDirectory(
      atPath: tempGamePath, withIntermediateDirectories: true, attributes: nil)

    delegate?.updateProgress(
      progress: XsollaUpdateProgress.Start(totalActions: parser.actions.count))

    var currentAction = 0
    for action in parser.actions {
      currentAction += 1
      delegate?.updateProgress(
        progress: XsollaUpdateProgress.Progress(
          currentAction: currentAction, totalActions: parser.actions.count))
      switch action {
      case .Download(let downloadAction):
        delegate?.updateProgress(
          progress: XsollaUpdateProgress.Downloading(url: downloadAction.url))
        let (localURL, response) = try await URLSession.shared.download(
          from: URL(string: downloadAction.url)!)
        let toPath = downloadAction.to.replacingOccurrences(of: "$temp_path", with: tempGamePath)
        delegate?.updateProgress(
          progress: XsollaUpdateProgress.DownloadComplete(url: downloadAction.url))
        try FileManager.default.moveItem(at: localURL, to: URL(fileURLWithPath: toPath))
        break
      case .Extract(let extractAction):
        let fromPath = extractAction.file.replacingOccurrences(of: "$temp_path", with: tempGamePath)
        let toPath = extractAction.to.replacingOccurrences(of: "$temp_path", with: tempGamePath)
        let archivePath = try Path(fromPath)
        let archivePathInStream = try InStream(path: archivePath)
        let decoder = try Decoder(
          stream: archivePathInStream, fileType: .sevenZ)
        let _ = try decoder.open()
        delegate?.updateProgress(progress: XsollaUpdateProgress.Extracting(currentFile: fromPath))
        let _ = try decoder.extract(to: Path(toPath))
        delegate?.updateProgress(
          progress: XsollaUpdateProgress.ExtractComplete(currentFile: fromPath))
        break
      case .Patch(let patchAction):
        var binaries = patchAction.binaries
        binaries = binaries.replacingOccurrences(of: "$temp_path", with: tempGamePath)
        binaries = binaries.replacingOccurrences(
          of: "$game_path", with: gamePath)
        let patch = patchAction.patch.replacingOccurrences(of: "$temp_path", with: tempGamePath)
        let patch_rules_json = URL(fileURLWithPath: patch).appendingPathComponent("patchRules.json")
        let patch_rules = try JSONDecoder().decode(
          [PatchRule].self, from: Data(contentsOf: patch_rules_json))
        delegate?.updateProgress(
          progress: XsollaUpdateProgress.Patching(totalFiles: patch_rules.count))
        for rule in patch_rules {
          var relativePath = rule.relative_path.trimmingCharacters(in: .whitespacesAndNewlines)
          if relativePath.starts(with: "/") || relativePath.starts(with: "\\") {
            relativePath = String(relativePath.dropFirst())
          }

          let targetPath = tempPath.contentURL.appendingPathComponent(relativePath)
          let sourcePath = URL(fileURLWithPath: gamePath).appendingPathComponent(relativePath)
          let patchPath = URL(fileURLWithPath: patch).appendingPathComponent(relativePath)

          //delegate?.updateProgress()
          switch rule.rule {
          case "patch":
            if !FileManager.default.fileExists(atPath: targetPath.path) {
              try FileManager.default.createDirectory(
                at: targetPath.deletingLastPathComponent(), withIntermediateDirectories: true,
                attributes: nil)
              try FileManager.default.copyItem(at: sourcePath, to: targetPath)
            }
            try rsyncApply(source: sourcePath, patch: patchPath, output: targetPath)
            break
          case "create":
            if !FileManager.default.fileExists(atPath: targetPath.path) {
              try FileManager.default.createDirectory(
                at: targetPath.deletingLastPathComponent(), withIntermediateDirectories: true,
                attributes: nil)
              try Data().write(to: targetPath)
            }
          case "delete":
            do {
              try FileManager.default.removeItem(at: sourcePath)
            } catch {
              print("Error deleting file \(sourcePath)")
            }
          case "copy":
            if !FileManager.default.fileExists(atPath: targetPath.path) {
              try FileManager.default.createDirectory(
                at: targetPath.deletingLastPathComponent(), withIntermediateDirectories: true,
                attributes: nil)
            }
            try FileManager.default.copyItem(at: patchPath, to: targetPath)
            break
          default:
            print("Unknown rule \(rule.rule)")
            break
          }
          delegate?.updateProgress(progress: XsollaUpdateProgress.PatchStepComplete)
        }
        delegate?.updateProgress(progress: XsollaUpdateProgress.PatchComplete)
        break
      case .WaitActions:
        delegate?.updateProgress(progress: XsollaUpdateProgress.Waiting)
        break
      case .Version(let versionAction):
        var versionPath = URL(fileURLWithPath: gamePath)
        versionPath.appendPathComponent(".version")
        let fileVersion = String(format: "&game=%d", versionAction.version)
        delegate?.updateProgress(progress: XsollaUpdateProgress.ApplyVersion)
        try fileVersion.write(to: versionPath, atomically: true, encoding: .utf8)
        delegate?.updateProgress(progress: XsollaUpdateProgress.VersionApplied)
        break

      }
    }
    delegate?.updateProgress(progress: XsollaUpdateProgress.Finalizing)
    copyContentsOfDirectory(from: tempPath.contentURL, to: URL(fileURLWithPath: gamePath))
    delegate?.updateProgress(progress: XsollaUpdateProgress.CleaningUp)
    try FileManager.default.removeItem(atPath: tempGamePath)

  }
}

func copyContentsOfDirectory(from sourceURL: URL, to targetURL: URL) {
  let fileManager = FileManager.default

  guard
    let sourceContents = try? fileManager.contentsOfDirectory(
      at: sourceURL, includingPropertiesForKeys: nil)
  else {
    print("Error accessing contents of directory at \(sourceURL.path)")
    return
  }

  for sourceItem in sourceContents {
    let destinationItem = targetURL.appendingPathComponent(sourceItem.lastPathComponent)

    do {
      if try sourceItem.resourceValues(forKeys: [.isDirectoryKey]).isDirectory ?? false {
        // If sourceItem is a directory, create corresponding directory in the target location
        try fileManager.createDirectory(
          at: destinationItem, withIntermediateDirectories: true, attributes: nil)
        copyContentsOfDirectory(from: sourceItem, to: destinationItem)  // Recursively copy contents of subdirectory
      } else {
        // If sourceItem is not a directory, check if a file with the same name exists in the target location
        if fileManager.fileExists(atPath: destinationItem.path) {
          // If a file with the same name exists, remove it before copying
          try fileManager.removeItem(at: destinationItem)
        }
        // Copy the file from the source directory to the target directory
        try fileManager.copyItem(at: sourceItem, to: destinationItem)
      }
    } catch {
      print("Error copying \(sourceItem.lastPathComponent) to \(destinationItem.path): \(error)")
    }
  }
}

func run_update() {
  //
}

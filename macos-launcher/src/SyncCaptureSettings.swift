import Foundation

enum SyncCaptureSettings {
  private static let bundleIdentifier = "com.stfcmod.startrekpatch"
  private static let beginMarker = "# BEGIN STFC LAUNCHER LOCAL SYNC CAPTURE"
  private static let endMarker = "# END STFC LAUNCHER LOCAL SYNC CAPTURE"

  static var settingsFileURL: URL {
    let library = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first!
    return library
      .appendingPathComponent("Preferences")
      .appendingPathComponent(bundleIdentifier)
      .appendingPathComponent("community_patch_settings.toml")
  }

  static var capturesRootURL: URL {
    let applicationSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
    return applicationSupport
      .appendingPathComponent(bundleIdentifier)
      .appendingPathComponent("Sync Captures")
  }

  static func createCaptureDirectory() throws -> URL {
    let formatter = DateFormatter()
    formatter.locale = Locale(identifier: "en_US_POSIX")
    formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"

    let root = capturesRootURL
    try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true, attributes: nil)

    let baseName = formatter.string(from: Date())
    var directory = root.appendingPathComponent(baseName)
    var suffix = 2

    while FileManager.default.fileExists(atPath: directory.path) {
      directory = root.appendingPathComponent("\(baseName)-\(suffix)")
      suffix += 1
    }

    try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true, attributes: nil)
    return directory
  }

  static func install(port: Int, token: String) throws {
    let settingsURL = settingsFileURL
    try FileManager.default.createDirectory(
      at: settingsURL.deletingLastPathComponent(),
      withIntermediateDirectories: true,
      attributes: nil)

    let existing = try existingSettings(at: settingsURL)
    let updated = replacingManagedBlock(in: existing, with: managedBlock(port: port, token: token))
    try updated.write(to: settingsURL, atomically: true, encoding: .utf8)
  }

  static func remove() throws {
    let settingsURL = settingsFileURL
    guard FileManager.default.fileExists(atPath: settingsURL.path) else {
      return
    }

    let existing = try existingSettings(at: settingsURL)
    let updated = replacingManagedBlock(in: existing, with: nil)
    try updated.write(to: settingsURL, atomically: true, encoding: .utf8)
  }

  private static func existingSettings(at settingsURL: URL) throws -> String {
    guard FileManager.default.fileExists(atPath: settingsURL.path) else {
      return ""
    }

    return try String(contentsOf: settingsURL, encoding: .utf8)
  }

  private static func managedBlock(port: Int, token: String) -> String {
    """
    \(beginMarker)
    # Managed by the STFC macOS launcher. Start Capture before Engage and keep the launcher open.
    [sync.targets.local_launcher_capture]
    url = "http://127.0.0.1:\(port)/sync"
    token = "\(tomlString(token))"
    proxy = ""
    verify_ssl = false
    battlelogs = true
    buffs = true
    buildings = true
    inventory = true
    jobs = true
    missions = true
    officer = true
    research = true
    resources = true
    ships = true
    slots = true
    tech = true
    traits = true
    \(endMarker)
    """
  }

  private static func replacingManagedBlock(in contents: String, with block: String?) -> String {
    var result = contents

    while let begin = result.range(of: beginMarker),
      let end = result.range(of: endMarker, range: begin.upperBound..<result.endIndex)
    {
      var removalRange = begin.lowerBound..<end.upperBound

      if removalRange.lowerBound > result.startIndex {
        let before = result.index(before: removalRange.lowerBound)
        if result[before] == "\n" {
          removalRange = before..<removalRange.upperBound
        }
      }

      if removalRange.upperBound < result.endIndex {
        let after = removalRange.upperBound
        if result[after] == "\n" {
          removalRange = removalRange.lowerBound..<result.index(after: after)
        }
      }

      result.removeSubrange(removalRange)
    }

    guard let block else {
      return result
    }

    if result.isEmpty {
      return block + "\n"
    }

    return result.hasSuffix("\n") ? result + "\n" + block + "\n" : result + "\n\n" + block + "\n"
  }

  private static func tomlString(_ value: String) -> String {
    value
      .replacingOccurrences(of: "\\", with: "\\\\")
      .replacingOccurrences(of: "\"", with: "\\\"")
  }
}

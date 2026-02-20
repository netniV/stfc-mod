import Foundation
import os

// MARK: - Log Level

enum LogLevel: Int, Comparable {
  case trace = 0
  case debug = 10
  case info = 20
  case warning = 30
  case error = 40

  static func < (lhs: LogLevel, rhs: LogLevel) -> Bool {
    lhs.rawValue < rhs.rawValue
  }

  var label: String {
    switch self {
    case .trace: return "TRACE"
    case .debug: return "DEBUG"
    case .info: return "INFO"
    case .warning: return "WARN"
    case .error: return "ERROR"
    }
  }
}

// MARK: - FileLogger

/// A simple file-based logger inspired by bit-log.
///
/// Log format:
/// ```
/// 2026-02-20T11:30:45.123+01:00 DEBUG [entitlements        ]: Some message here
/// ```
///
/// Writes to `~/Library/Preferences/com.stfcmod.startrekpatch/launcher.log`
/// and simultaneously forwards to Apple's `os.Logger` for Console.app visibility.
///
/// On each launch, rotates the previous day's log to `launcher-YYYY-MM-DD.log`
/// and purges logs older than 30 days.
final class FileLogger {
  let category: String
  let level: LogLevel
  private let osLogger: Logger
  private static let logFileURL: URL? = initLogFile()
  private static let lock = NSLock()
  private static let dateFormatter: DateFormatter = {
    let fmt = DateFormatter()
    fmt.dateFormat = "yyyy-MM-dd'T'HH:mm:ss.SSS"
    fmt.locale = Locale(identifier: "en_US_POSIX")
    return fmt
  }()
  private static let dayFormatter: DateFormatter = {
    let fmt = DateFormatter()
    fmt.dateFormat = "yyyy-MM-dd"
    fmt.locale = Locale(identifier: "en_US_POSIX")
    return fmt
  }()

  init(subsystem: String = "com.stfcmod.startrekpatch", category: String, level: LogLevel = .debug) {
    self.category = category
    self.level = level
    self.osLogger = Logger(subsystem: subsystem, category: category)
  }

  // MARK: - Log methods

  func trace(_ message: String) {
    log(.trace, message)
  }

  func debug(_ message: String) {
    log(.debug, message)
  }

  func info(_ message: String) {
    log(.info, message)
  }

  func warning(_ message: String) {
    log(.warning, message)
  }

  func error(_ message: String) {
    log(.error, message)
  }

  // MARK: - Core

  private func log(_ msgLevel: LogLevel, _ message: String) {
    guard msgLevel >= level else { return }

    let line = formatLine(msgLevel, message)
    writeToFile(line)

    // Forward to os.Logger
    switch msgLevel {
    case .trace:
      osLogger.trace("\(message, privacy: .public)")
    case .debug:
      osLogger.debug("\(message, privacy: .public)")
    case .info:
      osLogger.info("\(message, privacy: .public)")
    case .warning:
      osLogger.warning("\(message, privacy: .public)")
    case .error:
      osLogger.error("\(message, privacy: .public)")
    }
  }

  // MARK: - Formatting (bit-log style)

  private func formatLine(_ msgLevel: LogLevel, _ message: String) -> String {
    let timestamp = Self.formatTimestamp(Date())
    let paddedLevel = msgLevel.label.padding(toLength: 5, withPad: " ", startingAt: 0)
    let paddedCategory = truncateOrExtend(category, length: 20)
    return "\(timestamp) \(paddedLevel) [\(paddedCategory)]: \(message)"
  }

  private static func formatTimestamp(_ date: Date) -> String {
    let base = dateFormatter.string(from: date)
    let seconds = TimeZone.current.secondsFromGMT(for: date)
    let sign = seconds >= 0 ? "+" : "-"
    let absSeconds = abs(seconds)
    let hours = absSeconds / 3600
    let minutes = (absSeconds % 3600) / 60
    return String(format: "%@%@%02d:%02d", base, sign, hours, minutes)
  }

  private func truncateOrExtend(_ str: String, length: Int) -> String {
    if str.count <= length {
      return str.padding(toLength: length, withPad: " ", startingAt: 0)
    }
    let charsToShow = length - 1
    let front = charsToShow / 2 + charsToShow % 2
    let back = charsToShow / 2
    let startIdx = str.startIndex
    let frontEnd = str.index(startIdx, offsetBy: front)
    let backStart = str.index(str.endIndex, offsetBy: -back)
    return "\(str[startIdx..<frontEnd])\u{2026}\(str[backStart..<str.endIndex])"
  }

  // MARK: - File I/O & Rotation

  private static func logDirectory() -> URL? {
    guard let library = FileManager.default.urls(
      for: .libraryDirectory, in: .userDomainMask
    ).first else {
      return nil
    }
    return library
      .appendingPathComponent("Preferences")
      .appendingPathComponent("com.stfcmod.startrekpatch")
  }

  private static func initLogFile() -> URL? {
    guard let logDir = logDirectory() else { return nil }

    let fileManager = FileManager.default
    try? fileManager.createDirectory(at: logDir, withIntermediateDirectories: true)

    let logURL = logDir.appendingPathComponent("launcher.log")

    // Rotate previous day's log and purge old ones
    rotateIfNeeded(logURL: logURL, logDir: logDir)
    purgeOldLogs(logDir: logDir)

    // Write startup banner
    let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "unknown"
    let banner = "\(formatTimestamp(Date())) INFO  [launcher            ]:"
      + " STFC Community Mod Launcher v\(version) started\n"
    if let data = banner.data(using: .utf8) {
      if fileManager.fileExists(atPath: logURL.path) {
        if let handle = try? FileHandle(forWritingTo: logURL) {
          handle.seekToEndOfFile()
          handle.write(data)
          try? handle.close()
        }
      } else {
        try? data.write(to: logURL)
      }
    }

    return logURL
  }

  /// If the existing launcher.log contains entries from a previous day,
  /// rename it to `launcher-YYYY-MM-DD.log` (using the file's last modification date).
  private static func rotateIfNeeded(logURL: URL, logDir: URL) {
    let fileManager = FileManager.default
    guard fileManager.fileExists(atPath: logURL.path) else { return }

    guard let attrs = try? fileManager.attributesOfItem(atPath: logURL.path),
          let modDate = attrs[.modificationDate] as? Date else { return }

    let today = dayFormatter.string(from: Date())
    let fileDay = dayFormatter.string(from: modDate)

    // Only rotate if the log is from a previous day
    guard fileDay != today else { return }

    let rotatedName = "launcher-\(fileDay).log"
    let rotatedURL = logDir.appendingPathComponent(rotatedName)

    // If a rotated file for that day already exists, append to it
    if fileManager.fileExists(atPath: rotatedURL.path) {
      if let existingData = try? Data(contentsOf: logURL),
         let handle = try? FileHandle(forWritingTo: rotatedURL) {
        handle.seekToEndOfFile()
        handle.write(existingData)
        try? handle.close()
      }
      try? fileManager.removeItem(at: logURL)
    } else {
      try? fileManager.moveItem(at: logURL, to: rotatedURL)
    }
  }

  /// Remove launcher-YYYY-MM-DD.log files older than 30 days.
  private static func purgeOldLogs(logDir: URL) {
    let fileManager = FileManager.default
    guard let files = try? fileManager.contentsOfDirectory(atPath: logDir.path) else { return }

    let cutoff = Calendar.current.date(byAdding: .day, value: -30, to: Date()) ?? Date()

    for file in files {
      // Match launcher-YYYY-MM-DD.log pattern
      guard file.hasPrefix("launcher-"), file.hasSuffix(".log"),
            file != "launcher.log" else { continue }

      let dateString = file
        .replacingOccurrences(of: "launcher-", with: "")
        .replacingOccurrences(of: ".log", with: "")

      guard let fileDate = dayFormatter.date(from: dateString) else { continue }

      if fileDate < cutoff {
        let filePath = logDir.appendingPathComponent(file)
        try? fileManager.removeItem(at: filePath)
      }
    }
  }

  private func writeToFile(_ line: String) {
    guard let url = Self.logFileURL else { return }

    Self.lock.lock()
    defer { Self.lock.unlock() }

    guard let data = "\(line)\n".data(using: .utf8) else { return }

    if FileManager.default.fileExists(atPath: url.path) {
      guard let handle = try? FileHandle(forWritingTo: url) else { return }
      handle.seekToEndOfFile()
      handle.write(data)
      try? handle.close()
    } else {
      try? data.write(to: url)
    }
  }
}

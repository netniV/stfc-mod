import Foundation
import Network
import os

private let syncCaptureLogger = Logger(subsystem: "com.stfcmod.startrekpatch", category: "sync-capture")

enum SyncCaptureServerError: LocalizedError {
  case alreadyRunning
  case listenerStoppedBeforeReady
  case listenerFailed(String)
  case missingPort
  case unableToCreateRequestsFile

  var errorDescription: String? {
    switch self {
    case .alreadyRunning:
      return "Sync capture is already running."
    case .listenerStoppedBeforeReady:
      return "The sync capture server stopped before it was ready."
    case .listenerFailed(let message):
      return "Failed to start sync capture server: \(message)"
    case .missingPort:
      return "The sync capture server did not report a local port."
    case .unableToCreateRequestsFile:
      return "Could not create requests.jsonl in the capture folder."
    }
  }
}

final class SyncCaptureServer: ObservableObject {
  @Published private(set) var isRunning = false
  @Published private(set) var captureDirectory: URL?
  @Published private(set) var port: Int?
  @Published private(set) var requestCount = 0
  @Published private(set) var lastError: String?

  private let serverQueue = DispatchQueue(label: "com.stfcmod.startrekpatch.sync-capture")
  private let maxBodyBytes = 250 * 1024 * 1024
  private let maxHeaderBytes = 64 * 1024

  private var listener: NWListener?
  private var token = ""
  private var requestsFile: FileHandle?

  deinit {
    stop()
  }

  func start() async throws {
    if isRunning || listener != nil {
      throw SyncCaptureServerError.alreadyRunning
    }

    let captureDirectory = try SyncCaptureSettings.createCaptureDirectory()
    let requestsURL = captureDirectory.appendingPathComponent("requests.jsonl")
    guard FileManager.default.createFile(atPath: requestsURL.path, contents: nil),
      let requestsFile = try? FileHandle(forWritingTo: requestsURL)
    else {
      throw SyncCaptureServerError.unableToCreateRequestsFile
    }

    let token = Self.generateToken()
    let parameters = NWParameters.tcp
    parameters.allowLocalEndpointReuse = true
    parameters.requiredLocalEndpoint = NWEndpoint.hostPort(
      host: .ipv4(IPv4Address("127.0.0.1")!),
      port: .any)

    let listener = try NWListener(using: parameters)

    self.token = token
    self.requestsFile = requestsFile
    self.listener = listener

    await MainActor.run {
      self.captureDirectory = captureDirectory
      self.port = nil
      self.requestCount = 0
      self.lastError = nil
    }

    do {
      let port = try await waitForReady(listener)
      try SyncCaptureSettings.install(port: port, token: token)
      await MainActor.run {
        self.port = port
        self.isRunning = true
      }
      syncCaptureLogger.info("Sync capture listening on 127.0.0.1:\(port, privacy: .public)")
    } catch {
      cleanupAfterFailedStart()
      throw error
    }
  }

  func stop() {
    listener?.cancel()
    listener = nil
    token = ""

    try? requestsFile?.close()
    requestsFile = nil

    if isRunning {
      try? SyncCaptureSettings.remove()
    }

    updateUIState {
      self.isRunning = false
      self.port = nil
    }
  }

  private func cleanupAfterFailedStart() {
    listener?.cancel()
    listener = nil
    token = ""
    try? requestsFile?.close()
    requestsFile = nil
    updateUIState {
      self.isRunning = false
      self.port = nil
    }
    try? SyncCaptureSettings.remove()
  }

  private func updateUIState(_ update: () -> Void) {
    if Thread.isMainThread {
      update()
    } else {
      DispatchQueue.main.sync(execute: update)
    }
  }

  private func waitForReady(_ listener: NWListener) async throws -> Int {
    try await withCheckedThrowingContinuation { continuation in
      let readySignal = ListenerReadySignal()

      listener.newConnectionHandler = { [weak self] connection in
        self?.handle(connection)
      }

      listener.stateUpdateHandler = { [weak self, weak listener] state in
        switch state {
        case .ready:
          guard let port = listener?.port else {
            readySignal.resume(continuation, with: .failure(SyncCaptureServerError.missingPort))
            return
          }

          readySignal.resume(continuation, with: .success(Int(port.rawValue)))

        case .failed(let error):
          if readySignal.hasResumed {
            self?.recordFailure(error.localizedDescription)
          } else {
            readySignal.resume(
              continuation,
              with: .failure(SyncCaptureServerError.listenerFailed(error.localizedDescription)))
          }

        case .cancelled:
          readySignal.resume(continuation, with: .failure(SyncCaptureServerError.listenerStoppedBeforeReady))

        default:
          break
        }
      }

      listener.start(queue: serverQueue)
    }
  }

  private func handle(_ connection: NWConnection) {
    connection.start(queue: serverQueue)

    var buffer = Data()

    func receive() {
      connection.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, isComplete, error in
        guard let self else {
          connection.cancel()
          return
        }

        if let data, !data.isEmpty {
          buffer.append(data)
        }

        do {
          switch try self.parseRequest(from: buffer) {
          case .needMoreData:
            if isComplete || error != nil {
              self.sendResponse(400, "Bad Request", connection: connection)
            } else {
              receive()
            }

          case .request(let request):
            self.process(request, connection: connection)
          }
        } catch let error as HTTPError {
          self.sendResponse(error.statusCode, error.message, connection: connection)
        } catch {
          self.sendResponse(400, "Bad Request", connection: connection)
        }
      }
    }

    receive()
  }

  private func process(_ request: HTTPRequest, connection: NWConnection) {
    guard request.method == "POST" else {
      sendResponse(405, "Method Not Allowed", connection: connection)
      return
    }

    guard request.path == "/sync" else {
      sendResponse(404, "Not Found", connection: connection)
      return
    }

    guard request.headers["stfc-sync-token"] == token else {
      sendResponse(401, "Unauthorized", connection: connection)
      return
    }

    requestsFile?.write(request.body)
    if request.body.last != UInt8(ascii: "\n") {
      requestsFile?.write(Data([UInt8(ascii: "\n")]))
    }

    DispatchQueue.main.async { [weak self] in
      self?.requestCount += 1
    }

    sendResponse(204, "No Content", connection: connection)
  }

  private func parseRequest(from buffer: Data) throws -> ParseResult {
    guard buffer.count <= maxHeaderBytes + maxBodyBytes else {
      throw HTTPError(statusCode: 413, message: "Payload Too Large")
    }

    let separator = Data([13, 10, 13, 10])
    guard let headerRange = buffer.range(of: separator) else {
      if buffer.count > maxHeaderBytes {
        throw HTTPError(statusCode: 431, message: "Request Header Fields Too Large")
      }

      return .needMoreData
    }

    guard headerRange.lowerBound <= maxHeaderBytes else {
      throw HTTPError(statusCode: 431, message: "Request Header Fields Too Large")
    }

    let headerData = buffer[..<headerRange.lowerBound]
    guard let headerText = String(data: headerData, encoding: .utf8) else {
      throw HTTPError(statusCode: 400, message: "Bad Request")
    }

    let lines = headerText.components(separatedBy: "\r\n")
    guard let requestLine = lines.first else {
      throw HTTPError(statusCode: 400, message: "Bad Request")
    }

    let requestParts = requestLine.split(separator: " ", maxSplits: 2, omittingEmptySubsequences: true)
    guard requestParts.count >= 2 else {
      throw HTTPError(statusCode: 400, message: "Bad Request")
    }

    var headers: [String: String] = [:]
    for line in lines.dropFirst() where !line.isEmpty {
      guard let colon = line.firstIndex(of: ":") else {
        continue
      }

      let name = line[..<colon].lowercased()
      let valueStart = line.index(after: colon)
      let value = line[valueStart...].trimmingCharacters(in: .whitespaces)
      headers[name] = value
    }

    guard let contentLengthText = headers["content-length"], let contentLength = Int(contentLengthText) else {
      throw HTTPError(statusCode: 411, message: "Length Required")
    }

    guard contentLength >= 0 else {
      throw HTTPError(statusCode: 400, message: "Bad Request")
    }

    guard contentLength <= maxBodyBytes else {
      throw HTTPError(statusCode: 413, message: "Payload Too Large")
    }

    let bodyStart = headerRange.upperBound
    guard buffer.count >= bodyStart + contentLength else {
      return .needMoreData
    }

    let body = buffer.subdata(in: bodyStart..<(bodyStart + contentLength))
    let path = String(requestParts[1]).split(separator: "?", maxSplits: 1).first.map(String.init) ?? ""

    return .request(HTTPRequest(method: String(requestParts[0]), path: path, headers: headers, body: body))
  }

  private func sendResponse(_ statusCode: Int, _ message: String, connection: NWConnection) {
    let body = statusCode == 204 ? "" : message + "\n"
    let response = "HTTP/1.1 \(statusCode) \(message)\r\nContent-Length: \(body.utf8.count)\r\nConnection: close\r\n\r\n\(body)"

    connection.send(content: Data(response.utf8), completion: .contentProcessed { _ in
      connection.cancel()
    })
  }

  private func recordFailure(_ message: String) {
    listener?.cancel()
    listener = nil
    token = ""
    try? requestsFile?.close()
    requestsFile = nil
    try? SyncCaptureSettings.remove()

    DispatchQueue.main.async { [weak self] in
      self?.lastError = message
      self?.isRunning = false
      self?.port = nil
    }
  }

  private static func generateToken() -> String {
    (UUID().uuidString + UUID().uuidString).replacingOccurrences(of: "-", with: "")
  }
}

private enum ParseResult {
  case needMoreData
  case request(HTTPRequest)
}

private struct HTTPRequest {
  let method: String
  let path: String
  let headers: [String: String]
  let body: Data
}

private struct HTTPError: Error {
  let statusCode: Int
  let message: String
}

private final class ListenerReadySignal: @unchecked Sendable {
  private let lock = NSLock()
  private var resumed = false

  var hasResumed: Bool {
    lock.lock()
    defer { lock.unlock() }
    return resumed
  }

  func resume(_ continuation: CheckedContinuation<Int, Error>, with result: Result<Int, Error>) {
    lock.lock()
    guard !resumed else {
      lock.unlock()
      return
    }
    resumed = true
    lock.unlock()

    switch result {
    case .success(let port):
      continuation.resume(returning: port)
    case .failure(let error):
      continuation.resume(throwing: error)
    }
  }
}

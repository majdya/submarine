#include "comm_link.h"

#include <chrono>

namespace submarine {

CommLink::CommLink(ITransport& transport, MessageHandler onUnsolicited)
    : transport_(transport), onUnsolicited_(std::move(onUnsolicited)) {}

CommLink::~CommLink() { stop(); }

bool CommLink::isReplyType(uint8_t type) const {
  return type == MSG_TYPE_ACK || type == MSG_TYPE_DATA_REPORT;
}

void CommLink::start() {
  if (running_) return;
  running_ = true;
  reader_ = std::thread(&CommLink::readerLoop, this);
}

void CommLink::stop() {
  if (!running_) return;
  running_ = false;
  if (reader_.joinable()) reader_.join();
}

void CommLink::readerLoop() {
  while (running_) {
    auto bytes = transport_.readSome(256, 100 /*ms*/);
    if (bytes.empty()) continue;

    for (const auto& msg : decoder_.feed(bytes)) {
      if (isReplyType(msg.type)) {
        std::lock_guard<std::mutex> lock(waitMutex_);
        if (waiting_) {
          pendingReplies_.push_back(msg);
          waitCv_.notify_one();
        }
        // else: a reply arrived with nobody waiting (e.g. a stray/late
        // frame after a timeout) - dropped rather than misrouted to
        // onUnsolicited, since ACK/DATA_REPORT are never meaningful
        // outside a request/response exchange.
      } else if (onUnsolicited_) {
        onUnsolicited_(msg);
      }
    }
  }
}

bool CommLink::send(const std::vector<uint8_t>& wireBytes) {
  return transport_.write(wireBytes);
}

std::optional<proto::Message> CommLink::sendAndWait(const std::vector<uint8_t>& wireBytes,
                                                     int timeoutMs) {
  {
    std::lock_guard<std::mutex> lock(waitMutex_);
    pendingReplies_.clear();
    waiting_ = true;
  }

  if (!transport_.write(wireBytes)) {
    std::lock_guard<std::mutex> lock(waitMutex_);
    waiting_ = false;
    return std::nullopt;
  }

  std::unique_lock<std::mutex> lock(waitMutex_);
  bool got = waitCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                               [this] { return !pendingReplies_.empty(); });
  waiting_ = false;
  if (!got || pendingReplies_.empty()) return std::nullopt;
  proto::Message msg = pendingReplies_.front();
  pendingReplies_.pop_front();
  return msg;
}

bool CommLink::sendAndStream(const std::vector<uint8_t>& wireBytes,
                              const std::function<void(const proto::Message&)>& onLine,
                              int perFrameTimeoutMs, int overallTimeoutMs) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(overallTimeoutMs);

  {
    std::lock_guard<std::mutex> lock(waitMutex_);
    pendingReplies_.clear();
    waiting_ = true;
  }
  if (!transport_.write(wireBytes)) {
    std::lock_guard<std::mutex> lock(waitMutex_);
    waiting_ = false;
    return false;
  }

  while (std::chrono::steady_clock::now() < deadline) {
    std::unique_lock<std::mutex> lock(waitMutex_);
    bool got = waitCv_.wait_for(lock, std::chrono::milliseconds(perFrameTimeoutMs),
                                 [this] { return !pendingReplies_.empty(); });
    if (!got) continue;  // no frame yet this slice - keep trying until overallTimeoutMs
    proto::Message msg = pendingReplies_.front();
    pendingReplies_.pop_front();
    lock.unlock();

    if (msg.type != MSG_TYPE_DATA_REPORT) continue;  // ignore anything unexpected
    if (msg.fields.empty()) {
      // Empty DATA_REPORT: the terminator (see comm_frame.h).
      std::lock_guard<std::mutex> lock2(waitMutex_);
      waiting_ = false;
      return true;
    }
    onLine(msg);
  }

  std::lock_guard<std::mutex> lock(waitMutex_);
  waiting_ = false;
  return false;  // overall timeout without a terminator
}

}  // namespace submarine

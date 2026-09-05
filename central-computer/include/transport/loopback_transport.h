#ifndef CENTRAL_COMPUTER_LOOPBACK_TRANSPORT_H
#define CENTRAL_COMPUTER_LOOPBACK_TRANSPORT_H

#include <deque>
#include <mutex>

#include "transport/itransport.h"

namespace submarine {

// An in-memory, no-hardware transport for host-side testing: bytes written
// via write() land in an internal queue that a test (or a stand-in fake
// LNC) can inspect with takeWritten(), and bytes queued via feedIncoming()
// are what readSome() returns. This is what lets CommLink, ManagementCommand
// and CentralComputer be exercised end-to-end - including realistic
// multi-frame exchanges - without a real board or COM port, the same way
// the LNC firmware's protocol code was verified by host-compiled tests
// rather than only by hand review.
class LoopbackTransport : public ITransport {
 public:
  bool open() override {
    open_ = true;
    return true;
  }

  void close() override { open_ = false; }

  bool isOpen() const override { return open_; }

  bool write(const std::vector<uint8_t>& data) override {
    if (!open_) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    written_.insert(written_.end(), data.begin(), data.end());
    return true;
  }

  std::vector<uint8_t> readSome(std::size_t maxBytes, int /*timeoutMs*/) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> out;
    while (!incoming_.empty() && out.size() < maxBytes) {
      out.push_back(incoming_.front());
      incoming_.pop_front();
    }
    return out;
  }

  std::string errorMessage() const override { return ""; }

  // Test/fake-LNC helpers - not part of ITransport.
  void feedIncoming(const std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    incoming_.insert(incoming_.end(), bytes.begin(), bytes.end());
  }

  std::vector<uint8_t> takeWritten() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> out(written_.begin(), written_.end());
    written_.clear();
    return out;
  }

 private:
  bool open_ = false;
  std::mutex mutex_;
  std::deque<uint8_t> written_;
  std::deque<uint8_t> incoming_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_LOOPBACK_TRANSPORT_H

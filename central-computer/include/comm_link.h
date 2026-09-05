#ifndef CENTRAL_COMPUTER_COMM_LINK_H
#define CENTRAL_COMPUTER_COMM_LINK_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

#include "protocol_bridge.h"
#include "transport/itransport.h"

namespace submarine {

// Communication Module (LNC-facing) - spec section 3.1: "Implements the
// protocol for sending and receiving messages to and from the LNC end
// unit. Implements a listener that forwards received messages to the
// appropriate modules. Communicates with the LNC end unit over UART or
// Ethernet; the module is built so the transport can be changed with
// minimal impact elsewhere" - CommLink depends only on ITransport, never on
// SerialTransport directly, satisfying that last point.
//
// Frame-type semantics (matches app_command.c / task_event.c /
// task_keepalive.c on the firmware side):
//   - MSG_TYPE_ACK and MSG_TYPE_DATA_REPORT are always *replies* to a
//     command this side just sent - they are only ever delivered to
//     whichever call is currently waiting for one (sendAndWait /
//     sendAndStream). One at a time: this class assumes a single command
//     is ever in flight, which matches the Management Command module's use
//     of it.
//   - MSG_TYPE_KEEPALIVE and MSG_TYPE_EVENT are always unsolicited pushes
//     from the LNC and are always delivered to the onUnsolicited callback,
//     regardless of whether a command reply is also pending.
class CommLink {
 public:
  using MessageHandler = std::function<void(const proto::Message&)>;

  CommLink(ITransport& transport, MessageHandler onUnsolicited);
  ~CommLink();

  CommLink(const CommLink&) = delete;
  CommLink& operator=(const CommLink&) = delete;

  // Starts the background reader thread. transport must already be open().
  void start();
  void stop();

  // Sends an already-encoded frame (see proto::MessageBuilder::encode) with
  // no reply expected (e.g. none of this project's commands are fire-and-
  // forget today, but the primitive is here for completeness/testing).
  bool send(const std::vector<uint8_t>& wireBytes);

  // Sends wireBytes, then waits up to timeoutMs for a single reply frame.
  // Returns std::nullopt on timeout or a transport error.
  std::optional<proto::Message> sendAndWait(const std::vector<uint8_t>& wireBytes,
                                             int timeoutMs);

  // Sends wireBytes, then repeatedly waits for MSG_TYPE_DATA_REPORT frames
  // (per comm_frame.h: "one result per frame, terminated by an empty
  // DATA_REPORT frame"), invoking onLine for every non-empty one. Returns
  // true if the terminator was seen before overallTimeoutMs elapsed.
  bool sendAndStream(const std::vector<uint8_t>& wireBytes,
                      const std::function<void(const proto::Message&)>& onLine,
                      int perFrameTimeoutMs, int overallTimeoutMs);

 private:
  void readerLoop();
  bool isReplyType(uint8_t type) const;

  ITransport& transport_;
  MessageHandler onUnsolicited_;
  proto::MessageDecoder decoder_;
  std::thread reader_;
  std::atomic<bool> running_{false};

  std::mutex waitMutex_;
  std::condition_variable waitCv_;
  bool waiting_ = false;
  std::deque<proto::Message> pendingReplies_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_COMM_LINK_H

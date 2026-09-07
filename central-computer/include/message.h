#ifndef CENTRAL_COMPUTER_MESSAGE_H
#define CENTRAL_COMPUTER_MESSAGE_H

#include <string>

namespace submarine {

// Per spec: "Whenever a submarine receives a message, the system must
// store the message content together with a reference to the submarine
// that sent it, so that the sender of each message can be identified."
// The "reference" is realized as the sender's serial number rather than a
// raw pointer: messages outlive the sending call and Fleet owns every
// Submarine's lifetime via unique_ptr, so a pointer could dangle if a
// submarine were ever removed - a serial number stays valid and resolvable
// via Fleet::findBySerial for as long as the message itself is kept.
class Message {
 public:
  Message(std::string senderSerial, std::string content)
      : senderSerial_(std::move(senderSerial)), content_(std::move(content)) {}

  const std::string& senderSerial() const { return senderSerial_; }
  const std::string& content() const { return content_; }

 private:
  std::string senderSerial_;
  std::string content_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_MESSAGE_H

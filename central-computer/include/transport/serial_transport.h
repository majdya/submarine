#ifndef CENTRAL_COMPUTER_SERIAL_TRANSPORT_H
#define CENTRAL_COMPUTER_SERIAL_TRANSPORT_H

#include "transport/itransport.h"

namespace submarine {

// Real hardware transport: a serial port to the LNC end unit, matching the
// spec's "Communication between the Central Computer and the LNC end unit
// is over UART" (section 1.2). Cross-platform by design (per the project's
// build requirement): POSIX termios on Linux/macOS, Win32 CreateFile/COMM
// APIs on Windows - see serial_transport.cpp. The port name is platform
// native: "/dev/ttyACM0" style on Linux, "COM8" (or "\\\\.\\COM8" for
// COM10+) on Windows.
class SerialTransport : public ITransport {
 public:
  SerialTransport(std::string portName, int baudRate);
  ~SerialTransport() override;

  SerialTransport(const SerialTransport&) = delete;
  SerialTransport& operator=(const SerialTransport&) = delete;

  bool open() override;
  void close() override;
  bool isOpen() const override;
  bool write(const std::vector<uint8_t>& data) override;
  std::vector<uint8_t> readSome(std::size_t maxBytes, int timeoutMs) override;
  std::string errorMessage() const override;

 private:
  std::string portName_;
  int baudRate_;
  std::string lastError_;
#if defined(_WIN32)
  void* handle_ = nullptr;  // HANDLE, stored as void* to keep windows.h out of this header
#else
  int fd_ = -1;
#endif
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_SERIAL_TRANSPORT_H

#include "transport/serial_transport.h"

#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace submarine {

SerialTransport::SerialTransport(std::string portName, int baudRate)
    : portName_(std::move(portName)), baudRate_(baudRate) {}

SerialTransport::~SerialTransport() { close(); }

#if defined(_WIN32)

namespace {
// Windows wants "\\.\COM8" (not just "COM8") for port numbers >= 10; the
// prefixed form also works fine for single-digit ports, so always use it.
std::string toWinPath(const std::string& portName) {
  return "\\\\.\\" + portName;
}

DWORD baudConstant(int baud) {
  switch (baud) {
    case 9600: return CBR_9600;
    case 19200: return CBR_19200;
    case 38400: return CBR_38400;
    case 57600: return CBR_57600;
    case 115200: return CBR_115200;
    default: return static_cast<DWORD>(baud);  // Windows accepts arbitrary rates too
  }
}
}  // namespace

bool SerialTransport::open() {
  close();
  std::string path = toWinPath(portName_);
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    lastError_ = "CreateFile failed for " + portName_ + " (error " + std::to_string(GetLastError()) + ")";
    return false;
  }

  DCB dcb{};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb)) {
    lastError_ = "GetCommState failed";
    CloseHandle(h);
    return false;
  }
  dcb.BaudRate = baudConstant(baudRate_);
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  if (!SetCommState(h, &dcb)) {
    lastError_ = "SetCommState failed";
    CloseHandle(h);
    return false;
  }

  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 1000;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  SetCommTimeouts(h, &timeouts);

  handle_ = h;
  return true;
}

void SerialTransport::close() {
  if (handle_) {
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
}

bool SerialTransport::isOpen() const { return handle_ != nullptr; }

bool SerialTransport::write(const std::vector<uint8_t>& data) {
  if (!handle_) return false;
  DWORD written = 0;
  BOOL ok = WriteFile(static_cast<HANDLE>(handle_), data.data(),
                       static_cast<DWORD>(data.size()), &written, nullptr);
  if (!ok || written != data.size()) {
    lastError_ = "WriteFile failed (error " + std::to_string(GetLastError()) + ")";
    return false;
  }
  return true;
}

std::vector<uint8_t> SerialTransport::readSome(std::size_t maxBytes, int timeoutMs) {
  std::vector<uint8_t> out;
  if (!handle_) return out;

  COMMTIMEOUTS timeouts{};
  GetCommTimeouts(static_cast<HANDLE>(handle_), &timeouts);
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
  timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(timeoutMs > 0 ? timeoutMs : 1);
  SetCommTimeouts(static_cast<HANDLE>(handle_), &timeouts);

  out.resize(maxBytes);
  DWORD readBytes = 0;
  BOOL ok = ReadFile(static_cast<HANDLE>(handle_), out.data(),
                      static_cast<DWORD>(maxBytes), &readBytes, nullptr);
  if (!ok) {
    lastError_ = "ReadFile failed (error " + std::to_string(GetLastError()) + ")";
    out.clear();
    return out;
  }
  out.resize(readBytes);
  return out;
}

#else  // POSIX (Linux/macOS)

namespace {
speed_t baudConstant(int baud) {
  switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    default: return B115200;
  }
}
}  // namespace

bool SerialTransport::open() {
  close();
  int fd = ::open(portName_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    lastError_ = "open() failed for " + portName_ + ": " + std::strerror(errno);
    return false;
  }

  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    lastError_ = std::string("tcgetattr failed: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }

  cfmakeraw(&tty);
  speed_t baud = baudConstant(baudRate_);
  cfsetispeed(&tty, baud);
  cfsetospeed(&tty, baud);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;  // reads are polled with poll(); see readSome

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    lastError_ = std::string("tcsetattr failed: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }

  fd_ = fd;
  return true;
}

void SerialTransport::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialTransport::isOpen() const { return fd_ >= 0; }

bool SerialTransport::write(const std::vector<uint8_t>& data) {
  if (fd_ < 0) return false;
  std::size_t total = 0;
  while (total < data.size()) {
    ssize_t n = ::write(fd_, data.data() + total, data.size() - total);
    if (n < 0) {
      if (errno == EINTR) continue;
      lastError_ = std::string("write() failed: ") + std::strerror(errno);
      return false;
    }
    total += static_cast<std::size_t>(n);
  }
  return true;
}

std::vector<uint8_t> SerialTransport::readSome(std::size_t maxBytes, int timeoutMs) {
  std::vector<uint8_t> out;
  if (fd_ < 0) return out;

  pollfd pfd{fd_, POLLIN, 0};
  int rc = ::poll(&pfd, 1, timeoutMs);
  if (rc <= 0) {
    return out;  // timeout (rc==0) or a poll() error we treat as "nothing available"
  }

  out.resize(maxBytes);
  ssize_t n = ::read(fd_, out.data(), maxBytes);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      out.clear();
      return out;
    }
    lastError_ = std::string("read() failed: ") + std::strerror(errno);
    out.clear();
    return out;
  }
  out.resize(static_cast<std::size_t>(n));
  return out;
}

#endif

std::string SerialTransport::errorMessage() const { return lastError_; }

}  // namespace submarine

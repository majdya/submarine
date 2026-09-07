#ifndef CENTRAL_COMPUTER_ITRANSPORT_H
#define CENTRAL_COMPUTER_ITRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace submarine {

// Per the spec's own note (section 1.2): "The physical transport (UART /
// Ethernet) is a configuration detail of the communication module, not a
// structural assumption elsewhere in the system." ITransport is that
// boundary - CommLink (comm_link.h) talks only to this interface, never to
// a concrete serial port or socket directly, so the underlying transport
// can be swapped (real COM port today, a future Ethernet link, or a
// deterministic in-memory loopback for host-side testing) with zero change
// to anything above it.
class ITransport {
 public:
  virtual ~ITransport() = default;

  // Opens the transport. Returns true on success; on failure, errorMessage()
  // holds a human-readable reason.
  virtual bool open() = 0;

  virtual void close() = 0;

  virtual bool isOpen() const = 0;

  // Writes exactly data.size() bytes, blocking until sent or an error
  // occurs. Returns false on any write error (isOpen() becomes false).
  virtual bool write(const std::vector<uint8_t>& data) = 0;

  // Reads up to maxBytes bytes, blocking for at most timeoutMs
  // milliseconds. Returns the bytes actually read - may be empty on a
  // timeout, which is not an error. Returns an empty vector and leaves
  // isOpen() false only on a genuine I/O error.
  virtual std::vector<uint8_t> readSome(std::size_t maxBytes, int timeoutMs) = 0;

  virtual std::string errorMessage() const = 0;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_ITRANSPORT_H

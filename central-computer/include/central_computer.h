#ifndef CENTRAL_COMPUTER_CENTRAL_COMPUTER_H
#define CENTRAL_COMPUTER_CENTRAL_COMPUTER_H

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "comm_link.h"
#include "data_collection.h"
#include "data_store.h"
#include "log_module.h"
#include "management_command.h"
#include "transport/itransport.h"
#include "transport/loopback_transport.h"

namespace submarine {

// Spec section 3 / OOP Part: "Each submarine has a central computer that
// works with the submarine's sensors and receives information from them.
// The central computer itself is defined in the first part of this
// document, but it should be treated as an object that belongs to each
// combat submarine." This class is that object: a thin composition root
// over the four section-3 modules (kept as separate classes/files - see
// comm_link.h, management_command.h, log_module.h, data_collection.h - so
// any of them could be split into its own process/binary later with no
// change to the others, per the project's "keep modules modular for easy
// separation" requirement).
//
// A CentralComputer is never transport-less: one not wired to real
// hardware yet (most CombatSubmarines in a fleet demo, since there is only
// one physical LNC/COM port available) defaults to a LoopbackTransport,
// which simply never produces or accepts any real data - isConnected()
// reports false for it until/unless something feeds it, same as a real
// port that hasn't been opened. Only the one CombatSubmarine actually
// wired to the lab hardware is constructed with a real SerialTransport.
// The most recent KEEPALIVE the Communication module received, kept for
// live display (the web dashboard - see dashboard_api.h) rather than
// requiring a reader to go back through DataStore's persisted history for
// "what is this submarine doing right now".
struct LiveSnapshot {
  bool hasData = false;
  std::string receivedAtHms;
  uint8_t mode = 0;
  uint32_t lightRaw = 0, tempAdcRaw = 0, batteryRaw = 0;
  uint8_t dhtTemp = 0, dhtHumidity = 0, dhtValid = 0;
  std::string lastEventDescription;  // e.g. "OBJECT_DETECTED", empty if none seen yet
  std::string lastEventAtHms;
};

class CentralComputer {
 public:
  explicit CentralComputer(std::string logDir, std::string dataDir,
                            std::unique_ptr<ITransport> transport = std::make_unique<LoopbackTransport>());
  ~CentralComputer();

  CentralComputer(const CentralComputer&) = delete;
  CentralComputer& operator=(const CentralComputer&) = delete;

  // Opens the transport and starts the Communication module's reader
  // thread. Returns false (see transport().errorMessage()) if the
  // transport failed to open - e.g. the named COM port doesn't exist.
  bool connect();
  void disconnect();
  bool isConnected() const;

  ITransport& transport() { return *transport_; }

  ManagementCommand& commands() { return managementCommand_; }
  LogModule& log() { return logModule_; }
  DataCollection& dataCollection() { return dataCollection_; }

  void enforceLogRetention(uint32_t todayYmd) { logModule_.enforceRetention(todayYmd); }

  // Thread-safe: safe to call from a web dashboard thread while CommLink's
  // reader thread is concurrently updating it.
  LiveSnapshot latestSnapshot() const;

 private:
  void onUnsolicited(const proto::Message& msg);

  std::unique_ptr<ITransport> transport_;
  CommLink commLink_;
  DataStore dataStore_;
  LogModule logModule_;
  DataCollection dataCollection_;
  ManagementCommand managementCommand_;

  mutable std::mutex snapshotMutex_;
  LiveSnapshot snapshot_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_CENTRAL_COMPUTER_H

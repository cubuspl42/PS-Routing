#pragma once
#include "Entry.h"

#include <netinet/ip.h>

#include <mutex>
#include <thread>
#include <vector>

struct EnabledInterface {
  in_addr addr;
  uint8_t addr_len;
  int oif;
};

class Service {
public:
  Service(std::vector<EnabledInterface> enabledInterfaces,
          std::vector<Entry> directRoutes);
  void join();

private:
  int findInterfaceByIp(struct in_addr addr);
  void broadcastRoute(Entry entry);
  void broadcastRoutingTable();
  void recvLoop();
  void broadcastLoop();
  int findMetricByDst(in_addr dst);
  void handleReceivedEntry(Entry entry);
  void replaceEntry(in_addr dst, Entry newEntry);

  std::mutex mutex;

  std::thread recvThread;
  std::thread broadcastThread;

  int sfd;

  std::vector<EnabledInterface> enabledInterfaces;
  std::vector<Entry> routingTable;
};

#pragma once
#include <netinet/ip.h>

#include <vector>
#include <thread>

struct EnabledInterface {
  in_addr addr;
  uint8_t addr_len;
  int oif;
};

struct Entry {
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
  int oif;
  int metric;
};

struct RtMessage {
  uint16_t msg_type;
  uint16_t flags;
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
  int oif;
  uint8_t protocol;
  uint8_t scope;
  uint8_t type;
};

class NetlinkRouteSocket {
public:
  std::vector<RtMessage> getRoutes();
  std::vector<Entry> getRoutes_();
  void setRoute(Entry entry);
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

  std::thread recvThread;
  std::thread broadcastThread;

  int sfd;

  std::vector<EnabledInterface> enabledInterfaces;
  std::vector<Entry> routingTable;
};

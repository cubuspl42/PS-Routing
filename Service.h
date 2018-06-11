#pragma once
#include <netinet/ip.h>

#include <vector>

struct Entry {
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
};

struct RtMessage {
  uint16_t msg_type;
  uint16_t flags;
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
  int oif;
  uint8_t protocol; /* Routing protocol; see below */
  uint8_t scope;    /* See below */
  uint8_t type;
};

class NetlinkRouteSocket {
public:
  std::vector<RtMessage> getRoutes();
  std::vector<Entry> getRoutes_();
  void setRoutes(std::vector<Entry> &routes);
};

class Service {
public:
  Service();
  void run();

private:
  void broadcastRoute(Entry entry);
  void broadcastRoutingTable();

  int sfd;
  int nfd;
};

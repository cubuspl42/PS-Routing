#pragma once
#include "Entry.h"

#include <vector>

struct RtMessage {
  uint16_t msg_type;
  uint16_t flags;
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
  int oif;
  int metric;
  uint8_t protocol;
  uint8_t scope;
  uint8_t type;
};

class NetlinkRouteSocket {
public:
  std::vector<RtMessage> getRoutes();
  void setRoute(Entry entry);
};

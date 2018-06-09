#include <netinet/ip.h>

#include <vector>

struct Entry {
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
};

class NetlinkRouteSocket {
public:
  std::vector<Entry> getRoutes();
};

class Service {
public:
  Service();
  void run();

private:
  void broadcastRoutingTable();

  int sfd;
  int nfd;
};

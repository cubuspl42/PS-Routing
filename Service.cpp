#include "Service.h"

#include "NetlinkRouteSocket.h"
#include "utils.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

static const int port = 1234;

Service::Service(std::vector<EnabledInterface> enabledInterfaces,
                 std::vector<Entry> directRoutes) {
  if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    throw std::runtime_error("socket [UDP]");
  }

  int broadcastEnable = 1;
  if (setsockopt(sfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable,
                 sizeof broadcastEnable) != 0) {
    throw std::runtime_error("setsockopt");
  }

  sockaddr_in si_me{};
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (sockaddr *)&si_me, sizeof si_me) == -1) {
    throw std::runtime_error("bind");
  }

  this->enabledInterfaces = enabledInterfaces;
  this->routingTable = directRoutes;

  this->recvThread = std::thread{[=]() { recvLoop(); }};
  this->broadcastThread = std::thread{[=]() { broadcastLoop(); }};
}

void Service::recvLoop() {
  while (true) {
    struct sockaddr_in sender {};
    Entry entry;

    socklen_t sendsize = sizeof sender;

    int len = recvfrom(sfd, &entry, sizeof entry, 0, (struct sockaddr *)&sender,
                       &sendsize);
    if (len != sizeof entry) {
      throw std::runtime_error("recvfrom");
    }

    entry.gateway = sender.sin_addr;
    entry.oif = findInterfaceByIp(sender.sin_addr);
    entry.metric++;

    handleReceivedEntry(entry);
  }
}

void Service::handleReceivedEntry(Entry entry) {
  std::lock_guard<std::mutex> lock{mutex};
  std::cerr << "Received entry: " << to_string(entry.dst) << "/"
            << (int)entry.dst_len << " via " << to_string(entry.gateway)
            << std::endl;

  int oldMetric = findMetricByDst(entry.dst);

  std::cerr << "old metric: " << oldMetric << " new metric: " << entry.metric
            << std::endl;

  if (entry.metric < oldMetric) {
    replaceEntry(entry.dst, entry);
    NetlinkRouteSocket nls;
    nls.setRoute(entry);
  }
}

static bool isInSubnet(struct in_addr addr, struct in_addr net,
                       uint8_t net_len) {
  std::cerr << "isInSubnet(" << to_string(addr) << ", " << to_string(net)
            << ", " << (int)net_len << ")" << std::endl;
  uint32_t addrh = ntohl(addr.s_addr);
  uint32_t neth = ntohl(net.s_addr);
  uint32_t mask = ~((1 << (32 - net_len)) - 1);
  std::cerr << mask << " " << addrh << " " << neth << std::endl;
  // int n = (32 - net_len);
  // bool rv = (addrh >> n) == (neth >> n);
  bool rv = (addrh & mask) == (neth & mask);
  std::cerr << "rv == " << rv << std::endl;
  return rv;
}

int Service::findInterfaceByIp(struct in_addr addr) {
  for (auto iface : enabledInterfaces) {
    if (isInSubnet(addr, iface.addr, iface.addr_len)) {
      return iface.oif;
    }
  }
  throw std::runtime_error("no such interface");
}

void Service::broadcastLoop() {
  while (true) {
    broadcastRoutingTable();
    std::this_thread::sleep_for(30s);
  }
}

static in_addr broadcastAddress(in_addr net, uint8_t net_len) {
  uint32_t neth = ntohl(net.s_addr);
  uint32_t mask = (1 << (32 - net_len)) - 1;
  uint32_t rvh = neth | mask;
  return in_addr{htonl(rvh)};
}

void Service::broadcastRoute(Entry entry) {
  std::cerr << "Service::broadcastRoute" << std::endl;

  for (auto iface : enabledInterfaces) {
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = broadcastAddress(iface.addr, iface.addr_len);

    if (sendto(sfd, &entry, sizeof entry, 0, (struct sockaddr *)&addr,
               sizeof addr) < 0)
      throw std::runtime_error("sendto");
  }
}

void Service::broadcastRoutingTable() {
  std::lock_guard<std::mutex> lock{mutex};
  std::cerr << "Broadcasting routing table..." << std::endl;
  for (auto entry : routingTable) {
    broadcastRoute(entry);
  }
}

void Service::join() {
  recvThread.join();
  broadcastThread.join();
}

int Service::findMetricByDst(in_addr dst) {
  for (auto entry : routingTable) {
    if (entry.dst == dst)
      return entry.metric;
  }
  return std::numeric_limits<int>::max();
}

void Service::replaceEntry(in_addr dst, Entry newEntry) {
  auto &r = routingTable;
  auto it =
      std::find_if(r.begin(), r.end(), [=](Entry e) { return e.dst == dst; });
  if (it != r.end())
    it = r.erase(it);
  r.insert(it, newEntry);
}

#include "Service.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace std::string_literals;

static const int port = 1234;

const struct in_addr in_addr_any = {INADDR_ANY};

bool operator==(in_addr a, in_addr b) { return a.s_addr == b.s_addr; }

bool operator!=(in_addr a, in_addr b) { return !(a == b); }

struct Message {
  nlmsghdr header;
  rtmsg body;
};

struct RtError {
  struct nlmsghdr nlh;
  struct nlmsgerr nle;
};

struct RtaInaddr {
  struct rtattr rta;
  in_addr ina;
};

static_assert(sizeof(RtaInaddr) == sizeof(struct rtattr) + 4);

struct RtaInt {
  struct rtattr rta;
  int i;
};

static_assert(sizeof(RtaInt) == sizeof(struct rtattr) + 4);

struct RtResponseHeader {
  struct nlmsghdr nlh;
  struct rtmsg rtm;
};

struct RtRequest {
  struct nlmsghdr nlh;
  struct rtmsg rtm;
  struct RtaInaddr rta_dst;
  struct RtaInaddr rta_gateway;
  struct RtaInt rta_oif;
};

// static_assert(sizeof(RtRequest) == NLMSG_LENGTH(sizeof
// (RtRequest)));

class RoutingTable {
public:
  Entry getEntryByDst(uint32_t dst) { return Entry{}; }

private:
  std::map<uint32_t, Entry> _entries;
};

static void handle_message(const nlmsghdr *nh, std::vector<Entry> &rv) {
  rtmsg *rtp = (rtmsg *)NLMSG_DATA(nh);

  if (rtp->rtm_table != RT_TABLE_MAIN)
    return;

  Entry entry;
  entry.dst_len = rtp->rtm_dst_len;

  int atlen = RTM_PAYLOAD(nh);
  for (rtattr *atp = (rtattr *)RTM_RTA(rtp); RTA_OK(atp, atlen);
       atp = RTA_NEXT(atp, atlen)) {
    switch (atp->rta_type) {
    case RTA_DST:
    case RTA_GATEWAY: {
      struct in_addr *addr = (struct in_addr *)RTA_DATA(atp);
      if (atp->rta_type == RTA_DST) {
        entry.dst = *addr;
      } else {
        entry.gateway = *addr;
      }
      break;
    }
    case RTA_OIF:
      break;
    }
  }

  rv.push_back(entry);
}

static std::vector<Entry> handle_response(int sfd) {
  std::vector<Entry> rv;
  while (true) {
    char buf[8192] = {};
    int len = recv(sfd, buf, sizeof buf, 0);

    for (nlmsghdr *nh = (nlmsghdr *)buf; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {
      if (nh->nlmsg_type == NLMSG_DONE)
        return rv;
      // print_nlp(nh);
      handle_message(nh, rv);
    }
  }
}

RtMessage repack_rt_message(const RtResponseHeader &rth) {
  const struct rtmsg &rtm = rth.rtm;

  RtMessage msg;
  msg.dst_len = rtm.rtm_dst_len;

  int len = RTM_PAYLOAD(&rth.nlh);
  for (rtattr *rta = (rtattr *)RTM_RTA(&rtm); RTA_OK(rta, len);
       rta = RTA_NEXT(rta, len)) {
    std::cerr << "rta_type: " << rta->rta_type << std::endl;
    switch (rta->rta_type) {
    case RTA_DST:
      msg.dst = ((RtaInaddr *)rta)->ina;
      break;
    case RTA_GATEWAY:
      msg.gateway = ((RtaInaddr *)rta)->ina;
      break;
    case RTA_OIF:
      msg.oif = ((RtaInt *)rta)->i;
      break;
    }
  }

  return msg;
}

std::vector<RtMessage> recv_rt_response(int sfd) {
  std::vector<RtMessage> rv;

  std::cout << "recv_rt_response" << std::endl;

  while (true) {
    char buf[8192] = {};
    int len = recv(sfd, buf, sizeof buf, 0);
    std::cout << "len: " << len << std::endl;

    for (nlmsghdr *nlh = (nlmsghdr *)buf; NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

      std::cout << "nlmsg_type: " << nlh->nlmsg_type << " ";
      switch (nlh->nlmsg_type) {
      case NLMSG_DONE:
        std::cout << "NLMSG_DONE";
        break;
      case RTM_NEWROUTE:
        std::cout << "RTM_NEWROUTE";
        break;
      case NLMSG_ERROR:
        std::cout << "NLMSG_ERROR";
        break;
      default:
        std::cout << "<other>";
        break;
      }
      std::cout << std::endl;

      if (nlh->nlmsg_type == NLMSG_DONE)
        return rv;

      if (nlh->nlmsg_type == NLMSG_ERROR) {
        RtError *err = (RtError *)nlh;
        if (err->nle.error == 0)
          return rv;
        else {
          char buf[1024] = {};
          char *ptr = strerror_r(-err->nle.error, buf, sizeof buf);
          throw std::runtime_error("netlink error: "s + std::string{ptr});
        }
      }

      if (nlh->nlmsg_type != RTM_NEWROUTE)
        throw std::runtime_error("unexpected nlmsg_type");

      const RtResponseHeader *rth = (RtResponseHeader *)nlh;
      rv.push_back(repack_rt_message(*rth));
    }
  }

  return rv;
}

std::vector<RtMessage> send_rt_request(const RtMessage &msg) {
  std::cerr << "send_rt_request" << std::endl;
  // std::cout << sizeof(RtRequest) << ":" << NLMSG_LENGTH(sizeof(RtRequest));
  // std::cout << std::endl;

  //   struct RtResponseHeader {
  //   struct nlmsghdr nlh;
  //   struct rtmsg rtm;
  // };

  // struct RtRequest {
  //   RtResponseHeader rth;
  //   struct RtaInaddr rta_dst;
  //   struct RtaInaddr rta_gateway;
  //   struct RtaInt rta_oif;
  // };
  // printf("nlmsghdr: %lu, rtmsg: %lu, RtaInaddr: %lu, RtaInt: %lu, "
  //        "RtRequest: %lu\n",
  //        sizeof(struct nlmsghdr), sizeof(struct rtmsg), sizeof(RtaInaddr),
  //        sizeof(RtaInt), sizeof(RtRequest));

  int sfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE); // TODO: error handling

  RtRequest req{};

  auto &nlh = req.nlh;
  nlh.nlmsg_len = sizeof(RtRequest);
  nlh.nlmsg_type = msg.msg_type;
  nlh.nlmsg_flags = msg.flags;
  nlh.nlmsg_seq = 0;
  nlh.nlmsg_pid = getpid();

  auto &rtm = req.rtm;
  rtm.rtm_family = AF_INET;
  rtm.rtm_dst_len = msg.dst_len;
  rtm.rtm_table = RT_TABLE_MAIN;
  rtm.rtm_protocol = msg.protocol;
  rtm.rtm_scope = msg.scope;
  rtm.rtm_type = msg.type;

  req.rta_dst.rta.rta_type = RTA_DST;
  req.rta_dst.rta.rta_len = sizeof req.rta_dst;
  req.rta_dst.ina = msg.dst;

  req.rta_gateway.rta.rta_type = RTA_GATEWAY;
  req.rta_gateway.rta.rta_len = sizeof req.rta_gateway;
  req.rta_gateway.ina = msg.gateway;

  req.rta_oif.rta.rta_type = RTA_OIF;
  req.rta_oif.rta.rta_len = sizeof req.rta_oif;
  req.rta_oif.i = msg.oif;

  struct sockaddr_nl snl {};
  snl.nl_family = AF_NETLINK;
  snl.nl_pid = 0;

  if (sendto(sfd, (void *)&req, req.nlh.nlmsg_len, 0, (sockaddr *)&snl,
             sizeof snl) < 0)
    throw std::runtime_error("sendto");

  auto rv = recv_rt_response(sfd);

  close(sfd);

  return rv;
}

// void NetlinkRouteSocket::deleteRoute() {}

std::vector<Entry> NetlinkRouteSocket::getRoutes_() {
  int sfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

  struct sockaddr_nl snl {};
  snl.nl_family = AF_NETLINK;
  snl.nl_pid = 0;

  struct Message msg {};
  msg.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  msg.header.nlmsg_type = RTM_GETROUTE;
  msg.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
  msg.header.nlmsg_seq = 0;
  msg.header.nlmsg_pid = getpid();
  msg.body.rtm_family = AF_INET;
  msg.body.rtm_table = RT_TABLE_MAIN;

  sendto(sfd, (void *)&msg, sizeof msg, 0, (sockaddr *)&snl, sizeof snl);

  auto rv = handle_response(sfd);

  close(sfd);

  return rv;
}

std::vector<RtMessage> NetlinkRouteSocket::getRoutes() {
  RtMessage msg;
  msg.msg_type = RTM_GETROUTE;
  msg.flags = NLM_F_REQUEST | NLM_F_ROOT;

  return send_rt_request(msg);
}

inline std::string to_string(in_addr addr) {
  char buf[32] = {};
  inet_ntop(AF_INET, &addr, buf, sizeof buf);
  return std::string{buf};
}

void NetlinkRouteSocket::setRoute(Entry entry) {
  std::cerr << "NetlinkRouteSocket::setRoute " << to_string(entry.dst)
            << " via " << to_string(entry.gateway) << " dev " << entry.oif
            << std::endl;

  RtMessage msg;
  msg.msg_type = RTM_NEWROUTE;
  msg.flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_ACK;
  msg.dst = entry.dst;
  msg.dst_len = entry.dst_len;
  msg.gateway = entry.gateway;
  msg.oif = entry.oif;
  msg.protocol = RTPROT_STATIC;
  msg.scope = RT_SCOPE_UNIVERSE;
  msg.type = RTN_UNICAST;

  auto rv = send_rt_request(msg);
}

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

    std::cerr << "Received entry from " << to_string(sender.sin_addr)
              << std::endl;

    entry.gateway = sender.sin_addr;
    entry.oif = findInterfaceByIp(sender.sin_addr);

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

void Service::broadcastRoute(Entry entry) {
  std::cerr << "Service::broadcastRoute" << std::endl;

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  if (sendto(sfd, &entry, sizeof entry, 0, (struct sockaddr *)&addr,
             sizeof addr) < 0)
    throw std::runtime_error("sendto");
}

void Service::broadcastRoutingTable() {
  std::cerr << "Broadcasting routing table..." << std::endl;
  for (auto entry : routingTable) {
    broadcastRoute(entry);
  }
}

void Service::join() {
  recvThread.join();
  broadcastThread.join();
}

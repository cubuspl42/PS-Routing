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

static const int port = 1234;

struct Message {
  struct nlmsghdr header;
  struct rtmsg body;
};

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
    std::cout << "len: " << len << std::endl;

    for (nlmsghdr *nh = (nlmsghdr *)buf; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {
      if (nh->nlmsg_type == NLMSG_DONE)
        return rv;
      // print_nlp(nh);
      handle_message(nh, rv);
    }
  }
}

std::vector<Entry> NetlinkRouteSocket::getRoutes() {
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

Service::Service() {
  if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    throw std::runtime_error("socket [UDP]");
  }

  sockaddr_in si_me{};
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (sockaddr *)&si_me, sizeof si_me) == -1) {
    throw std::runtime_error("bind");
  }
}

void Service::run() {
  while (true) {
    broadcastRoutingTable();
    std::this_thread::sleep_for(30s);
  }
}

void Service::broadcastRoutingTable() {}

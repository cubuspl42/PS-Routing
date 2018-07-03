#include "NetlinkRouteSocket.h"

#include "utils.h"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

using namespace std::string_literals;

const struct in_addr in_addr_any = {INADDR_ANY};

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
  struct RtaInt rta_metric;
  struct RtaInt rta_oif;
};

static RtMessage repack_rt_message(const RtResponseHeader &rth) {
  const struct rtmsg &rtm = rth.rtm;

  RtMessage msg;
  msg.dst_len = rtm.rtm_dst_len;

  int len = RTM_PAYLOAD(&rth.nlh);
  for (rtattr *rta = (rtattr *)RTM_RTA(&rtm); RTA_OK(rta, len);
       rta = RTA_NEXT(rta, len)) {
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
    case RTA_METRICS:
      msg.metric = ((RtaInt *)rta)->i;
      break;
    }
  }

  return msg;
}

static std::vector<RtMessage> recv_rt_response(int sfd) {
  std::vector<RtMessage> rv;

  while (true) {
    char buf[8192] = {};
    int len = recv(sfd, buf, sizeof buf, 0);

    for (nlmsghdr *nlh = (nlmsghdr *)buf; NLMSG_OK(nlh, len);
         nlh = NLMSG_NEXT(nlh, len)) {

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

static std::vector<RtMessage> send_rt_request(const RtMessage &msg) {
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

  req.rta_metric.rta.rta_type = RTA_METRICS;
  req.rta_metric.rta.rta_len = sizeof req.rta_metric;
  req.rta_metric.i = msg.metric;

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

std::vector<RtMessage> NetlinkRouteSocket::getRoutes() {
  RtMessage msg;
  msg.msg_type = RTM_GETROUTE;
  msg.flags = NLM_F_REQUEST | NLM_F_ROOT;

  return send_rt_request(msg);
}

void NetlinkRouteSocket::setRoute(Entry entry) {
  std::cerr << "Setting route: " << to_string(entry.dst) << " via "
            << to_string(entry.gateway) << " dev " << entry.oif << " metric "
            << entry.metric << std::endl;

  RtMessage msg;
  msg.msg_type = RTM_NEWROUTE;
  msg.flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
  msg.dst = entry.dst;
  msg.dst_len = entry.dst_len;
  msg.gateway = entry.gateway;
  msg.oif = entry.oif;
  msg.metric = entry.metric;
  msg.protocol = RTPROT_STATIC;
  msg.scope = RT_SCOPE_UNIVERSE;
  msg.type = RTN_UNICAST;

  auto rv = send_rt_request(msg);
}

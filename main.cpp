#include "Service.h"

#include <cstring>
#include <iostream>
#include <map>

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <sys/socket.h>
// #include <sys/types.h>
#include <unistd.h>

struct Message {
  struct nlmsghdr header;
  struct rtmsg body;
};

void print_nlp(struct nlmsghdr *nlp) {
  std::cout << "nlmsg_type: " << nlp->nlmsg_type << " ";
  switch (nlp->nlmsg_type) {
  case NLM_F_MULTI:
    std::cout << "NLM_F_MULTI";
    break;
  case NLMSG_DONE:
    std::cout << "NLMSG_DONE";
    break;
  default:
    std::cout << "<other>";
    break;
  }
  std::cout << std::endl;

  std::cout << "NLM_F_MULTI: " << !!(nlp->nlmsg_flags & NLM_F_MULTI)
            << std::endl;

  rtmsg *rtp = (rtmsg *)NLMSG_DATA(nlp);
  if (rtp->rtm_table != RT_TABLE_MAIN)
    return;

  int atlen = RTM_PAYLOAD(nlp);

  char dst[32] = {}, msk[32] = {}, gwy[32] = {}, dev[32] = {};

  for (rtattr *atp = (rtattr *)RTM_RTA(rtp); RTA_OK(atp, atlen);
       atp = RTA_NEXT(atp, atlen)) {
    switch (atp->rta_type) {
    case RTA_DST:
      inet_ntop(AF_INET, RTA_DATA(atp), dst, sizeof(dst));
      break;
    case RTA_GATEWAY:
      inet_ntop(AF_INET, RTA_DATA(atp), gwy, sizeof(gwy));
      break;
    case RTA_OIF:
      sprintf(dev, "%d", *((int *)RTA_DATA(atp)));
      break;
    }
  }

  sprintf(msk, "%d", rtp->rtm_dst_len);
  if (strlen(dst) == 0)
    printf("default via %s dev %s\n", gwy, dev);
  else if (strlen(gwy) == 0)
    printf("%s/%s dev %s\n", dst, msk, dev);
  else
    printf("dst %s/%s gwy %s dev %s\n", dst, msk, gwy, dev);
}

void handle_response(int sfd) {
  while (true) {
    char buf[8192] = {};
    int len = recv(sfd, buf, sizeof buf, 0);
    std::cout << "len: " << len << std::endl;

    for (nlmsghdr *nh = (nlmsghdr *)buf; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len)) {
      print_nlp(nh);
      if (nh->nlmsg_type == NLMSG_DONE)
        return;
    }
  }
}

int main_(int argc, char const *argv[]) {
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

  handle_response(sfd);

  close(sfd);

  return 0;
}

std::string to_string(in_addr addr) {
  char buf[32] = {};
  inet_ntop(AF_INET, &addr, buf, sizeof buf);
  return std::string{buf};
}

int main(int argc, char const *argv[]) {
  NetlinkRouteSocket nrs;
  auto routes = nrs.getRoutes();
  for (auto r : routes) {
    std::cout << to_string(r.dst) << "/" << (int)r.dst_len;
    std::cout << " via " << to_string(r.gateway) << std::endl;
  }
}

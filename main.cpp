#include "Service.h"

#include "utils.h"
#include "nlohmann/json.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

using nlohmann::json;

int main(int argc, char const *argv[]) {
  std::ifstream is{argv[1]};
  json configJson;
  is >> configJson;

  std::vector<EnabledInterface> enabledInterfaces;
  for (auto enabledInterfaceJson : configJson["enabledInterfaces"]) {
    EnabledInterface iface;
    iface.addr = pton(enabledInterfaceJson["addr"]);
    iface.addr_len = enabledInterfaceJson["addr_len"];
    iface.oif = (int)enabledInterfaceJson["oif"];
    enabledInterfaces.push_back(iface);
  }

  std::vector<Entry> directRoutes;
  for (auto directRouteJson : configJson["directRoutes"]) {
    Entry directRoute{};
    directRoute.dst = pton(directRouteJson["dst"]);
    directRoute.dst_len = directRouteJson["dst_len"];
    directRoute.oif = directRouteJson["oif"];
    directRoutes.push_back(directRoute);
  }

  std::cerr << "Enabled interfaces:" << std::endl;
  for (auto ei : enabledInterfaces) {
    std::cerr << to_string(ei.addr) << "/" << (int)ei.addr_len << " dev "
              << ei.oif << std::endl;
  }

  Service service{enabledInterfaces, directRoutes};
  service.join();
}

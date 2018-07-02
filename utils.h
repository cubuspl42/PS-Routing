#include <arpa/inet.h>
#include <netinet/ip.h>

#include <string>

inline std::string to_string(in_addr addr) {
  char buf[32] = {};
  inet_ntop(AF_INET, &addr, buf, sizeof buf);
  return std::string{buf};
}

inline bool operator==(in_addr a, in_addr b) { return a.s_addr == b.s_addr; }

inline bool operator!=(in_addr a, in_addr b) { return !(a == b); }

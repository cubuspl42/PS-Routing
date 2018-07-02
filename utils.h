#include <arpa/inet.h>
#include <netinet/ip.h>

#include <stdexcept>
#include <string>

inline in_addr pton(std::string src) {
  in_addr rv;
  if (inet_pton(AF_INET, src.c_str(), &rv) != 1)
    throw std::runtime_error("inet_pton");
  return rv;
}

inline std::string to_string(in_addr addr) {
  char buf[32] = {};
  inet_ntop(AF_INET, &addr, buf, sizeof buf);
  return std::string{buf};
}

inline bool operator==(in_addr a, in_addr b) { return a.s_addr == b.s_addr; }

inline bool operator!=(in_addr a, in_addr b) { return !(a == b); }

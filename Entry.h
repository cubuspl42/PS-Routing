#pragma once
#include <netinet/ip.h>

#include <cstdint>

struct Entry {
  in_addr dst;
  uint8_t dst_len;
  in_addr gateway;
  int oif;
  int metric;
};

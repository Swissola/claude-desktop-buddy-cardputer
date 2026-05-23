#pragma once
// No-op Preferences stub for native (host) unit testing.
// All reads return their default, all writes are discarded.
#include <stdint.h>
#include <string.h>

class Preferences {
public:
  void   begin(const char*, bool) {}
  void   end() {}
  void   clear() {}

  uint32_t getUInt(const char*, uint32_t d = 0)    { return d; }
  uint16_t getUShort(const char*, uint16_t d = 0)  { return d; }
  uint8_t  getUChar(const char*, uint8_t d = 0)    { return d; }
  bool     getBool(const char*, bool d = false)     { return d; }
  size_t   getBytes(const char*, void*, size_t)     { return 0; }
  size_t   getString(const char* k, char* buf, size_t len) {
    if (buf && len) buf[0] = 0;
    return 0;
  }

  void putUInt(const char*, uint32_t)         {}
  void putUShort(const char*, uint16_t)       {}
  void putUChar(const char*, uint8_t)         {}
  void putBool(const char*, bool)             {}
  void putBytes(const char*, const void*, size_t) {}
  void putString(const char*, const char*)    {}
};

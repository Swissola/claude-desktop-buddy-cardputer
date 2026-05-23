#pragma once
// Minimal Arduino type/function stubs for native (host) unit testing.
// Only the surface area used by stats.h is covered here.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Controllable clock — tests set this directly to simulate elapsed time.
static uint32_t _mock_millis = 0;
static inline uint32_t millis() { return _mock_millis; }

// Arduino uses a macro-style min that works on any type; provide the
// template version the compiler expects in C++ contexts.
template<typename T>
static inline T min(T a, T b) { return a < b ? a : b; }
template<typename T>
static inline T max(T a, T b) { return a > b ? a : b; }

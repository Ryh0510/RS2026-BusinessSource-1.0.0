#pragma once

#include <cstdint>
#include <string>

// =====================
// Configuration area: one-step switch.
// =====================

//#define IDGEN_USE_UUID   // Enable this to use UUID.
#define IDGEN_USE_RANDOM64 // Default: 64-bit random.

// =====================
// Public ID type.
// =====================

#ifdef IDGEN_USE_UUID
#include <array>
using ID = std::array<std::uint8_t, 16>;  // 128-bit UUID
#else
using ID = std::uint64_t;                 // 64-bit random
#endif

// =====================
// IdGenerator
// =====================

class IDGenerator {
public:
    static ID generate();

    // Optional serialization helpers.
    static std::string toString(const ID& id);
    static ID fromString(const std::string& s);

private:
    IDGenerator() = default;
};

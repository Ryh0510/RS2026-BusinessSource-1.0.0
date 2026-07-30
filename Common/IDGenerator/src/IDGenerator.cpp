#include <IDGenerator/IDGenerator.h>

#include <random>
#include <sstream>
#include <iomanip>

#ifdef IDGEN_USE_UUID
// =====================
// UUID Implementation
// =====================

ID IDGenerator::generate()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(0, 255);

    ID id{};
    for (auto& b : id) {
        b = static_cast<std::uint8_t>(dist(gen));
    }

    // UUID v4 variant bits (optional but recommended).
    id[6] = (id[6] & 0x0F) | 0x40;
    id[8] = (id[8] & 0x3F) | 0x80;

    return id;
}

std::string IDGenerator::toString(const ID& id)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < id.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(id[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) oss << '-';
    }
    return oss.str();
}

ID IDGenerator::fromString(const std::string& s)
{
    ID id{};
    std::string hex;
    for (char c : s) {
        if (c != '-') hex += c;
    }

    for (size_t i = 0; i < id.size(); ++i) {
        id[i] = static_cast<std::uint8_t>(
            std::stoi(hex.substr(i * 2, 2), nullptr, 16)
            );
    }
    return id;
}

#else
// =====================
// 64-bit Random Implementation
// =====================

ID IDGenerator::generate()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<ID> dist;

    return dist(gen);
}

std::string IDGenerator::toString(const ID& id)
{
    return std::to_string(id);
}

ID IDGenerator::fromString(const std::string& s)
{
    return static_cast<ID>(std::stoull(s));
}

#endif

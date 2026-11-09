#ifndef TRINITYCORE_TESTS_CLASSIC_WIRE_SPEC_READER_H
#define TRINITYCORE_TESTS_CLASSIC_WIRE_SPEC_READER_H

#include "Define.h"
#include <bit>
#include <sstream>
#include <string>
#include <string_view>

// The 70291 client's own reader specs (see tests/game/ClassicWireSpec.cpp for the grammar) and a reader that checks a packet payload against one.
// Shared by the golden byte tests and the headless client.
namespace ClassicWireSpec
{
struct SpecEntry
{
    uint32 Index;
    char const* Spec;
};

inline constexpr SpecEntry MainGroupSpecs[] =
{
#include "Classic70291MainGroupSpecs.inc"
};

inline constexpr uint32 MainGroup70291 = 0x47;

inline SpecEntry const* FindSpec(uint32 wireOpcode)
{
    if ((wireOpcode >> 16) != MainGroup70291)
        return nullptr;
    for (SpecEntry const& entry : MainGroupSpecs)
        if (entry.Index == (wireOpcode & 0xFFFF))
            return &entry;
    return nullptr;
}

// Reads `spec` from `data`; returns an empty string if the data is consumed exactly, else what went wrong.
inline std::string ReadSpec(std::string_view spec, uint8 const* data, size_t size)
{
    size_t pos = 0;
    std::istringstream tokens{ std::string(spec) };
    std::string token;
    std::ostringstream trace;
    while (tokens >> token)
    {
        size_t need = 0;
        if (token == "u8")
            need = 1;
        else if (token == "u16")
            need = 2;
        else if (token == "u32")
            need = 4;
        else if (token == "u64")
            need = 8;
        else if (token == "GUID")
        {
            if (pos + 2 > size)
            {
                trace << "GUID mask at " << pos << " is past the end (" << size << " bytes)";
                return trace.str();
            }
            need = 2 + size_t(std::popcount(uint32(data[pos])) + std::popcount(uint32(data[pos + 1])));
        }
        else
            return "spec token not readable: " + token;

        if (pos + need > size)
        {
            trace << "client reads " << token << " at byte " << pos << " but the packet has " << size << " bytes";
            return trace.str();
        }
        pos += need;
    }
    if (pos != size)
    {
        trace << "client stops at byte " << pos << ", the packet has " << size << " (" << (size - pos) << " bytes the client never reads)";
        return trace.str();
    }
    return { };
}

}

#endif

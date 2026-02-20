#pragma once

#include <cstdint>

namespace datahandler {

#pragma pack(push, 1)
struct TapeHeader {
    char magic[8];       // "TAPEv001"
    uint32_t version;
    uint32_t record_type;   // 2 = BAR_1M
    uint32_t record_size;  // sizeof(Bar1m)
    uint32_t reserved0;
    uint64_t start_ts_ns;
    uint64_t end_ts_ns;
    uint64_t record_count;
    uint8_t reserved[24];
};

struct Bar1m {
    uint64_t ts_ns;
    double open;
    double high;
    double low;
    double close;
    float volume;
};
#pragma pack(pop)

static_assert(sizeof(TapeHeader) == 72, "TapeHeader must be 72 bytes");
static_assert(sizeof(Bar1m) == 44, "Bar1m must be 44 bytes");

}  // namespace datahandler

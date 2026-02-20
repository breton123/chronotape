#pragma once
#include <cstdint>

namespace runpack
{

    // Keep names short for fixed-size header.
    static constexpr uint64_t MAGIC = 0x31504B504E555252ull; // "RRUNPPK1" (just a marker)
    static constexpr uint32_t VERSION = 1;

    enum class DType : uint32_t
    {
        I32 = 1,
        I64 = 2,
        F32 = 3,
        F64 = 4,
    };

#pragma pack(push, 1)

    struct FileHeader
    {
        uint64_t magic = MAGIC;
        uint32_t version = VERSION;
        uint32_t endian = 0x01020304; // allows endian check
        uint64_t created_unix_ms = 0;

        uint64_t meta_offset = 0;
        uint64_t meta_bytes = 0;

        uint64_t toc_offset = 0;
        uint32_t toc_count = 0;
        uint32_t reserved0 = 0;

        uint64_t trades_offset = 0;
        uint64_t trades_count = 0; // number of trade records

        uint64_t file_bytes = 0;
    };

    struct TocEntry
    {
        char name[32]; // null-terminated if shorter; else truncated
        DType dtype;
        uint32_t elem_size; // bytes per element
        uint64_t len;       // elements
        uint64_t offset;    // byte offset from file start to data blob
    };

    struct TradeDiskV1
    {
        int64_t entry_ts = 0;
        int64_t exit_ts = 0;
        int32_t entry_i = -1;
        int32_t exit_i = -1;

        int8_t side = 0; // +1 long, -1 short
        int8_t reserved1 = 0;
        int16_t reserved2 = 0;

        float lots = 0.0f;
        float entry_price = 0.0f;
        float exit_price = 0.0f;
        float pnl = 0.0f;

        float pnl_r = 0.0f;
        float mae = 0.0f;
        float mfe = 0.0f;
    };

#pragma pack(pop)

} // namespace runpack
#include "TapeReader.hpp"
#include "DateUtils.hpp"
#include <cstring>
#include <stdexcept>

namespace datahandler {

namespace {
    void die_if(bool cond, const char* msg) {
        if (cond) throw std::runtime_error(msg);
    }
}

TapeReader::TapeReader(std::string base_dir,
                       std::string symbol,
                       std::string timeframe,
                       int start_ymd,
                       int end_ymd)
    : base_dir_(std::move(base_dir))
    , symbol_(std::move(symbol))
    , timeframe_(std::move(timeframe))
    , start_ymd_(start_ymd)
    , end_ymd_(end_ymd)
    , current_day_(start_ymd)
    , bar_index_(0)
    , bar_count_(0)
    , recs_(nullptr)
{
    die_if(end_ymd_ < start_ymd_, "end date must be >= start date");
}

bool TapeReader::nextBar(Bar1m& out) {
    while (bar_index_ >= bar_count_) {
        if (!open_next_tape())
            return false;
    }

    out = recs_[bar_index_];
    ++bar_index_;
    return true;
}

bool TapeReader::open_next_tape() {
    while (current_day_ <= end_ymd_) {
        std::string path = make_tape_path(base_dir_, symbol_, timeframe_, current_day_);
        current_day_ = next_day(current_day_);

        if (!file_exists(path))
            continue;

        mmap_.close();
        mmap_.open_readonly(path);

        die_if(mmap_.size() < sizeof(TapeHeader), "File too small");
        const auto* hdr = reinterpret_cast<const TapeHeader*>(mmap_.data());

        die_if(std::memcmp(hdr->magic, "TAPEv001", 8) != 0, "Bad magic");
        die_if(hdr->version != 1, "Bad version");
        die_if(hdr->record_type != 2, "Expected BAR_1M");
        die_if(hdr->record_size != sizeof(Bar1m), "Bad record size");

        const size_t max_records = (mmap_.size() - sizeof(TapeHeader)) / sizeof(Bar1m);
        die_if(hdr->record_count > max_records, "record_count exceeds file size");

        const uint8_t* base = static_cast<const uint8_t*>(mmap_.data());
        recs_ = reinterpret_cast<const Bar1m*>(base + sizeof(TapeHeader));
        bar_count_ = hdr->record_count;
        bar_index_ = 0;
        return true;
    }
    return false;
}

}  // namespace datahandler

#pragma once

#include <string>

namespace datahandler {

// YYYYMMDD integer date helpers.
bool file_exists(const std::string& path);

int next_day(int yyyymmdd);

// Path: BASE_DIR/bars/SYMBOL/TIMEFRAME/YYYY/SYMBOL_YYYYMMDD.tape
std::string make_tape_path(const std::string& base_dir,
                           const std::string& symbol,
                           const std::string& timeframe,
                           int yyyymmdd);

}  // namespace datahandler

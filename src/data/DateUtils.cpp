#include "DateUtils.hpp"
#include <cstdio>
#include <windows.h>

namespace datahandler {

static bool is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2) ? mdays[m - 1] + (is_leap(y) ? 1 : 0) : mdays[m - 1];
}

static void ymd_from_int(int yyyymmdd, int& y, int& m, int& d) {
    y = yyyymmdd / 10000;
    m = (yyyymmdd / 100) % 100;
    d = yyyymmdd % 100;
}

static int ymd_to_int(int y, int m, int d) {
    return y * 10000 + m * 100 + d;
}

bool file_exists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int next_day(int yyyymmdd) {
    int y, m, d;
    ymd_from_int(yyyymmdd, y, m, d);
    d += 1;
    int dim = days_in_month(y, m);
    if (d > dim) {
        d = 1;
        m += 1;
    }
    if (m > 12) {
        m = 1;
        y += 1;
    }
    return ymd_to_int(y, m, d);
}

static std::string two(int x) {
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02d", x);
    return std::string(buf);
}

static std::string four(int x) {
    char buf[5];
    std::snprintf(buf, sizeof(buf), "%04d", x);
    return std::string(buf);
}

std::string make_tape_path(const std::string& base_dir,
                           const std::string& symbol,
                           const std::string& timeframe,
                           int yyyymmdd) {
    int y, m, d;
    ymd_from_int(yyyymmdd, y, m, d);
    return base_dir + "/bars/" + symbol + "/" + timeframe + "/" + four(y) + "/" +
           symbol + "_" + four(y) + two(m) + two(d) + ".tape";
}

}  // namespace datahandler

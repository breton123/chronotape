#pragma once

#include <cstddef>
#include <string>
#include <stdexcept>

#include <windows.h>

namespace datahandler {

// RAII memory-mapped file for read-only access.
class MMapFile {
public:
    MMapFile() = default;
    ~MMapFile() { close(); }

    MMapFile(const MMapFile&) = delete;
    MMapFile& operator=(const MMapFile&) = delete;

    void open_readonly(const std::string& path);
    void close();

    void* data() const { return data_; }
    size_t size() const { return size_; }
    bool is_open() const { return data_ != nullptr; }

private:
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = NULL;
    void* data_ = nullptr;
    size_t size_ = 0;
};

}  // namespace datahandler

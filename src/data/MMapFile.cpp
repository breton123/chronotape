#include "MMapFile.hpp"

namespace datahandler {

void MMapFile::open_readonly(const std::string& path) {
    file_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE)
        throw std::runtime_error("CreateFile failed: " + path);

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(file_, &sz))
        throw std::runtime_error("GetFileSizeEx failed");
    size_ = static_cast<size_t>(sz.QuadPart);

    mapping_ = CreateFileMappingA(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_)
        throw std::runtime_error("CreateFileMapping failed");

    data_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
    if (!data_)
        throw std::runtime_error("MapViewOfFile failed");
}

void MMapFile::close() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = NULL;
    }
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
    size_ = 0;
}

}  // namespace datahandler

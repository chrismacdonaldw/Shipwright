#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace SohSaveFile {

inline bool Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    const bool written = out.good();
    out.close();
    return written && !out.fail();
}

inline std::mutex& WriterMutex() {
    static std::mutex mutex;
    return mutex;
}

inline uint64_t& NextTemporary() {
    static uint64_t nextTemporary = 0;
    return nextTemporary;
}

// Serialize writers and reserve an adjacent directory so cleanup owns every temporary path.
// Rename publishes a complete closed file; this does not promise power-loss durability.
template <typename Writer>
bool Publish(const std::filesystem::path& path, const std::string& text, Writer writer) {
    const std::lock_guard<std::mutex> lock(WriterMutex());
    struct Temporary {
        std::filesystem::path directory;
        std::filesystem::path file;
        bool owned = false;
        ~Temporary() {
            if (!owned) {
                return;
            }
            std::error_code ignored;
            if (!file.empty()) {
                std::filesystem::remove(file, ignored);
            }
            std::filesystem::remove(directory, ignored);
        }
    } temp;
    try {
        std::error_code error;
        for (int attempt = 0; attempt < 64; ++attempt) {
            temp.directory = path;
            temp.directory += ".tmp-" + std::to_string(NextTemporary()++);
            if (std::filesystem::create_directory(temp.directory, error)) {
                temp.owned = true;
                break;
            }
            if (error && error != std::errc::file_exists) {
                return false;
            }
            error.clear();
        }
        if (!temp.owned) {
            return false;
        }
        temp.file = temp.directory / "save";
        if (!writer(temp.file, text)) {
            return false;
        }
        std::filesystem::rename(temp.file, path, error);
        return !error;
    } catch (...) {
        return false;
    }
}

inline bool Publish(const std::filesystem::path& path, const std::string& text) {
    return Publish(path, text, Write);
}

} // namespace SohSaveFile

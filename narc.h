#pragma once

#include <cstdint>
#include <fstream>
#include <vector>

#if (__cplusplus < 201703L)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace narc {

enum class NarcError {
    None,
    InvalidInputFile,
    InvalidHeaderId,
    InvalidByteOrderMark,
    InvalidVersion,
    InvalidHeaderSize,
    InvalidChunkCount,
    InvalidFileAllocationTableId,
    InvalidFileAllocationTableReserved,
    InvalidFileNameTableId,
    InvalidFileImagesId,
    InvalidOutputFile
};

struct Header {
    uint32_t Id;
    uint16_t ByteOrderMark;
    uint16_t Version;
    uint32_t FileSize;
    uint16_t ChunkSize;
    uint16_t ChunkCount;
};

struct FileAllocationTable {
    uint32_t Id;
    uint32_t ChunkSize;
    uint16_t FileCount;
    uint16_t Reserved;
};

struct FileAllocationTableEntry {
    uint32_t Start;
    uint32_t End;
};

struct FileNameTable {
    uint32_t Id;
    uint32_t ChunkSize;
};

struct FileNameTableEntry {
    uint32_t Offset;
    uint16_t FirstFileId;
    uint16_t Utility;
};

struct FileImages {
    uint32_t Id;
    uint32_t ChunkSize;
};

// clang-format off
extern narc::NarcError pack(const fs::path &dst_file,
                            const fs::path &src_dir,
                            const fs::path &order_path = "",
                            const fs::path &ignore_path = "",
                            const fs::path &keep_path = "");
extern narc::NarcError unpack(const fs::path &src_file,
                              const fs::path &dst_dir);
// clang-format on

}; // namespace narc

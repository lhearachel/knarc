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

typedef struct Header {
    uint32_t id;
    uint16_t endianness;
    uint16_t version;
    uint32_t file_size;
    uint16_t chunk_size;
    uint16_t num_chunks;
} Header;

typedef struct FileAllocationTable {
    uint32_t id;
    uint32_t chunk_size;
    uint16_t num_files;
    uint16_t reserved;
} FileAllocationTable;

typedef struct FileAllocationTableEntry {
    uint32_t start;
    uint32_t end;
} FileAllocationTableEntry;

typedef struct FileNameTable {
    uint32_t id;
    uint32_t chunk_size;
} FileNameTable;

typedef struct FileNameTableEntry {
    uint32_t offset;
    uint16_t first_file_id;
    uint16_t util;
} FileNameTableEntry;

typedef struct FileImages {
    uint32_t id;
    uint32_t chunk_size;
} FileImages;

// clang-format off
extern narc::NarcError pack(const fs::path &dst_file,
                            const fs::path &src_dir,
                            const fs::path &order_path = "",
                            const fs::path &ignore_path = "",
                            const fs::path &keep_path = "");
extern narc::NarcError unpack(const fs::path &src_file,
                              const fs::path &dst_dir);
// clang-format on

} // namespace narc

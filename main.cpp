#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>

#include "argparse.hpp"
#include "narc.h"

#define PROGRAM_NAME          "knarc"
#define PROGRAM_VERSION_MAJOR 2
#define PROGRAM_VERSION_MINOR 0
#define PROGRAM_VERSION_PATCH 0

template<typename... Args>
std::string string_format(const std::string &format, Args... args)
{
    int strsize = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // + '\0'
    if (strsize <= 0) {
        throw std::runtime_error("Format allocation error");
    }

    auto size = static_cast<size_t>(strsize);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);

    return std::string(buf.get(), buf.get() + size - 1); // trim '\0'
}

#define PROGRAM_VERSION         string_format("{}.{}", PROGRAM_VERSION_MAJOR, PROGRAM_VERSION_MINOR)
#define PROGRAM_VERSION_PATCHED string_format("{}.{}.{}", PROGRAM_VERSION_MAJOR, PROGRAM_VERSION_MINOR, PROGRAM_VERSION_PATCH)

bool debug = false;
bool build_fnt = false;
bool output_header = false;

void PrintError(NarcError error)
{
    switch (error) {
    case NarcError::None:
        std::cerr << "ERROR: No error???" << std::endl;
        break;

    case NarcError::InvalidInputFile:
        std::cerr << "ERROR: Invalid input file" << std::endl;
        break;

    case NarcError::InvalidHeaderId:
        std::cerr << "ERROR: Invalid header ID" << std::endl;
        break;

    case NarcError::InvalidByteOrderMark:
        std::cerr << "ERROR: Invalid byte order mark" << std::endl;
        break;

    case NarcError::InvalidVersion:
        std::cerr << "ERROR: Invalid NARC version" << std::endl;
        break;

    case NarcError::InvalidHeaderSize:
        std::cerr << "ERROR: Invalid header size" << std::endl;
        break;

    case NarcError::InvalidChunkCount:
        std::cerr << "ERROR: Invalid chunk count" << std::endl;
        break;

    case NarcError::InvalidFileAllocationTableId:
        std::cerr << "ERROR: Invalid file allocation table ID" << std::endl;
        break;

    case NarcError::InvalidFileAllocationTableReserved:
        std::cerr << "ERROR: Invalid file allocation table reserved section" << std::endl;
        break;

    case NarcError::InvalidFileNameTableId:
        std::cerr << "ERROR: Invalid file name table ID" << std::endl;
        break;

    case NarcError::InvalidFileNameTableEntryId:
        std::cerr << "ERROR: Invalid file name table entry ID" << std::endl;
        break;

    case NarcError::InvalidFileImagesId:
        std::cerr << "ERROR: Invalid file images ID" << std::endl;
        break;

    case NarcError::InvalidOutputFile:
        std::cerr << "ERROR: Invalid output file" << std::endl;
        break;

    default:
        std::cerr << "ERROR: Unknown error???" << std::endl;
        break;
    }
}

int main(int argc, char *argv[])
{
    std::string directory, source, target;
    argparse::ArgumentParser program(PROGRAM_NAME, PROGRAM_VERSION);

    program.add_argument("-d", "--directory")
        .metavar("DIRECTORY")
        .help("directory to be packed or to dump unpacked files")
        .required()
        .store_into(directory);

    auto &pack_or_unpack = program.add_mutually_exclusive_group();
    pack_or_unpack.add_argument("-p", "--pack")
        .metavar("SOURCE")
        .help("directory to be packed to a target NARC")
        .store_into(source);
    pack_or_unpack.add_argument("-u", "--unpack")
        .metavar("TARGET")
        .help("directory to dump unpacked files from a source NARC")
        .store_into(target);

    program.add_argument("-f", "--filename-table")
        .help("build the filename table")
        .flag()
        .store_into(build_fnt);

    program.add_argument("-n", "--naix")
        .help("output a C-style .naix header")
        .flag()
        .store_into(output_header);

    program.add_argument("--verbose")
        .help("output additional program messages")
        .flag()
        .store_into(debug);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (debug) {
        std::cout << "directory: " << directory << std::endl;
        std::cout << "source:    " << source << std::endl;
        std::cout << "target:    " << target << std::endl;

        std::cout << std::boolalpha;
        std::cout << "build filename table? " << build_fnt << std::endl;
        std::cout << "output NAIX header?   " << output_header << std::endl;
    }

    Narc narc;
    bool pack = target.empty();
    if (pack) {
        if (!narc.Pack(source, directory)) {
            PrintError(narc.GetError());
            std::exit(1);
        }
    } else {
        if (!narc.Unpack(target, directory)) {
            PrintError(narc.GetError());
            std::exit(1);
        }
    }

    return 0;
}

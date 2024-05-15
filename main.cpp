#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
bool pack_with_fnt = false;
bool output_header = false;
bool use_v0 = false;
bool prefix_header_entries = false;

static void print_error(narc::NarcError error);
static void load_response_file(const char *fname, std::vector<std::string> &args);

int main(int argc, char *argv[])
{
    std::string directory, source, target;
    std::string ignore_fname, keep_fname, order_fname;
    std::vector<std::string> response_file_args;

    argparse::ArgumentParser program(PROGRAM_NAME, PROGRAM_VERSION);
    program.add_description("Utility for un/packing Nitro Archives for the Nintendo DS");

    program.add_argument("-d", "--directory")
        .metavar("DIRECTORY")
        .help("directory to be packed or to dump unpacked files")
        .required()
        .store_into(directory);

    auto &pack_or_unpack = program.add_mutually_exclusive_group();
    pack_or_unpack.add_argument("-p", "--pack")
        .metavar("TARGET")
        .help("name of a NARC to be packed from DIRECTORY")
        .store_into(target);
    pack_or_unpack.add_argument("-u", "--unpack")
        .metavar("SOURCE")
        .help("name of a NARC to be unpacked into DIRECTORY")
        .store_into(source);

    program.add_argument("-f", "--filename-table")
        .help("build the filename table")
        .flag()
        .store_into(pack_with_fnt);

    program.add_argument("-n", "--naix")
        .help("output a C-style .naix header")
        .flag()
        .store_into(output_header);

    program.add_argument("--prefix-header-entries")
        .help("prefix entries in an output .naix header with TARGET; dependent on --naix")
        .flag()
        .store_into(prefix_header_entries);

    program.add_argument("-i", "--ignore")
        .metavar("IGNORE_FILE")
        .help("specify a file listing file-patterns to ignore for packing")
        .store_into(ignore_fname);

    program.add_argument("-k", "--keep")
        .metavar("KEEP_FILE")
        .help("specify a file listing file-patterns to keep during packing; "
              "listed patterns override those matching patterns in IGNORE_FILE")
        .store_into(keep_fname);

    program.add_argument("-o", "--order")
        .metavar("ORDER_FILE")
        .help("specify a file listing order of files for packing; "
              "listed files override those matching patterns in IGNORE_FILE")
        .store_into(order_fname);

    program.add_argument("-z", "--version-zero")
        .help("output the NARC as version 0 spec")
        .flag()
        .store_into(use_v0);

    program.add_argument("--verbose")
        .help("output additional program messages")
        .flag()
        .store_into(debug);

    if (argc > 1 && argv[1][0] == '@') {
        response_file_args.push_back(argv[0]);
        load_response_file(argv[1], response_file_args);
    }

    try {
        if (response_file_args.empty()) {
            program.parse_args(argc, argv);
        } else {
            program.parse_args(response_file_args);
        }
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (debug) {
        std::cout << "[DEBUG] directory: " << directory << std::endl;
        std::cout << "[DEBUG] target:    " << target << std::endl;
        std::cout << "[DEBUG] source:    " << source << std::endl;

        std::cout << std::boolalpha;
        std::cout << "[DEBUG] build filename table? " << pack_with_fnt << std::endl;
        std::cout << "[DEBUG] output NAIX header?   " << output_header << std::endl;

        std::cout << "[DEBUG] ignore file: " << ignore_fname << std::endl;
        std::cout << "[DEBUG] keep file:   " << keep_fname << std::endl;
        std::cout << "[DEBUG] order file:  " << order_fname << std::endl;
    }

    narc::NarcError err = source.empty()
                            ? narc::pack(target, directory, order_fname, ignore_fname, keep_fname)
                            : narc::unpack(source, directory);

    if (err != narc::NarcError::None) {
        print_error(err);
        std::exit(1);
    }

    return 0;
}

inline void ltrim(std::string &s)
{
    // clang-format off
    s.erase(
        s.begin(),
        find_if(s.begin(), s.end(),
            [](unsigned char c) {
                return !isspace(c);
            }
        )
    );
    // clang-format on
}

inline void rtrim(std::string &s)
{
    // clang-format off
    s.erase(
        find_if(s.rbegin(), s.rend(),
            [](unsigned char c) {
                return !isspace(c);
            }
        ).base(),
        s.end()
    );
    // clang-format on
}

inline void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

static void load_response_file(const char *fname, std::vector<std::string> &args)
{
    std::string fname_str(fname + 1);
    std::ifstream ifs(fname_str);
    if (!ifs.good()) {
        throw std::invalid_argument("failed to read response file " + fname_str);
    }

    std::string arg;
    while (std::getline(ifs, arg, ' ')) {
        trim(arg);
        args.push_back(arg);
    }
}

static void print_error(narc::NarcError error)
{
    switch (error) {
    case narc::NarcError::None:
        std::cerr << "ERROR: No error???" << std::endl;
        break;

    case narc::NarcError::InvalidInputFile:
        std::cerr << "ERROR: Invalid input file" << std::endl;
        break;

    case narc::NarcError::InvalidHeaderId:
        std::cerr << "ERROR: Invalid header ID" << std::endl;
        break;

    case narc::NarcError::InvalidByteOrderMark:
        std::cerr << "ERROR: Invalid byte order mark" << std::endl;
        break;

    case narc::NarcError::InvalidVersion:
        std::cerr << "ERROR: Invalid NARC version" << std::endl;
        break;

    case narc::NarcError::InvalidHeaderSize:
        std::cerr << "ERROR: Invalid header size" << std::endl;
        break;

    case narc::NarcError::InvalidChunkCount:
        std::cerr << "ERROR: Invalid chunk count" << std::endl;
        break;

    case narc::NarcError::InvalidFileAllocationTableId:
        std::cerr << "ERROR: Invalid file allocation table ID" << std::endl;
        break;

    case narc::NarcError::InvalidFileAllocationTableReserved:
        std::cerr << "ERROR: Invalid file allocation table reserved section" << std::endl;
        break;

    case narc::NarcError::InvalidFileNameTableId:
        std::cerr << "ERROR: Invalid file name table ID" << std::endl;
        break;

    case narc::NarcError::InvalidFileImagesId:
        std::cerr << "ERROR: Invalid file images ID" << std::endl;
        break;

    case narc::NarcError::InvalidOutputFile:
        std::cerr << "ERROR: Invalid output file" << std::endl;
        break;

    default:
        std::cerr << "ERROR: Unknown error???" << std::endl;
        break;
    }
}


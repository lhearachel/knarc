#include "narc.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <vector>

#include "fnmatch.h"

#if (__cplusplus < 201703L)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#define FATB_ID 0x46415442
#define FNTB_ID 0x464E5442
#define FIMG_ID 0x46494D47
#define NARC_ID 0x4352414E

#define LE_BYTE_ORDER 0xFFFE

#define NARC_V0 0x0000
#define NARC_V1 0x0100

#define NARC_CHUNK_COUNT 0x03

using namespace std;

extern bool debug;
extern bool pack_with_fnt;
extern bool output_header;
extern bool use_v0;
extern bool prefix_header_entries;

namespace {

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

inline bool read_spec_file(const fs::path &spec_fname, vector<string> &patterns)
{
    if (spec_fname.empty()) {
        return true;
    }

    ifstream ifs(spec_fname);
    if (!ifs.good()) {
        if (debug) {
            cout << "[DEBUG] Could not open spec file " << spec_fname << endl;
        }

        return false;
    }

    string line;
    while (getline(ifs, line)) {
        trim(line);
        if (!line.empty()) {
            patterns.push_back(line);
        }
    }

    return true;
}

class WildcardVector : public vector<string> {
  public:
    WildcardVector() : vector<string>() {}

    WildcardVector(fs::path fp)
    {
        if (!fs::exists(fp)) {
            return;
        }

        fstream infile;
        infile.open(fp, ios_base::in);

        string line;
        while (getline(infile, line)) {
            trim(line);
            if (!line.empty()) {
                push_back(line);
            }
        }
    }

    bool matches(string fp) const
    {
        for (const string &pattern : *this) {
            if (fnmatch(pattern.c_str(), fp.c_str(), FNM_PERIOD) == 0) {
                return true;
            }
        }
        return false;
    }
};

vector<fs::directory_entry> find_files(const fs::path &dir, const WildcardVector &ignore_patterns, const WildcardVector &keep_patterns);
vector<fs::directory_entry> find_files(const fs::path &dir, const WildcardVector &ignore_patterns, const WildcardVector &keep_patterns, vector<string> &order_spec, const bool explicit_order = true);

void align_dword(ofstream &ofs, uint8_t padding_byte)
{
    if ((ofs.tellp() % 4) != 0) {
        for (int i = 4 - (ofs.tellp() % 4); i-- > 0;) {
            ofs.write(reinterpret_cast<char *>(&padding_byte), sizeof(uint8_t));
        }
    }
}

/*
 * Find path entries recursively beneath a given directory, sorting them in
 * a particular order.
 *
 * - Files which match a pattern to be ignored will be excluded from the output.
 * - Files which match a pattern to be kept or which are included in the order
 * specification will always be included.
 * - Files which are not included in the order specification will be added at
 * the end of the output and are sorted in lexicographical order.
 */
vector<fs::directory_entry> find_files(const fs::path &dir, const WildcardVector &ignore_patterns, const WildcardVector &keep_patterns, vector<string> &order_spec, const bool explicit_order)
{
    vector<fs::directory_entry> ordered_files, unordered_files;
    for (auto &entry : order_spec) {
        fs::path file_path = dir / entry;

        // clang-format off
        if (fs::exists(file_path)
                && (!ignore_patterns.matches(file_path)
                    || keep_patterns.matches(file_path))) {
            if (ignore_patterns.matches(file_path) && !keep_patterns.matches(file_path)) {
                if (debug) {
                    cout << "[DEBUG] File exists but will be ignored: " << file_path << endl;
                }
            } else {
                if (debug) {
                    cout << "[DEBUG] Adding file from order spec: " << file_path << endl;
                }

                ordered_files.push_back(fs::directory_entry(file_path));
            }
        } else {
            if (debug) {
                cout << "[DEBUG] File from order spec does not exist: " << file_path << endl;
            }
        }
        // clang-format on
    }

    // find files in subdirectories
    // clear out the order_spec so that files are not double-added for
    // an explicit ordering
    order_spec.clear();
    for (auto &entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            // if the order was not explicitly specified, invoke the leading function
            // which will parse a .knarcorder file at the next entry
            vector<fs::directory_entry> subdir_files = explicit_order
                                                         ? find_files(entry.path(), ignore_patterns, keep_patterns, order_spec)
                                                         : find_files(entry.path(), ignore_patterns, keep_patterns);
        }
    }

    // add remaining files
    for (auto &entry : fs::directory_iterator(dir)) {
        auto file_path = entry.path();
        auto filename = file_path.filename();

        // clang-format off
        if (entry.is_regular_file()
                && filename != ".knarcorder"
                && find(ordered_files.begin(), ordered_files.end(), entry) == ordered_files.end()) {
            if (ignore_patterns.matches(filename) && !keep_patterns.matches(filename)) {
                if (debug) {
                    cout << "[DEBUG] File ignored: " << entry  << endl;
                }
            } else {
                if (debug) {
                    cout << "[DEBUG] Adding unordered file: " << entry << endl;
                }

                unordered_files.push_back(entry);
            }
        }
        // clang-format on
    }

    // clang-format off
    sort(unordered_files.begin(), unordered_files.end(),
        [](const fs::directory_entry &a, const fs::directory_entry &b) {
            string a_str = a.path().filename().string();
            string b_str = b.path().filename().string();

            for (size_t i = 0; i < a_str.size(); ++i) {
                a_str[i] = tolower(a_str[i]);
            }

            for (size_t i = 0; i < b_str.size(); ++i) {
                b_str[i] = tolower(b_str[i]);
            }

            return a_str < b_str;
        }
    );
    // clang-format on

    ordered_files.insert(ordered_files.end(), unordered_files.begin(), unordered_files.end());
    return ordered_files;
}

vector<fs::directory_entry> find_files(const fs::path &dir, const WildcardVector &ignore_patterns, const WildcardVector &keep_patterns)
{
    vector<string> order_spec;

    if (fs::exists(dir / ".knarcorder")) {
        if (debug) {
            cout << "[DEBUG] knarcorder file exists for " << dir << endl;
        }

        read_spec_file(dir / ".knarcorder", order_spec);
    }

    return find_files(dir, ignore_patterns, keep_patterns, order_spec, false);
}

tuple<narc::FileAllocationTable, vector<narc::FileAllocationTableEntry>> build_fat(vector<fs::directory_entry> &files, ofstream &header_ofs, const string &main_stem, const string &main_stem_upper)
{
    auto not_directory = [](fs::directory_entry &e) { return !fs::is_directory(e); };
    vector<narc::FileAllocationTableEntry> fat_entries;
    int member_idx = 0;

    for (auto &entry : files | views::filter(not_directory)) {
        uint32_t entry_start = fat_entries.empty()
                                 ? 0
                                 : fat_entries.back().end;
        if (entry_start % 4 != 0) {
            entry_start += 4 - (entry_start % 4);
        }

        uint32_t entry_end = entry_start + static_cast<uint32_t>(fs::file_size(entry));

        fat_entries.push_back(narc::FileAllocationTableEntry {
            .start = entry_start,
            .end = entry_end,
        });

        if (output_header) {
            string entry_stem = entry.path().filename().string();
            replace(entry_stem.begin(), entry_stem.end(), '.', '_');

            header_ofs << "#define ";
            if (prefix_header_entries) {
                header_ofs << "NARC_" << main_stem << "_";
            }
            header_ofs << entry_stem << " " << member_idx << "\n";
            member_idx++;
        }
    }

    if (output_header) {
        // clang-format off
        header_ofs << "\n"
                      "#endif // NARC_" << main_stem_upper << "_NAIX_\n";
        // clang-format on
    }

    return {
        narc::FileAllocationTable {
            .id = FATB_ID,
            .chunk_size = static_cast<uint32_t>(sizeof(narc::FileAllocationTable) + ((uint32_t)fat_entries.size() * sizeof(narc::FileAllocationTableEntry))),
            .num_files = static_cast<uint16_t>(fat_entries.size()),
            .reserved = 0x0,
        },
        fat_entries,
    };
}

uint16_t build_fnt_sub_entries(vector<fs::directory_entry> &files, map<fs::path, string> &sub_entries, vector<fs::path> &sub_paths)
{
    uint16_t num_dirs = 0;

    for (const auto &file : files) {
        const auto &file_path = file.path();
        const auto &parent_path = file_path.parent_path();
        const auto &filename = file_path.filename().string();
        if (!sub_entries.count(parent_path)) {
            sub_entries.insert({ parent_path, "" });
            sub_paths.push_back(parent_path);
        }

        if (fs::is_directory(file)) {
            num_dirs++;

            sub_entries[parent_path] += static_cast<uint8_t>(0x80 + filename.size());
            sub_entries[parent_path] += filename;
            sub_entries[parent_path] += (0xF000 + num_dirs) & 0xFF;
            sub_entries[parent_path] += (0xF000 + num_dirs) >> 8;
        } else {
            sub_entries[parent_path] += static_cast<uint8_t>(filename.size());
            sub_entries[parent_path] += filename;
        }

        sub_entries[parent_path] += '\0';
    }

    return num_dirs;
}

typedef struct FileNameTableData {
    narc::FileNameTable fnt;
    vector<narc::FileNameTableEntry> fnt_entries;
    map<fs::path, string> sub_entries;
    vector<fs::path> sub_paths;
} FileNameTableData;

FileNameTableData build_fnt(vector<fs::directory_entry> &files)
{
    vector<narc::FileNameTableEntry> fnt_entries;
    map<fs::path, string> sub_entries;
    vector<fs::path> sub_paths;

    if (pack_with_fnt) {
        uint16_t num_dirs = build_fnt_sub_entries(files, sub_entries, sub_paths);

        fnt_entries.push_back({
            .offset = static_cast<uint32_t>((num_dirs + 1) * sizeof(narc::FileNameTableEntry)),
            .first_file_id = 0x0,
            .util = static_cast<uint16_t>(num_dirs + 1),
        });

        for (uint16_t i = 0; i < num_dirs; i++) {
            auto &sub_entry = sub_entries[sub_paths[i]];
            fnt_entries.push_back({
                .offset = static_cast<uint32_t>(fnt_entries.back().offset + sub_entry.size()),
                .first_file_id = fnt_entries.back().first_file_id,
                .util = 0x0,
            });

            for (size_t j = 0; j < sub_entry.size() - 1; j++) {
                if (static_cast<uint8_t>(sub_entry[j]) <= 0x7F) {
                    j += static_cast<uint8_t>(sub_entry[j]);
                    fnt_entries.back().first_file_id++;
                } else {
                    j += static_cast<uint8_t>(sub_entry[j]) - 0x80 + 0x02;
                }
            }

            fnt_entries.back().util = find(sub_paths.begin(), sub_paths.end(), sub_paths[i + 1].parent_path())
                                       - sub_paths.begin() + 0xF000;
        }
    } else {
        fnt_entries.push_back({
            .offset = 0x4,
            .first_file_id = 0x0,
            .util = 0x1,
        });
    }

    narc::FileNameTable fnt {
        .id = FNTB_ID,
        .chunk_size = static_cast<uint32_t>(sizeof(narc::FileNameTable) + (fnt_entries.size() * sizeof(narc::FileNameTableEntry))),
    };

    if (pack_with_fnt) {
        for (const auto &sub_entry : sub_entries) {
            fnt.chunk_size += sub_entry.second.size();
        }
    }

    if (fnt.chunk_size % 4 != 0) {
        fnt.chunk_size += 4 - (fnt.chunk_size % 4);
    }

    return {
        fnt,
        fnt_entries,
        sub_entries,
        sub_paths,
    };
}

} // namespace

narc::NarcError narc::pack(const fs::path &dst_file, const fs::path &src_dir, const fs::path &order_file, const fs::path &ignore_file, const fs::path &keep_file)
{
    ofstream ofs(dst_file, ios::binary);
    if (!ofs.good()) {
        if (debug) {
            cout << "[DEBUG] Could not open output file " << dst_file << endl;
        }

        return NarcError::InvalidOutputFile;
    }

    // Pikalax 29 May 2021
    // Output an includable header that enumerates the NARC contents
    ofstream ofhs;
    string stem, stem_upper;
    if (output_header) {
        fs::path naix_fname = dst_file;
        naix_fname.replace_extension(".naix");

        ofhs.open(naix_fname);
        if (!ofhs.good()) {
            return NarcError::InvalidOutputFile;
        }

        stem = dst_file.stem().string();
        for (auto &c : stem) {
            stem_upper += toupper(c);
        }

        // clang-format off
        ofhs << "/*\n"
                " * THIS FILE WAS AUTOMATICALLY GENERATED BY knarc\n"
                " *              DO NOT MODIFY!!!\n"
                " */\n"
                "\n"
                "#ifndef NARC_" << stem_upper << "_NAIX_\n"
                "#define NARC_" << stem_upper << "_NAIX_\n"
                "\n";
        // clang-format on
    }

    // build set of file-patterns to be ignored/kept
    WildcardVector ignore_patterns, keep_patterns;
    ignore_patterns.push_back("*.knarcignore");
    ignore_patterns.push_back("*.knarckeep");
    ignore_patterns.push_back("*.knarcorder");

    if (!read_spec_file(ignore_file, ignore_patterns) || !read_spec_file(keep_file, keep_patterns)) {
        return NarcError::InvalidInputFile;
    }

    // find files to be included in the packed NARC
    vector<fs::directory_entry> files;
    if (order_file.empty()) {
        if (debug) {
            cout << "[DEBUG] Building file list using implicit .knarcorder" << endl;
        }

        files = find_files(src_dir, ignore_patterns, keep_patterns);
    } else {
        if (debug) {
            cout << "[DEBUG] Building file list from explicit " << order_file << endl;
        }

        vector<string> order_spec;
        if (!read_spec_file(order_file, order_spec)) {
            return NarcError::InvalidInputFile;
        }

        files = find_files(src_dir, ignore_patterns, keep_patterns, order_spec);
    }

    auto [fat, fat_entries] = build_fat(files, ofhs, stem, stem_upper);
    auto [fnt, fnt_entries, sub_entries, sub_paths] = build_fnt(files);

    FileImages fi {
        .id = FIMG_ID,
        .chunk_size = static_cast<uint32_t>(sizeof(FileImages) + (fat_entries.empty() ? 0 : fat_entries.back().end)),
    };

    if (fi.chunk_size % 4 != 0) {
        fi.chunk_size += 4 - (fi.chunk_size % 4);
    }

    Header header {
        .id = NARC_ID,
        .endianness = LE_BYTE_ORDER,
        .version = static_cast<uint16_t>(use_v0 ? NARC_V0 : NARC_V1),
        .file_size = static_cast<uint32_t>(sizeof(Header) + fat.chunk_size + fnt.chunk_size + fi.chunk_size),
        .chunk_size = sizeof(Header),
        .num_chunks = NARC_CHUNK_COUNT,
    };

    ofs.write(reinterpret_cast<char *>(&header), sizeof(Header));

    ofs.write(reinterpret_cast<char *>(&fat), sizeof(FileAllocationTable));
    for (auto &entry : fat_entries) {
        ofs.write(reinterpret_cast<char *>(&entry), sizeof(FileAllocationTableEntry));
    }

    ofs.write(reinterpret_cast<char *>(&fnt), sizeof(FileNameTable));
    for (auto &entry : fnt_entries) {
        ofs.write(reinterpret_cast<char *>(&entry), sizeof(FileNameTableEntry));
    }

    if (pack_with_fnt) {
        for (const auto &sub_path : sub_paths) {
            ofs << sub_entries[sub_path];
        }
    }

    align_dword(ofs, 0xFF);

    ofs.write(reinterpret_cast<char *>(&fi), sizeof(FileImages));
    for (const auto &entry : files) {
        if (is_directory(entry)) {
            continue;
        }

        ifstream ifs(entry.path(), ios::binary | ios::ate);
        if (!ifs.good()) {
            return NarcError::InvalidInputFile;
        }

        streampos len = ifs.tellg();
        unique_ptr<char[]> buf = make_unique<char[]>(static_cast<unsigned int>(len));

        ifs.seekg(0);
        ifs.read(buf.get(), len);
        ofs.write(buf.get(), len);
        align_dword(ofs, 0xFF);
    }

    return NarcError::None;
}

narc::NarcError narc::unpack(const fs::path &src_file, const fs::path &dst_dir)
{
    ifstream ifs(src_file, ios::binary);
    if (!ifs.good()) {
        return NarcError::InvalidInputFile;
    }

    Header header;
    ifs.read(reinterpret_cast<char *>(&header), sizeof(Header));

    if (header.id != NARC_ID) {
        return NarcError::InvalidHeaderId;
    }

    if (header.endianness != LE_BYTE_ORDER) {
        return NarcError::InvalidByteOrderMark;
    }

    if (header.version != NARC_V1 && header.version != NARC_V0) {
        return NarcError::InvalidVersion;
    }

    if (header.chunk_size != sizeof(Header)) {
        return NarcError::InvalidHeaderSize;
    }

    if (header.num_chunks != NARC_CHUNK_COUNT) {
        return NarcError::InvalidChunkCount;
    }

    FileAllocationTable fat;
    ifs.read(reinterpret_cast<char *>(&fat), sizeof(FileAllocationTable));

    if (fat.id != FATB_ID) {
        return NarcError::InvalidFileAllocationTableId;
    }

    if (fat.reserved != 0x00) {
        return NarcError::InvalidFileAllocationTableReserved;
    }

    unique_ptr<FileAllocationTableEntry[]> fat_entries = make_unique<FileAllocationTableEntry[]>(fat.num_files);
    for (uint16_t i = 0; i < fat.num_files; i++) {
        ifs.read(reinterpret_cast<char *>(&fat_entries.get()[i]), sizeof(FileAllocationTableEntry));
    }

    FileNameTable fnt;
    ifs.read(reinterpret_cast<char *>(&fnt), sizeof(FileNameTable));

    if (fnt.id != FNTB_ID) {
        return NarcError::InvalidFileNameTableId;
    }

    uint32_t fnt_entries_start = header.chunk_size + fat.chunk_size + sizeof(FileNameTable);
    vector<FileNameTableEntry> fnt_entries;
    do {
        FileNameTableEntry fnt_entry;
        ifs.read(reinterpret_cast<char *>(&fnt_entry), sizeof(FileNameTableEntry));
        fnt_entries.push_back(fnt_entry);
    } while (static_cast<uint32_t>(ifs.tellg()) < fnt_entries_start + fnt_entries[0].offset);

    unique_ptr<string[]> file_names = make_unique<string[]>(0xFFFF);
    for (size_t i = 0; i < fnt_entries.size(); i++) {
        ifs.seekg(static_cast<uint64_t>(fnt_entries_start) + fnt_entries[i].offset);

        uint16_t file_id = 0;
        for (uint8_t len = 0x80; len != 0x00; ifs.read(reinterpret_cast<char *>(&len), sizeof(uint8_t))) {
            if (len <= 0x7F) {
                for (uint8_t j = 0; j < len; j++) {
                    uint8_t c;
                    ifs.read(reinterpret_cast<char *>(&c), sizeof(uint8_t));

                    file_names.get()[fnt_entries[i].first_file_id + file_id] += c;
                }

                file_id++;
            } else if (len == 0x80) {
                // reserved
            } else {
                len -= 0x80;
                string dir_name;

                for (uint8_t j = 0; j < len; j++) {
                    uint8_t c;
                    ifs.read(reinterpret_cast<char *>(&c), sizeof(uint8_t));

                    dir_name += c;
                }

                uint16_t dir_id;
                ifs.read(reinterpret_cast<char *>(&dir_id), sizeof(uint16_t));

                file_names.get()[dir_id] = dir_name;
            }
        }
    }

    if (ifs.tellg() % 4 != 0) {
        ifs.seekg(4 - (ifs.tellg() % 4), ios::cur);
    }

    FileImages fi;
    ifs.read(reinterpret_cast<char *>(&fi), sizeof(FileImages));

    if (fi.id != FIMG_ID) {
        return NarcError::InvalidFileImagesId;
    }

    fs::create_directory(dst_dir);
    fs::current_path(dst_dir);

    if (fnt.chunk_size == 0x10) {
        for (uint16_t i = 0; i < fat.num_files; i++) {
            ifs.seekg(static_cast<uint64_t>(header.chunk_size) + fat.chunk_size + fnt.chunk_size + 8 + fat_entries.get()[i].start);

            auto &fat_entry = fat_entries.get()[i];
            const auto fat_entry_size = fat_entry.end - fat_entry.start;
            unique_ptr<char[]> buf = make_unique<char[]>(fat_entry_size);
            ifs.read(buf.get(), fat_entry_size);

            ostringstream oss;
            oss << src_file.stem().string() << "_" << setfill('0') << setw(8) << i << ".bin";

            ofstream ofs(oss.str(), ios::binary);
            if (!ofs.good()) {
                return NarcError::InvalidOutputFile;
            }

            ofs.write(buf.get(), fat_entry_size);
            ofs.close();
        }
    } else {
        fs::path absolute_path = fs::absolute(fs::current_path());

        for (size_t i = 0; i < fnt_entries.size(); i++) {
            fs::current_path(absolute_path);
            stack<string> dirs;
            auto &fnt_entry = fnt_entries[i];

            for (uint16_t j = fnt_entry.util; j > 0xF000; j = fnt_entries[j - 0xF000].util) {
                dirs.push(file_names.get()[j]);
            }

            for (; !dirs.empty(); dirs.pop()) {
                fs::create_directory(dirs.top());
                fs::current_path(dirs.top());
            }

            if (fnt_entry.util >= 0xF000) {
                fs::create_directory(file_names.get()[0xF000 + i]);
                fs::current_path(file_names.get()[0xF000 + i]);
            }

            ifs.seekg(fnt_entries_start + fnt_entry.offset);

            uint16_t file_id = 0;
            for (uint8_t len = 0x80; len != 0x00; ifs.read(reinterpret_cast<char *>(&len), sizeof(uint8_t))) {
                if (len <= 0x7F) {
                    streampos old_pos = ifs.tellg();
                    auto &fat_entry = fat_entries.get()[fnt_entry.first_file_id + file_id];
                    auto &file_name = file_names.get()[fnt_entry.first_file_id + file_id];
                    const auto fat_entry_size = fat_entry.end - fat_entry.start;

                    ifs.seekg(static_cast<uint64_t>(header.chunk_size) + fat.chunk_size + fnt.chunk_size + 8 + fat_entry.start);

                    unique_ptr<char[]> buf = make_unique<char[]>(fat_entry_size);
                    ifs.read(buf.get(), fat_entry_size);

                    ofstream ofs(file_name, ios::binary);
                    if (!ofs.good()) {
                        return NarcError::InvalidOutputFile;
                    }

                    ofs.write(buf.get(), fat_entry_size);
                    ofs.close();

                    ifs.seekg(old_pos);
                    ifs.seekg(len, ios::cur);

                    file_id++;
                } else if (len == 0x80) {
                    // reserved
                } else {
                    ifs.seekg(static_cast<uint64_t>(len) - 0x80 + 0x02, ios::cur);
                }
            }
        }
    }

    return NarcError::None;
}

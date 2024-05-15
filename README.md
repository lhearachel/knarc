# knarc
A tool that extracts the contents of Nitro Archives.

Forked from https://github.com/kr3nshaw/knarc

# Usage
```
Usage: knarc [--help] [--version] [--directory DIRECTORY] [[--pack TARGET]|[--unpack SOURCE]] [--filename-table] [--naix] [--prefix-header-entries] [--ignore IGNORE_FILE] [--keep KEEP_FILE] [--order ORDER_FILE] [--version-zero] [--verbose] [FILES]...

Utility for un/packing Nitro Archives for the Nintendo DS

Positional arguments:
  FILES                      list of files to pack into the destination NARC; if specified, -d, -i, -k, and -o will be ignored [nargs: 0 or more]

Optional arguments:
  -h, --help                 shows help message and exits
  -v, --version              prints version information and exits
  -d, --directory DIRECTORY  directory to be packed or to dump unpacked files
  -p, --pack TARGET          name of a NARC to be packed from DIRECTORY
  -u, --unpack SOURCE        name of a NARC to be unpacked into DIRECTORY
  -f, --filename-table       build the filename table
  -n, --naix                 output a C-style .naix header
  --prefix-header-entries    prefix entries in an output .naix header with TARGET; dependent on --naix
  -i, --ignore IGNORE_FILE   specify a file listing file-patterns to ignore for packing
  -k, --keep KEEP_FILE       specify a file listing file-patterns to keep during packing; listed patterns override those matching patterns in IGNORE_FILE
  -o, --order ORDER_FILE     specify a file listing order of files for packing; listed files override those matching patterns in IGNORE_FILE
  -z, --version-zero         output the NARC as version 0 spec
  --verbose                  output additional program messages
```

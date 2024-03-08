project('knarc', 'cpp')

c_srcs = []

if build_machine.system() == 'windows'
    c_srcs += 'fnmatch.c'
endif

cpp_srcs = [
    'Source.cpp',
    'Narc.cpp',
]

c_args = [
    '-O2',
    '-Wall',
    '-Wno-switch',
]

cpp_args = [
    '-std=c++17',
    '-O2',
    '-Wall',
    '-Wno-switch',
]

knarc_exe = executable('knarc',
    sources: [
        c_srcs,
        cpp_srcs,
    ],
    c_args: c_args,
    cpp_args: cpp_args,
    native: true,
)

meson.override_find_program('knarc', knarc_exe)


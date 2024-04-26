project('knarc', 'cpp')

c_srcs = []

if build_machine.system() == 'windows'
    c_srcs += 'fnmatch.c'
endif

cpp_srcs = [
    'main.cpp',
    'narc.cpp',
]

c_args = [
    '-O2',
    '-Wall',
    '-Wno-switch',
    '-Wpedantic',
    '-Wextra',
    '-Werror',
]

cpp_args = [
    '-std=c++20',
    '-O2',
    '-Wall',
    '-Wno-switch',
    '-Wpedantic',
    '-Wextra',
    '-Werror',
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


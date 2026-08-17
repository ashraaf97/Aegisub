fs = import('fs')

# respack.py replaces the old respack.lua: the Lua version had to be run by a
# LuaJIT binary that the build itself produced, which forced an awkward
# bootstrap ordering.
respack = find_program(meson.project_source_root() / 'tools/respack.py')

bitmap_manifest = '../bitmaps/manifest.respack'
bitmap_files = []
foreach bfile : fs.read(bitmap_manifest).strip().split('\n')
    bfile_s = bfile.strip()
    if bfile_s != ''
        bitmap_files += files('../bitmaps' / bfile_s)
    endif
endforeach

resrc = [
    custom_target('bitmap.{cpp,h}',
                  command: [respack, '@INPUT@', '@OUTPUT@'],
                  input: files(bitmap_manifest),
                  depend_files: bitmap_files,
                  output: ['bitmap.cpp', 'bitmap.h'])
]

# The platform config is a plain copy on Windows and a substituted template
# elsewhere, since only the non-Windows build needs DEFAULT_PLAYER_AUDIO
# baked in at configure time.
if host_machine.system() == 'windows'
    conf_platform_json = configure_file(input: 'default_config_win.json',
                                        output: 'default_config_platform.json',
                                        copy: true)
else
    conf_platform_json = configure_file(input: 'default_config_platform.json.in',
                                        output: '@BASENAME@',
                                        configuration: conf_platform)
endif

if host_machine.system() == 'darwin'
    resmanifest = 'manifest_osx.respack'
else
    resmanifest = 'manifest.respack'
endif

# default_config_platform.json is generated into the build directory rather
# than listed here; respack.py looks there as well as in the source tree.
resmanifest_files = [conf_platform_json]
foreach rfile : fs.read(resmanifest).strip().split('\n')
    rfile_s = rfile.strip()
    if rfile_s != '' and fs.is_file(rfile_s)
        resmanifest_files += files(rfile_s)
    endif
endforeach

resrc += custom_target('default_config.{cpp,h}',
                       command: [respack, '@INPUT0@', '@OUTPUT@'],
                       input: [files(resmanifest)],
                       depend_files: resmanifest_files,
                       output: ['default_config.cpp', 'default_config.h'])

libresrc_inc = include_directories('.')
libresrc = static_library('resrc', 'libresrc.cpp', resrc,
                          include_directories: [version_inc, deps_inc],
                          dependencies: deps)

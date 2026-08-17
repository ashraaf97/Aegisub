project('Aegisub', ['c', 'cpp'],
        license: 'BSD-3-Clause',
        meson_version: '>=1.0.0',
        default_options: ['cpp_std=c++17', 'buildtype=debugoptimized'],
        version: '3.2.2')

# Meson replaces both the autotools build and the MSBuild solution. The
# autotools tree is still present but is no longer the supported path on
# Windows; see README.md.

cmake = import('cmake')

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')

if host_machine.system() == 'windows'
    add_project_arguments('-DNOMINMAX', language: ['c', 'cpp'])
    add_project_arguments('-DUNICODE', '-D_UNICODE', language: ['c', 'cpp'])
    add_project_arguments('-DWIN32', '-D_WINDOWS', language: ['c', 'cpp'])

    # Boost's own auto-linking picks libraries by filename, which does not
    # survive Meson's dependency handling; link them explicitly instead.
    add_project_arguments('-DBOOST_ALL_NO_LIB', language: 'cpp')
    add_project_arguments('-DBOOST_HAS_ICU', language: 'cpp')
    add_project_arguments('-DBOOST_SYSTEM_NO_DEPRECATED', language: 'cpp')
    add_project_arguments('-DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION', language: 'cpp')
    add_project_arguments('-DU_STATIC_IMPLEMENTATION', language: ['c', 'cpp'])

    if cxx.get_id() == 'msvc'
        # Sources are UTF-8; without this MSVC guesses the active codepage.
        add_project_arguments('/utf-8', language: ['c', 'cpp'])
        # C4345: behaviour change for POD value-initialisation. Noise only.
        add_project_arguments('/wd4345', language: 'cpp')
    endif

    if not get_option('csri').disabled()
        add_global_arguments('-DCSRI_NO_EXPORT', language: 'c')
    endif

    # libass builds its assembly optimisations with nasm. Resolve it before
    # any subproject is configured so libass picks it up.
    sys_nasm = find_program('nasm', required: false)
    if not sys_nasm.found()
        nasm = subproject('nasm').get_variable('nasm')
        meson.override_find_program('nasm', nasm)
    endif
endif

# git_version.h -- regenerated on every build, but only rewritten when the
# contents actually change, so it does not force needless rebuilds.
if host_machine.system() == 'windows'
    # Invoke PowerShell explicitly rather than relying on the script shebang:
    # that resolves to Windows PowerShell and loads the user's profile, which
    # can fail for reasons that have nothing to do with this build.
    powershell = find_program('pwsh', 'powershell')
    version_command = [powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                       '-File', files('tools/version.ps1')]
else
    version_command = [find_program('build/version.sh')]
endif
version_inc = include_directories('.')
version_h = custom_target('git_version.h',
                          command: version_command + [meson.current_build_dir(), meson.current_source_dir()],
                          build_by_default: true,
                          build_always_stale: true,
                          output: ['git_version.h'])

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')
dataroot = datadir / 'aegisub'
docdir = prefix / 'doc'

# MSVC derives this from /MDd; other compilers need it spelled out.
if get_option('debug') and host_machine.system() != 'windows'
    add_project_arguments('-D_DEBUG', language: 'cpp')
endif

conf = configuration_data()
conf.set_quoted('P_BIN', bindir)
conf.set_quoted('P_DATA', dataroot)
conf.set_quoted('P_LOCALE', localedir)
if get_option('credit') != ''
    conf.set_quoted('BUILD_CREDIT', get_option('credit'))
endif
if get_option('enable_update_checker')
    conf.set('WITH_UPDATE_CHECKER', 1)
    conf.set_quoted('UPDATE_CHECKER_SERVER', get_option('update_server'))
    conf.set_quoted('UPDATE_CHECKER_BASE_URL', get_option('update_url'))
endif

deps = []
deps_inc = []

if host_machine.system() != 'windows'
    conf.set('WITH_FONTCONFIG', 1)
    deps += dependency('fontconfig')
endif

deps += cc.find_library('m', required: false)
deps += cc.find_library('dl', required: false)

iconv_dep = dependency('iconv', fallback: ['iconv', 'libiconv_dep'])
deps += iconv_dep

deps += dependency('libass', version: '>=0.9.7', fallback: ['libass', 'libass_dep'])

boost_modules = ['chrono', 'filesystem', 'thread', 'locale', 'regex']
boost_dep = []
if not get_option('local_boost')
    boost_dep = dependency('boost', version: '>=1.70.0',
                           modules: boost_modules + ['system'],
                           required: false,
                           static: get_option('default_library') == 'static')
endif

if get_option('local_boost') or not boost_dep.found()
    boost_dep = []
    boost = subproject('boost')
    foreach module: boost_modules
        boost_dep += boost.get_variable('boost_' + module + '_dep')
    endforeach
endif
deps += boost_dep

if host_machine.system() == 'windows'
    conf.set('BOOST_USE_WINDOWS_H', 1)
endif

deps += dependency('zlib')

# wxWidgets 3.2 is what gives us dark mode and per-monitor DPI on Windows 11.
wx_minver = '>=' + get_option('wx_version')
wx_dep = dependency('wxWidgets', version: wx_minver,
                    required: false,
                    modules: ['std', 'stc', 'gl'])

if wx_dep.found()
    deps += wx_dep
else
    build_shared = get_option('default_library') != 'static'
    build_type = get_option('buildtype') == 'debug' ? 'Debug' : 'Release'

    opt_var = cmake.subproject_options()
    opt_var.set_override_option('cpp_std', 'c++17')
    opt_var.add_cmake_defines({
        'wxBUILD_INSTALL': false,
        'wxBUILD_PRECOMP': 'OFF',   # otherwise breaks project generation w/ meson
        'wxBUILD_SHARED': build_shared,
        'wxBUILD_MONOLITHIC': true, # otherwise breaks project generation w/ meson
        'wxUSE_WEBVIEW': false,
        # wxWebRequest is how the AI client talks to Ollama Cloud: it wraps
        # WinHTTP here and NSURLSession/libcurl elsewhere, so we get HTTPS
        # without taking on OpenSSL or libcurl ourselves. On by default, but
        # this is a custom monolithic build, so say so explicitly.
        'wxUSE_WEBREQUEST': true,
        'wxUSE_REGEX': 'builtin',
        'wxUSE_IMAGE': true,
        'CMAKE_BUILD_TYPE': build_type,
    })

    wx = cmake.subproject('wxWidgets', options: opt_var)
    deps += [
        wx.dependency('wxmono'),
        wx.dependency('wxregex'),
        wx.dependency('wxscintilla'),
    ]

    if host_machine.system() == 'windows'
        deps += [
            wx.dependency('wxpng'),
            wx.dependency('wxzlib'),
            wx.dependency('wxexpat'),
        ]

        if cc.has_header('rpc.h')
            deps += cc.find_library('rpcrt4', required: true)
        else
            error('Missing Windows SDK RPC library (rpc.h / rpcrt4.lib)')
        endif
        if cc.has_header('commctrl.h')
            deps += cc.find_library('comctl32', required: true)
        else
            error('Missing Windows SDK Common Controls library (commctrl.h / comctl32.lib)')
        endif
    endif
endif

deps += dependency('icu-uc', version: '>=4.8.1.1')
deps += dependency('icu-i18n', version: '>=4.8.1.1')

# Optional dependencies. dep_avail records what was actually found so that
# src/meson.build can pick the matching source files and so the default audio
# output can be resolved below.
dep_avail = []
foreach dep: [
    # audio output, in order of precedence
    ['libpulse', [], 'PulseAudio', []],
    ['alsa', [], 'ALSA', []],
    ['portaudio-2.0', [], 'PortAudio', []],
    ['openal', '>=0.0.8', 'OpenAL', []],
    # video
    ['ffms2', '>=2.22', 'FFMS2', ['ffms2', 'ffms2_dep']],
    # other
    ['fftw3', [], 'FFTW3', []],
    ['hunspell', [], 'Hunspell', ['hunspell', 'hunspell_dep']],
    ['uchardet', [], 'uchardet', ['uchardet', 'uchardet_dep']],
]
    optname = dep[0].split('-')[0]
    if not get_option(optname).disabled()
        if dep[3].length() > 0
            d = dependency(dep[0], version: dep[1], fallback: dep[3])
        else
            d = dependency(dep[0], version: dep[1], required: false)
        endif

        if d.found()
            deps += d
            conf.set('WITH_@0@'.format(optname.to_upper()), 1)
            dep_avail += dep[2]
        elif get_option(optname).enabled()
            error('@0@ enabled but not found'.format(dep[2]))
        endif
    endif
endforeach

if get_option('avisynth').enabled()
    conf.set('WITH_AVISYNTH', 1)
    dep_avail += 'AviSynth'
    if host_machine.system() == 'windows'
        deps += cc.find_library('avifil32', required: true)
    endif
endif

if host_machine.system() == 'windows'
    # DirectSound needs nothing beyond the modern Windows SDK now: the
    # June 2010 DirectX SDK is no longer a prerequisite.
    if not get_option('directsound').disabled()
        have_dsound_h = cc.has_header('dsound.h')
        dsound_dep = cc.find_library('dsound', required: get_option('directsound'))
        winmm_dep = cc.find_library('winmm', required: get_option('directsound'))
        ole32_dep = cc.find_library('ole32', required: get_option('directsound'))
        dxguid_dep = cc.find_library('dxguid', required: get_option('directsound'))

        if not have_dsound_h and get_option('directsound').enabled()
            error('DirectSound enabled but dsound.h not found')
        endif

        if have_dsound_h and dsound_dep.found() and winmm_dep.found() and ole32_dep.found() and dxguid_dep.found()
            deps += [dsound_dep, winmm_dep, ole32_dep, dxguid_dep]
            conf.set('WITH_DIRECTSOUND', 1)
            dep_avail += 'DirectSound'
        endif
    endif

    # XAudio2 is the modern replacement and ships with the Windows SDK.
    if not get_option('xaudio2').disabled()
        have_xaudio_h = cc.has_header('xaudio2.h')
        xaudio2_dep = cc.find_library('xaudio2', required: get_option('xaudio2'))

        if not have_xaudio_h and get_option('xaudio2').enabled()
            error('XAudio2 enabled but xaudio2.h not found')
        endif

        if have_xaudio_h and xaudio2_dep.found()
            deps += xaudio2_dep
            conf.set('WITH_XAUDIO2', 1)
            dep_avail += 'XAudio2'
            # XAudio2 requires Windows 8 or newer.
            add_project_arguments('-D_WIN32_WINNT=0x0602', language: ['c', 'cpp'])
        else
            add_project_arguments('-D_WIN32_WINNT=0x0601', language: ['c', 'cpp'])
        endif
    else
        add_project_arguments('-D_WIN32_WINNT=0x0601', language: ['c', 'cpp'])
    endif

    if cc.has_header('dwrite_3.h')
        conf.set('HAVE_DWRITE_3', 1)
    endif
endif

def_audio = get_option('default_audio_output')
if def_audio != 'auto'
    if not dep_avail.contains(def_audio)
        error('Default audio output "@0@" selected but not available'.format(def_audio))
    endif
elif dep_avail.length() != 0
    def_audio = dep_avail[0]
else
    def_audio = ''
endif

conf_platform = configuration_data()
conf_platform.set('DEFAULT_PLAYER_AUDIO', def_audio)

luajit = dependency('luajit', version: '>=2.0.0', required: get_option('system_luajit'))
use_system_luajit = false
if luajit.found() and luajit.type_name() != 'internal' and not meson.is_cross_build()
    # Aegisub's Lua code relies on LuaJIT being built in 5.2 compatibility
    # mode; a system LuaJIT without it would fail at runtime instead.
    luajit_test = cc.run('''#include <lauxlib.h>
int main(void)
{
    lua_State *L = luaL_newstate();
    if (!L) return 1;
    // This is valid in lua 5.2, but a syntax error in 5.1
    const char testprogram[] = "function foo() while true do break return end end";
    return luaL_loadstring(L, testprogram) == LUA_ERRSYNTAX;
}''', dependencies: luajit)

    if luajit_test.returncode() == 1
        if get_option('system_luajit')
            error('System luajit found but not compiled in 5.2 mode')
        else
            message('System luajit found but not compiled in 5.2 mode; using the bundled copy')
        endif
    else
        use_system_luajit = true
    endif
endif

if use_system_luajit
    deps += luajit
    luajit_inc = include_directories(luajit.get_variable('includedir'))
else
    message('Using the bundled luajit')
    luajit_sp = subproject('luajit')
    luajit_inc = luajit_sp.get_variable('src_inc')
    deps += luajit_sp.get_variable('luajit_dep')
endif

subdir('vendor/luabins/src')

dep_gl = dependency('gl', required: false)
if not dep_gl.found()
    if host_machine.system() == 'windows'
        dep_gl = cc.find_library('opengl32', required: true)
    else
        dep_gl = cc.find_library('GL', required: true)
    endif
endif
deps += dep_gl

if not get_option('csri').disabled() and host_machine.system() == 'windows'
    conf.set('WITH_CSRI', 1)
    csri_sp = subproject('csri')
    deps += csri_sp.get_variable('csri_dep')
endif

acconf = configure_file(output: 'acconf.h', configuration: conf)

subdir('automation')
subdir('libaegisub')
subdir('po')
subdir('src')

if not get_option('tests').disabled() and not meson.is_cross_build()
    subdir('tests')
endif

aegisub_cpp_pch = ['src/include/agi_pre.h']

link_args = []
link_depends = []
if host_machine.system() == 'windows'
    # Embedding the manifest is what makes the app per-monitor DPI aware and
    # gives it the v6 common controls.
    manifest_file = configure_file(copy: true,
                                   input: 'src/res/aegisub.exe.manifest',
                                   output: 'aegisub.exe.manifest')
    link_args += ['/MANIFEST:EMBED', '/MANIFESTINPUT:@0@'.format(manifest_file)]
    link_depends += manifest_file
endif

aegisub = executable('aegisub', aegisub_src, version_h, acconf, resrc,
                     link_with: [libresrc, libluabins, libaegisub],
                     link_args: link_args,
                     link_depends: link_depends,
                     include_directories: [libaegisub_inc, libresrc_inc, version_inc,
                                           deps_inc, include_directories('src')],
                     cpp_pch: aegisub_cpp_pch,
                     install: true,
                     install_dir: bindir,
                     dependencies: deps,
                     win_subsystem: 'windows')

summary({
    'Build type': get_option('buildtype'),
    'C++ standard': get_option('cpp_std'),
}, section: 'Build')

summary({
    'Audio/video backends': ' '.join(dep_avail),
    'Default audio output': def_audio == '' ? 'none' : def_audio,
    'Update checker': get_option('enable_update_checker'),
}, section: 'Features')

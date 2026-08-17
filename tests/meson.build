gtest_proj = subproject('gtest')

test_deps = [
    gtest_proj.get_variable('gtest_dep'),
    gtest_proj.get_variable('gmock_dep'),
    iconv_dep,
    boost_dep,
]

test_sources = [
    'support/main.cpp',
    'support/util.cpp',

    'tests/access.cpp',
    'tests/audio.cpp',
    'tests/cajun.cpp',
    'tests/calltip_provider.cpp',
    'tests/character_count.cpp',
    'tests/color.cpp',
    'tests/dialogue_lexer.cpp',
    'tests/format.cpp',
    'tests/fs.cpp',
    'tests/hotkey.cpp',
    'tests/iconv.cpp',
    'tests/ifind.cpp',
    'tests/karaoke_matcher.cpp',
    'tests/keyframe.cpp',
    'tests/line_iterator.cpp',
    'tests/line_wrap.cpp',
    'tests/mru.cpp',
    'tests/option.cpp',
    'tests/path.cpp',
    'tests/signals.cpp',
    'tests/split.cpp',
    'tests/syntax_highlight.cpp',
    'tests/thesaurus.cpp',
    'tests/time.cpp',
    'tests/type_name.cpp',
    'tests/util.cpp',
    'tests/uuencode.cpp',
    'tests/vfr.cpp',
    'tests/word_split.cpp',
]

test_inc = include_directories('support')

runner = executable('gtest-main', test_sources,
                    include_directories: [test_inc, libaegisub_inc, version_inc, deps_inc],
                    dependencies: test_deps,
                    link_with: [libaegisub])

# The tests read fixture files from the working directory, so they are run
# from the build root where setup.bat/setup.sh stages the data.
test('gtest main', runner, workdir: meson.project_build_root())

if host_machine.system() == 'windows'
    setup_script = find_program('setup.bat')
else
    setup_script = find_program('setup.sh')
endif

custom_target('setup-test-data',
              input: runner,
              output: 'applied_test_setup',
              command: [setup_script, meson.project_build_root()],
              build_by_default: true)

#!/usr/bin/env python3

import os
import re
import subprocess
import sys

is_bad_lib = re.compile(r'(/usr/local|/opt)').match
is_sys_lib = re.compile(r'(/usr|/System)').match
otool_libname_extract = re.compile(r'\s+(/.*?)[\(\s:]').search
goodlist = []
badlist = []
link_map = {}


def otool(args):
    return subprocess.run(['otool'] + args, check=True, capture_output=True,
                          text=True).stdout.splitlines(keepends=True)


def collectlibs(lib, masterlist, targetdir):
    liblist = otool(['-L', lib])
    locallist = []

    for line in liblist:
        lr = otool_libname_extract(line)
        if not lr:
            continue
        l = lr.group(1)
        if is_bad_lib(l) and l not in badlist:
            badlist.append(l)
        if ((not is_sys_lib(l)) or is_bad_lib(l)) and l not in masterlist:
            locallist.append(l)
            print("found %s:" % l)

            check = l
            link_list = []
            while check:
                if os.path.isfile(check) and not os.path.islink(check):
                    subprocess.run(['cp', check, targetdir], check=True)
                    print("    FILE %s ... copied to target" % check)
                    for link in link_list:
                        link_map[link] = os.path.basename(check)
                    break

                if os.path.islink(check):
                    print("    LINK %s" % check)
                    link_list.append(os.path.basename(check))
                    check = os.path.dirname(check) + "/" + os.readlink(check)
                else:
                    break

        elif l not in goodlist and l not in masterlist:
            goodlist.append(l)
    masterlist.extend(locallist)

    for l in locallist:
        collectlibs(l, masterlist, targetdir)


binname = sys.argv[1]
targetdir = os.path.dirname(binname)
print("Searching for libraries in ", binname, "...")
libs = [binname]
collectlibs(sys.argv[1], libs, targetdir)

print()
print("System libraries used...")
goodlist.sort()
for l in goodlist:
    print(l)

print()
print("Fixing library install names...")
in_tool_args = []
for lib in libs:
    libbase = os.path.basename(lib)
    if libbase in link_map:
        libbase = link_map[libbase]
    in_tool_args += ['-change', lib, '@executable_path/%s' % libbase]

for lib in libs:
    libbase = os.path.basename(lib)

    if libbase in link_map:
        libbase = link_map[libbase]
        print("%s -> @executable_path/%s (REMAPPED)" % (lib, libbase))
    else:
        print("%s -> @executable_path/%s" % (lib, libbase))

    subprocess.run(['install_name_tool'] + in_tool_args
                   + ['-id', '@executable_path/%s' % libbase,
                      '%s/%s' % (targetdir, libbase)], check=True)
    sys.stdout.flush()

if badlist:
    print()
    print("WARNING: The following libraries have blacklisted paths:")
    for lib in sorted(badlist):
        print(lib)
    print("These paths normally have files from a package manager, which means that end result may not work if copied to another machine.")

print()
print("All done!")

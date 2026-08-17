#!/usr/bin/env python3
# Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Aegisub Project http://www.aegisub.org/

"""Pack the files listed in a manifest into a C++ source/header pair.

This is a Python port of tools/respack.lua. The Lua original had to be run
by a LuaJIT binary that the build itself produced, which forced an awkward
bootstrap ordering; Python is available before anything is compiled, so the
resource blobs can be generated as an ordinary build step.

Files are looked up in the manifest's own directory first and then in the
output directory, so that generated inputs (default_config_platform.json)
work in an out-of-tree build.

Usage: respack.py <manifest> <c++ file> <header>
"""

import os
import sys


def main(argv):
    if len(argv) != 4:
        sys.stderr.write('Usage: respack.py <manifest>[in] <c++ file>[out] <header>[out]\n')
        return 1

    manifest_path, cpp_path, h_path = argv[1:]

    with open(manifest_path, 'r') as manifest:
        entries = [line.strip() for line in manifest if line.strip() != '']

    source_dir = os.path.dirname(manifest_path)
    build_dir = os.path.dirname(cpp_path)

    resolved = []
    for entry in entries:
        candidates = [os.path.join(source_dir, entry), os.path.join(build_dir, entry)]
        for candidate in candidates:
            if os.path.isfile(candidate):
                resolved.append((entry, candidate))
                break
        else:
            sys.stderr.write("{}: Failed to open '{}'\n".format(manifest_path, entry))
            return 1

    with open(cpp_path, 'w') as cpp, open(h_path, 'w') as header:
        cpp.write('#include "libresrc.h"\n')

        for entry, path in resolved:
            with open(path, 'rb') as f:
                data = f.read()

            # The identifier is the bare filename: bitmaps/button/foo.png
            # becomes the symbol `foo`.
            name = os.path.splitext(os.path.basename(entry))[0]

            cpp.write('const unsigned char {}[] = {{{}}};\n'.format(
                name, ','.join(str(b) for b in data)))
            header.write('extern const unsigned char {}[{}];\n'.format(name, len(data)))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

#!/usr/bin/env python3
# Copyright 2026 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import platform
import subprocess
import sys


def main():
    if len(sys.argv) < 4:
        print(
            "Usage: run.py <tool> <source> <tool arguments...>",
            file=sys.stderr,
        )
        return 1

    tool = sys.argv[1]
    source = sys.argv[2]
    if not os.path.exists(tool) or os.path.getmtime(source) > os.path.getmtime(tool):
        tool_dir = os.path.dirname(tool)
        if tool_dir:
            os.makedirs(tool_dir, exist_ok=True)
        root_dir = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..", "..")
        )
        compiler_suffix = ".exe" if platform.system() == "Windows" else ""
        compiler = os.path.join(
            root_dir,
            "buildtools",
            "llvm",
            "bin",
            "clang++" + compiler_suffix,
        )
        subprocess.check_call([compiler, "-std=c++17", source, "-o", tool])

    return subprocess.call([os.path.abspath(tool)] + sys.argv[3:])


if __name__ == "__main__":
    sys.exit(main())

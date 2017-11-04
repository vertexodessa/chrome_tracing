#!/bin/sh

find . -name '*.c*' | xargs clang-check
find . -name '*.c*' | xargs clang-tidy -checks '*,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-cppcoreguidelines-pro-type-union-access' 2>&1| grep -v kipping


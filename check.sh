#!/bin/sh

find . -name '*.c*' | xargs clang-check
find . -name '*.c*' | xargs clang-tidy -checks \* 2>&1| grep -v kipping


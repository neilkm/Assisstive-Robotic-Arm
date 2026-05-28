#!/bin/sh
set -eu

cd "$(dirname "$0")"

build_dir="../../../builds/UART_Driver"

rm -rf "$build_dir"
cmake -S . -B "$build_dir" -G "Unix Makefiles"
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure

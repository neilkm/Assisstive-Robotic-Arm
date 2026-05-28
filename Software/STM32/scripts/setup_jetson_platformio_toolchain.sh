#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
package_dir="$project_dir/.pio/toolchain-gccarmnoneeabi"

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "arm-none-eabi-gcc was not found."
    echo "Install it on the Jetson first:"
    echo "  sudo apt update"
    echo "  sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi"
    exit 1
fi

rm -rf "$package_dir"
mkdir -p "$package_dir/bin"

cat > "$package_dir/package.json" <<'JSON'
{
  "name": "toolchain-gccarmnoneeabi",
  "version": "1.70201.0",
  "description": "System GNU Arm Embedded Toolchain wrapper for PlatformIO on Linux ARM64"
}
JSON

for tool in addr2line ar c++filt cpp elfedit gcc gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool gdb gdb-py gprof ld ld.bfd nm objcopy objdump ranlib readelf size strings strip; do
    if command -v "arm-none-eabi-$tool" >/dev/null 2>&1; then
        ln -s "$(command -v "arm-none-eabi-$tool")" "$package_dir/bin/arm-none-eabi-$tool"
    fi
done

echo "Created PlatformIO toolchain wrapper at:"
echo "  $package_dir"
echo
echo "Use the Jetson environment:"
echo "  pio run -e nucleo_f446re_jetson -t upload"

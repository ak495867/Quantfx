#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
os_name=${RUNNER_OS:-$(uname -s)}
case "$os_name" in
Linux)
    make check
    make platform
    ./ci/qemu_firmware.sh bios
    ;;
macOS)
    command -v nasm >/dev/null
    nasm -f macho64 platform/qfx_darwin.asm -o /tmp/qfx-darwin.o
    command -v as >/dev/null
    as platform/qfx_darwin_arm64.S -o /tmp/qfx-darwin-arm64.o
    ;;
Windows)
    command -v nasm >/dev/null
    nasm -f win64 platform/qfx_nt.asm -o /tmp/qfx-nt.obj
    nasm -f win64 platform/qfx_nt_syscalls.asm -o /tmp/qfx-nt-syscalls.obj
    ;;
*)
    echo UNKNOWN_RUNNER
    exit 1
    ;;
esac
echo TARGET_PASS:$os_name

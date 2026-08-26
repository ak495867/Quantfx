#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
make platform >/dev/null
file build/qfx-nt.exe | grep -q 'PE32+ executable'
file build/qfx.efi | grep -q 'EFI application'
file build/qfx-bios.bin | grep -q 'DOS/MBR boot sector'
test "$(od -An -tx2 -j510 -N2 build/qfx-bios.bin | tr -d ' \n')" = "aa55"
file build/qfx-darwin.o | grep -q 'Mach-O 64-bit x86_64 object'
file build/qfx-nt-syscalls.obj | grep -q 'Intel amd64 COFF object'
file build/qfx-timer.o | grep -q 'ELF 64-bit LSB relocatable'
test -s platform/qfx_darwin_arm64.S
echo PLATFORM_PASS

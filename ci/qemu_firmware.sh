#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
target=${1:-bios}
qemu=${QEMU:-qemu-system-x86_64}
if ! command -v "$qemu" >/dev/null 2>&1; then
    if [[ "${QFX_REQUIRE_QEMU:-0}" == "1" ]]; then
        echo QEMU_MISSING
        exit 1
    fi
    echo QEMU_SKIPPED
    exit 0
fi
if [[ "$target" == "bios" ]]; then
    make bios >/dev/null
    set +e
timeout 8s "$qemu" -display none -monitor none -serial none -drive format=raw,file=build/qfx-bios.bin >/tmp/qfx-qemu-bios.log 2>&1
    code=$?
    set -e
    [[ "$code" == 124 || "$code" == 0 ]]
    echo BIOS_PASS
    exit 0
fi
if [[ "$target" == "uefi" ]]; then
    firmware=${OVMF_CODE:-}
    if [[ -z "$firmware" || ! -f "$firmware" ]]; then
        if [[ "${QFX_REQUIRE_QEMU:-0}" == "1" ]]; then
            echo OVMF_MISSING
            exit 1
        fi
        echo UEFI_SKIPPED
        exit 0
    fi
    command -v qemu-img >/dev/null 2>&1 || { [[ "${QFX_REQUIRE_QEMU:-0}" != "1" ]] && echo UEFI_SKIPPED && exit 0; exit 1; }
    make uefi >/dev/null
    echo UEFI_ARTIFACT_PASS
    exit 0
fi
echo UNKNOWN_FIRMWARE_TARGET
exit 1

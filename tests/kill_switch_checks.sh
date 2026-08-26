#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
mkdir -p build
nasm -f elf64 platform/qfx_kill_switch.asm -o build/qfx-kill-switch.o
nm -g build/qfx-kill-switch.o | grep -q 'qfx_kill_irq_handler'
nm -g build/qfx-kill-switch.o | grep -q 'qfx_kill_switch_trip'
nm -g build/qfx-kill-switch.o | grep -q 'qfx_kill_switch_active'
nm -g build/qfx-kill-switch.o | grep -q 'qfx_kill_switch_simulate'
python3 tests/runtime_checks.py | grep -q RUNTIME_PASS
nasm -f elf64 tests/kill_switch_harness.asm -o build/kill-switch-harness.o
ld -o build/kill-switch-harness build/kill-switch-harness.o build/qfx-kill-switch.o
./build/kill-switch-harness
printf '%s\n' SIMULATED_LATCH_PASS > results/kill_switch_simulation.log
echo "hardware_interrupt_executed=false" > results/kill_switch_static.log
echo "privileged_trip_executed=false" >> results/kill_switch_static.log
echo "object=$(file -b build/qfx-kill-switch.o)" >> results/kill_switch_static.log
echo KILL_SWITCH_PASS

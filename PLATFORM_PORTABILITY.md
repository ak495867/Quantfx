# QuantFX Platform Portability

## Target matrix

| Target | Runtime model | Required backend | Intended capability | Current state |
|---|---|---|---|---|
| Linux x86-64 | ELF64 hosted process | Linux syscall and filesystem backend | full current CLI, backtest, live stream, journal, checkpoint, and routing baseline | validated |
| macOS x86-64 | Mach-O hosted process | Darwin syscall or libc ABI backend | research, backtest, routing, and adapter-host operation | backend contract defined; native build requires macOS SDK validation |
| macOS arm64 | Mach-O hosted process | Darwin arm64 ABI backend | research, backtest, routing, and adapter-host operation | backend contract defined; native build requires macOS SDK validation |
| Windows x86-64 | PE/COFF hosted process | Windows API or MinGW ABI backend | research, backtest, routing, and adapter-host operation | backend contract defined; Windows toolchain validation required |
| Windows arm64 | PE/COFF hosted process | Windows arm64 API backend | research, backtest, routing, and adapter-host operation | backend contract defined; Windows toolchain validation required |
| x86-64 bare metal | freestanding firmware | boot, serial, timer, storage, and network HAL | deterministic research/replay and constrained execution appliance | architecture defined; firmware and hardware validation required |
| arm64 bare metal | freestanding firmware | boot, UART, timer, storage, and network HAL | deterministic research/replay and constrained execution appliance | architecture defined; firmware and hardware validation required |

## Portability boundary

The quantitative core must not depend directly on Linux syscall numbers, ELF entry conventions, `/proc`, systemd, POSIX file descriptors, or a specific path format. It consumes a platform service table containing function pointers for exit, read, write, open, close, seek, sync, map, unmap, clock, sleep, mutex, atomic operations, and network transport.

Hosted targets use an adapter host process for broker and data-vendor APIs. The assembly engine receives canonical market and broker records through the stable wire protocol. It does not attempt to embed every vendor SDK or depend on a platform-specific GUI.

Bare-metal targets cannot safely run arbitrary broker SDKs, TLS stacks, or cloud APIs without a board-specific network and cryptography implementation. The bare-metal target is therefore a deterministic execution appliance and replay node until a validated board support package, network controller driver, clock source, secure key store, TLS stack, watchdog, and deployment process are supplied.

## Build policy

Each target has an explicit assembler format, object format, entry point, linker script, calling convention, and platform service table. A target is marked supported only after the executable starts on that target, passes canonical parser and routing tests, passes deterministic replay checks, and completes failure-injection tests for storage and transport errors.

The current repository contains the validated Linux backend and the cross-platform contract. macOS, Windows, and bare-metal support require their respective SDKs, linkers, board definitions, and runtime verification; those cannot be truthfully marked production-complete from a Linux-only build host.

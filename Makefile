PROJECT=qfx
BUILD=build
NASM=nasm
LD=ld
ASFLAGS=-f elf64 -Iinclude/
LDFLAGS=-dynamic-linker /lib64/ld-linux-x86-64.so.2

all: $(BUILD)/$(PROJECT)

platform: windows-pe uefi bios darwin-object darwin-arm64-source nt-syscalls-object timer-object kill-switch-object

qemu-firmware:
	./ci/qemu_firmware.sh bios

windows-pe: $(BUILD)/qfx_nt.obj
	$(LD) -mi386pep -e qfx_nt_start $< -o $(BUILD)/qfx-nt.exe

uefi: $(BUILD)/qfx_uefi.obj
	$(LD) -mi386pep --subsystem 10 -e efi_main $< -o $(BUILD)/qfx.efi

bios: $(BUILD)/qfx-bios.bin

darwin-object: $(BUILD)/qfx-darwin.o

timer-object: $(BUILD)/qfx-timer.o

kill-switch-object: $(BUILD)/qfx-kill-switch.o

darwin-arm64-source: platform/qfx_darwin_arm64.S

nt-syscalls-object: $(BUILD)/qfx-nt-syscalls.obj

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/qfx.o: src/qfx.asm include/qfx.inc | $(BUILD)
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD)/qfx_nt.obj: platform/qfx_nt.asm | $(BUILD)
	$(NASM) -f win64 $< -o $@

$(BUILD)/qfx_uefi.obj: platform/qfx_uefi.asm | $(BUILD)
	$(NASM) -f win64 $< -o $@

$(BUILD)/qfx-nt-syscalls.obj: platform/qfx_nt_syscalls.asm | $(BUILD)
	$(NASM) -f win64 $< -o $@

$(BUILD)/qfx-bios.bin: platform/qfx_bios.asm | $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/qfx-darwin.o: platform/qfx_darwin.asm | $(BUILD)
	$(NASM) -f macho64 $< -o $@

$(BUILD)/qfx-timer.o: platform/qfx_timer.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/qfx-kill-switch.o: platform/qfx_kill_switch.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/$(PROJECT): $(BUILD)/qfx.o
	$(LD) $(LDFLAGS) $< -o $@

check: all
	./tests/run_tests.sh
	./tests/platform_checks.sh
	python3 tests/runtime_checks.py
	./tests/kill_switch_checks.sh

clean:
	rm -rf $(BUILD)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/config $(DESTDIR)$(PREFIX)/docs
	install -m 0755 $(BUILD)/$(PROJECT) $(DESTDIR)$(PREFIX)/bin/$(PROJECT)
	install -m 0644 config/default.toml $(DESTDIR)$(PREFIX)/config/default.toml
	install -m 0644 docs/architecture.md $(DESTDIR)$(PREFIX)/docs/architecture.md
	install -m 0644 docs/usage.md $(DESTDIR)$(PREFIX)/docs/usage.md

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROJECT) $(DESTDIR)$(PREFIX)/config/default.toml $(DESTDIR)$(PREFIX)/docs/architecture.md $(DESTDIR)$(PREFIX)/docs/usage.md

.PHONY: all platform qemu-firmware windows-pe uefi bios darwin-object darwin-arm64-source nt-syscalls-object timer-object kill-switch-object check clean install uninstall

# =============================================================================
# Find My Keys beacon - build system
#
# Bare-metal nRF52840 firmware, no SDK. Targets:
#   make            build firmware.elf + firmware.hex + firmware.bin
#   make flash      flash + run + stream RTT logs   (probe-rs)
#   make rtt        attach to a running target's RTT (no reflash)
#   make erase      recover a locked board (APPROTECT) via full erase
#   make size       show flash/RAM usage
#   make test       build & run the host-side SipHash self-test (native gcc)
#   make lint       static analysis (cppcheck)
#   make format     apply clang-format in place
#   make format-check   verify formatting (used by CI)
#   make clean
# =============================================================================

# --- Toolchain ---------------------------------------------------------------
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

CHIP    ?= nRF52840_xxAA

# --- Project layout ----------------------------------------------------------
SRC_DIR := src
BUILD   := build
TARGET  := firmware
LDSCRIPT := linker/nrf52840.ld

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

# --- Flags -------------------------------------------------------------------
# Cortex-M4, Thumb, software float (FPU left off to keep startup minimal).
CPU := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# -ffreestanding + -nostdlib: no hosted C library. We supply memcpy/memset.
# -ffunction-sections/-fdata-sections + --gc-sections: drop unused code.
CFLAGS := $(CPU) -std=c11 -Os -g3 \
          -Wall -Wextra -Werror \
          -ffreestanding -ffunction-sections -fdata-sections \
          -fno-common -fno-builtin

LDFLAGS := $(CPU) -T$(LDSCRIPT) -nostdlib \
           -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
           -Wl,--print-memory-usage

# libgcc supplies compiler helpers (e.g. 64-bit shifts used by SipHash).
LIBS := -lgcc

# --- Default build -----------------------------------------------------------
.PHONY: all
all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# --- Size --------------------------------------------------------------------
.PHONY: size
size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

# --- Flash / debug (probe-rs) ------------------------------------------------
# `probe-rs run` downloads the ELF, resets, and streams RTT to your terminal.
.PHONY: flash
flash: $(BUILD)/$(TARGET).elf
	probe-rs run --chip $(CHIP) $<

# Attach without reflashing/resetting - inspect a running beacon.
.PHONY: rtt
rtt:
	probe-rs attach --chip $(CHIP) $(BUILD)/$(TARGET).elf

# Recover a board whose debug port is locked (see docs/04-security.md).
.PHONY: erase
erase:
	probe-rs erase --chip $(CHIP) --allow-erase-all

# --- Host-side unit test -----------------------------------------------------
# Compiles siphash.c with the *native* compiler and checks it against the
# official test vector. Same source that runs on the chip - proof it is correct.
.PHONY: test
test:
	$(MAKE) -C tools run

# --- Quality gates -----------------------------------------------------------
.PHONY: lint
lint:
	cppcheck --enable=warning,portability,performance --error-exitcode=1 \
	         --inline-suppr --std=c11 --quiet \
	         -I $(SRC_DIR) $(SRC_DIR)

.PHONY: format
format:
	clang-format -i $(SRC_DIR)/*.c $(SRC_DIR)/*.h tools/*.c

.PHONY: format-check
format-check:
	clang-format --dry-run --Werror $(SRC_DIR)/*.c $(SRC_DIR)/*.h tools/*.c

# --- Housekeeping ------------------------------------------------------------
.PHONY: clean
clean:
	rm -rf $(BUILD)
	$(MAKE) -C tools clean

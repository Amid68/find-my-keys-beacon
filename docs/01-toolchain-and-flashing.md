# 01 — Toolchain and flashing

*What actually happens between `make` and code running on the chip.*

## Cross-compilation

Your MacBook's CPU is aarch64 (64-bit ARM, Asahi Linux). The nRF52840's CPU is
a **Cortex-M4** — 32-bit ARMv7E-M, Thumb-2 instruction set only, no MMU, no
OS. Different architecture, different ABI, different everything; you cannot
run your host compiler's output there. So we use a *cross toolchain*:

```
arm-none-eabi-gcc
 │   │    │
 │   │    └── eabi: ARM's Embedded ABI (calling convention, stack layout)
 │   └── none: no operating system ("bare metal")
 └── target CPU family
```

`arm-none-eabi-gcc-cs` from Fedora gives you the compiler; `arm-none-eabi-newlib`
provides the C library headers (`stdint.h`, `stddef.h`, …). Note we link with
`-nostdlib`: we take newlib's *headers* but none of its code. Every function
the firmware calls exists in `src/`. The one exception is `-lgcc` — the
compiler runtime that provides helpers GCC itself emits calls to (e.g. 64-bit
shifts on a 32-bit CPU, which SipHash needs).

Flags worth understanding (see `Makefile`):

| Flag | Why |
| ---- | --- |
| `-mcpu=cortex-m4 -mthumb` | generate Thumb-2 code for our exact core |
| `-mfloat-abi=soft` | don't touch the FPU — we never enabled it in startup |
| `-ffreestanding` | "there is no OS, don't assume `main()` semantics or a hosted libc" |
| `-ffunction-sections` + `--gc-sections` | every function in its own section, linker drops unreferenced ones |
| `-Werror` | warnings in register-poking code are usually real bugs |
| `-Os -g3` | optimize for size; full debug info costs nothing at runtime (it stays in the ELF, not in flash) |

## What the build produces

- `firmware.elf` — code + symbols + debug info. What probe-rs flashes and
  what a debugger reads.
- `firmware.hex` / `firmware.bin` — just the raw flash contents, for other
  programmers. The `.bin` is literally the bytes at flash address 0: the
  first 4 bytes are the initial stack pointer, the next 4 the reset vector
  (see doc 02).
- `firmware.map` — where the linker put every symbol. When you wonder "why
  did flash grow 400 bytes", the answer is in here.

## The debug probe: what's actually on the DK

The nRF52840-DK has **two** chips: the nRF52840 target, and a separate
interface MCU running SEGGER J-Link firmware. The USB connector next to the
power switch (J2) talks to the J-Link chip, which talks to the target over
**SWD** (Serial Wire Debug — a 2-wire protocol giving full access to the
target's memory bus: halt the core, read/write RAM and registers, program
flash).

```
 USB ──▶ J-Link OB (interface MCU) ──SWD──▶ nRF52840 (target)
```

That is why flashing needs no bootloader and no special mode on the target:
the probe writes flash directly through the debug port.

## probe-rs

`probe-rs` is an open-source Rust replacement for vendor flashing tools. The
three commands used by the Makefile:

```sh
probe-rs run    --chip nRF52840_xxAA build/firmware.elf  # flash + reset + RTT
probe-rs attach --chip nRF52840_xxAA build/firmware.elf  # RTT only, no reset
probe-rs erase  --chip nRF52840_xxAA --allow-erase-all   # full chip recovery
```

`run` is the daily driver: it programs only changed flash sectors, resets the
core, then stays attached streaming RTT logs (Ctrl-C detaches; the beacon
keeps running — it does not depend on the probe).

## RTT: printf without a UART

Classic embedded logging uses a UART: wire protocol, baud rates, blocking
writes. **RTT (Real-Time Transfer)** is smarter: the target keeps a ring
buffer in its own RAM plus a small control block starting with the magic
string `"SEGGER RTT"`. The host (probe-rs) scans target RAM for the magic
over SWD, then simply reads the ring buffer *behind the target's back* —
memory reads over SWD don't interrupt the CPU at all.

We implement the whole thing in `src/rtt.c` (~200 lines). Two details worth
noticing in the code:

- The magic string is assembled byte-by-byte at runtime, so the only
  occurrence of `"SEGGER RTT"` in memory is the real control block — not a
  string constant in flash the host might find first.
- The writer is *non-blocking* ("trim" mode): if the buffer is full because
  no host is attached, log bytes are dropped rather than stalling the beacon.
  Logging must never change timing behavior — a lesson every embedded and
  every distributed-systems engineer learns exactly once.

## Fedora specifics: udev and the plugdev warning

USB device nodes are root-owned by default. The probe-rs udev rules grant
access two ways: `TAG+="uaccess"` (systemd grants access to locally
logged-in users — the modern Fedora way) and `GROUP="plugdev"` (the Debian
way). The warning you saw:

```
WARN probe_rs::probe::list::linux: The 'plugdev' group does not exist.
```

is harmless on Fedora — `uaccess` already covers you when logged in at the
machine. `scripts/setup-fedora.sh` creates the group anyway, which silences
the warning and also covers ssh sessions (which are not "locally logged in"
as far as uaccess is concerned).

`No debug probes were found` with the board unplugged is the expected output.
After plugging in J2 and switching the board on, `probe-rs list` should show
a J-Link entry.

## Reproducibility (the DevOps part)

The same `make` runs identically in CI (`.github/workflows/ci.yml`) on every
push, and artifacts are published with SHA-256 sums. `probe-rs verify` can
later confirm a board is running exactly a given image — closing the loop
from git commit → CI artifact → hash → silicon.

---

**Next:** [02 — Boot and memory](02-boot-and-memory.md) — what happens between
power-on and the first line of `main()`.

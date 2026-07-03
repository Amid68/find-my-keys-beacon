# 02 — Boot and memory

*The road from power-on to `main()`, with no startup files hiding it.*

Relevant sources: [`src/startup.c`](../src/startup.c),
[`linker/nrf52840.ld`](../linker/nrf52840.ld).

## The memory map

The Cortex-M has a single flat 4 GB address space. On the nRF52840 the
interesting regions are:

```
0x0000_0000 ┌─────────────────┐
            │  FLASH  (1 MB)  │  code, constants, vector table at offset 0
0x0010_0000 └─────────────────┘
0x1000_0000 ┌─────────────────┐
            │  FICR           │  factory info (device address!) — read-only
            ├─────────────────┤
            │  UICR           │  user config (persists across reflash)
            └─────────────────┘
0x2000_0000 ┌─────────────────┐
            │  RAM  (256 KB)  │  globals, stack, RTT buffer, the adv PDU
0x2004_0000 └─────────────────┘
0x4000_0000 ┌─────────────────┐
            │  peripherals    │  CLOCK, RADIO, RTC0, RNG, ... one 4 KB page each
            └─────────────────┘
0x5000_0000   GPIO
0xE000_0000   Cortex-M private peripherals (NVIC, SysTick, ...)
```

Two things fall out of this design that the whole firmware relies on:

1. **Peripherals are just memory.** `RADIO_TASKS_TXEN = 1;` is a normal
   32-bit store to address `0x40001000`. That is all "driving a peripheral"
   ever is; SDKs only wrap this in functions. Hence `src/nrf52840.h` — a list
   of addresses.
2. **`volatile` is non-negotiable.** The compiler assumes ordinary memory
   doesn't change behind its back and that redundant stores can be deleted.
   Both assumptions are false for hardware registers. Every register accessor
   in `nrf52840.h` goes through `volatile uint32_t *` so each read/write in
   the source is a real read/write on the bus, in order.

## What the CPU does at power-on

The Cortex-M4 hardware itself (before any code) does exactly two things:

1. Loads the **initial stack pointer** from address `0x0000_0000`.
2. Loads the **reset vector** (address of the first instruction) from
   `0x0000_0004`, and jumps to it.

That's why the very first thing in flash must be the *vector table* — an
array of addresses. Look at `startup.c`: `vector_table[]` is a plain C array
of function pointers placed in the `.vectors` section, and the linker script
pins that section to flash offset 0. Entry 0 is not code at all; it's the
value `_estack` (end of RAM), because the stack grows downward.

The rest of the table is exception and interrupt handlers, in an order fixed
by ARM (entries 1–15) and by Nordic (entry 16 + IRQ number). When the RTC0
peripheral fires interrupt 11, the hardware jumps to `vector_table[16+11]` —
no registration call, no OS; the table *is* the registration.

Every handler is declared with
`__attribute__((weak, alias("Default_Handler")))`: a do-nothing definition
that any strong definition elsewhere silently replaces. `rtc.c` defines
`RTC0_IRQHandler`, so its entry points there; every other IRQ parks in
`Default_Handler`'s infinite loop where a debugger can catch it.

## Why C needs a startup routine at all

C guarantees that globals are initialized before `main()`:

```c
static uint32_t counter = 42;   // must be 42 at main()
static uint8_t buffer[512];     // must be all zeros at main()
```

On a PC, the OS loader arranges this. Here there is no loader — flash is
just *there* at power-on and RAM is *garbage* at power-on. So:

- Initialized globals (`.data`): their values are stored in **flash** (they
  must survive power-off) but the variables live at **RAM addresses** (they
  must be writable). `Reset_Handler` copies the flash image to RAM, word by
  word. The linker script's `> RAM AT> FLASH` is what gives the section two
  addresses — *run* address in RAM, *load* address in flash — and
  `_sidata/_sdata/_edata` are linker-provided symbols marking the copy's
  source and bounds.
- Zero-initialized globals (`.bss`): no point storing a block of zeros in
  flash; the startup code just zeroes the range `_sbss.._ebss`.

Then it calls `main()`. That's the entire C runtime, ~15 lines. When people
say "the CRT" or "crt0", this is what they mean.

## The linker script, in one paragraph

`nrf52840.ld` declares the two memory regions (FLASH at 0, RAM at
0x20000000), then assigns sections: `.vectors` first in flash, then `.text`
(code) and `.rodata` (constants), then `.data` (RAM, load-image in flash),
then `.bss` (RAM only). `KEEP(*(.vectors))` stops `--gc-sections` from
discarding the vector table — nothing *references* it, the hardware just
knows where it is, and the linker can't see that.

After a build, verify the story with:

```sh
make size                       # text = flash, data+bss = RAM
arm-none-eabi-objdump -h build/firmware.elf   # section addresses
arm-none-eabi-nm -n build/firmware.elf | head # symbols in address order
```

You'll see `vector_table` at `0x0`, code right after it, and `_sdata` at
`0x20000000`.

## The stack (and what we didn't set up)

We set the initial SP and nothing else: no heap (`malloc` doesn't exist
here — nothing calls it), no FPU enable (we compile `-mfloat-abi=soft`), no
MPU. A beacon needs none of it, and each omission is one less thing between
you and the hardware. If you ever *do* enable the FPU, it's two register
writes in `Reset_Handler` — and the day you need that, you'll know exactly
where it goes.

---

**Next:** [03 — BLE advertising from scratch](03-ble-advertising.md) — the
radio itself.

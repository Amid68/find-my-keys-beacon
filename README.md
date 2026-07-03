# Find My Keys — a bare-metal BLE beacon for the nRF52840-DK

A "find my keys" Bluetooth Low Energy beacon written **from scratch**: no
Nordic SDK, no nrfx, no SoftDevice, no Zephyr, no CMSIS headers. Every
peripheral is driven by writing 32-bit values to memory-mapped registers whose
addresses come straight out of the nRF52840 Product Specification. The whole
firmware is ~1,300 lines of commented C and compiles to **under 3 KB of
flash**.

Any phone running a BLE scanner (nRF Connect, LightBlue) sees the beacon,
shows its signal strength (RSSI), and can track its proximity. On top of the
"hello BLE" basics, every advertisement carries a **SipHash-2-4 authenticated
rolling counter**, so a receiver holding the shared key can distinguish the
genuine beacon from a spoofed clone and detect replayed frames.

```
       nRF52840-DK                              iPhone / Linux
  ┌──────────────────────┐                   ┌──────────────────┐
  │  main loop           │   2.4 GHz, ch     │ nRF Connect app  │
  │   ├ read FICR MAC    │   37 / 38 / 39    │  name + RSSI     │
  │   ├ build raw PDU    │  ~~~~~~~~~~~~~~▶  │                  │
  │   ├ SipHash tag      │  ADV_NONCONN_IND  │ verify_beacon.py │
  │   └ RADIO TX ×3      │   every ~100 ms   │  checks the tag  │
  │  then WFE sleep      │                   └──────────────────┘
  └──────────────────────┘
```

## Why no SDK?

Because the SDK is exactly what hides the interesting parts. This project
exists to answer questions like: *how does a chip know its own MAC address?
What bytes actually go over the air in a BLE advertisement? What happens
between power-on and `main()`?* Every layer that normally comes for free is
implemented here, visibly:

| Normally provided by...        | Here implemented in...                    |
| ------------------------------ | ----------------------------------------- |
| CMSIS device headers           | `src/nrf52840.h` (hand-written, ~200 loc) |
| SDK startup files              | `src/startup.c` (vector table, .data/.bss)|
| SDK linker scripts             | `linker/nrf52840.ld`                      |
| The BLE stack / SoftDevice     | `src/radio.c` + `src/ble.c`               |
| `printf` over SEGGER RTT       | `src/rtt.c` (control block from scratch)  |
| libc `memcpy`/`memset`         | `src/util.c`                              |
| A crypto library               | `src/siphash.c` (spec-faithful, ~100 loc) |

## Repository layout

```
├── src/                  firmware sources (each file = one concern)
│   ├── nrf52840.h        the register map — start reading here
│   ├── config.h          all tunables: name, intervals, TX power, demo key
│   ├── startup.c         vector table + reset handler (how C starts)
│   ├── main.c            the advertising loop
│   ├── clock.c/.h        HFXO/LFXO crystal bring-up
│   ├── ficr.c/.h         factory MAC → BLE random-static address
│   ├── radio.c/.h        the RADIO peripheral: BLE PHY from registers
│   ├── ble.c/.h          hand-assembled advertising PDU (the byte array)
│   ├── auth.c/.h         rolling counter + truncated SipHash tag
│   ├── siphash.c/.h      SipHash-2-4, shared verbatim with the host tests
│   ├── rtc.c/.h          low-power interval timer (WFE sleep)
│   ├── rng.c/.h          hardware RNG for the BLE-mandated adv jitter
│   ├── board.c/.h        DK LEDs + button 1 ("lost mode")
│   ├── rtt.c/.h          debug logging over the debug probe, no UART
│   └── util.c/.h         freestanding memcpy/memset/delay
├── linker/nrf52840.ld    memory map: what goes in flash vs RAM and why
├── tools/
│   ├── test_siphash.c    host-side test against published SipHash vectors
│   └── verify_beacon.py  the "receiver": verifies tags, flags spoofs/replays
├── scripts/setup-fedora.sh   idempotent dev-environment bootstrap
├── .github/workflows/ci.yml  build + test + lint pipeline
├── docs/                 the teaching material (read in order)
│   ├── 01-toolchain-and-flashing.md
│   ├── 02-boot-and-memory.md
│   ├── 03-ble-advertising.md
│   ├── 04-security.md
│   └── 05-testing.md
└── Makefile              build / flash / rtt / size / test / lint / format
```

## Quick start

Tools (you have already installed these; `scripts/setup-fedora.sh` re-does it
idempotently and also silences the probe-rs `plugdev` warning):

```sh
sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib make cppcheck
# probe-rs installer + udev rules: see scripts/setup-fedora.sh
```

Build and test on the host — no board needed:

```sh
make          # cross-compile → build/firmware.{elf,hex,bin}, prints memory use
make size     # flash/RAM usage breakdown
make test     # run the SipHash core against published reference vectors
```

Flash and watch it run — board needed:

```sh
# Connect the DK's J2 USB (next to the power switch), switch ON, then:
probe-rs list                 # should show "J-Link" — see docs/01 if not
make flash                    # flash + reset + stream RTT logs to terminal
```

Expected RTT output:

```
=== Find My Keys beacon ===
name="FindMyKeys" interval=100ms
MAC (random static): e2:xx:xx:xx:xx:xx
advertising...
adv #64 sent
...
```

On the iPhone: install **nRF Connect** (Nordic, free), open the Scanner tab,
and look for **FindMyKeys**. RSSI updates as you move — that's your
proximity tracker. Tap the entry to see the manufacturer data: the rolling
counter and the SipHash tag, changing on every frame. Full walkthrough with
verification steps in [docs/05-testing.md](docs/05-testing.md).

**LED1** blinks as the advertising heartbeat. **Button 1** toggles *lost
mode*: the interval drops from 100 ms to 30 ms (faster RSSI updates → easier
hunting) and **LED2** lights while active.

## The security angle

Anyone can clone a dumb beacon: record one advertisement, replay it forever.
This beacon makes frames *verifiable*:

```
tag = SipHash-2-4( key, MAC ‖ version ‖ counter )   truncated to 4 bytes
```

The counter increments every advertising event and is sent in the clear next
to the tag. A receiver with the key ([tools/verify_beacon.py](tools/verify_beacon.py))
recomputes the tag — a **spoofed** frame fails the tag check, a **replayed**
frame fails the counter monotonicity check. Threat model, the reasoning
behind SipHash, truncation trade-offs, key handling, and what APPROTECT
does/doesn't protect: [docs/04-security.md](docs/04-security.md).

> ⚠️ `src/config.h` ships a **public demo key** so the repo works out of the
> box. Change it before pretending any of this is private.

## The DevOps angle

CI ([.github/workflows/ci.yml](.github/workflows/ci.yml)) runs on every push:

- **build** — warning-clean cross-compile (`-Werror`), memory budget printed,
  artifacts uploaded with a SHA-256 manifest (flash-image provenance: any
  image on any board can be traced to the commit that built it).
- **test** — the SipHash core vs. published reference vectors, natively
  compiled from the same source file the firmware uses; plus the independent
  Python implementation, including a must-fail forged-tag case.
- **lint** — cppcheck static analysis.

The workflow token is read-only (`permissions: contents: read`) and actions
come pinned from official orgs only. `make format` / `.clang-format` keep
style consistent locally; formatting is deliberately not a CI gate.

## Where to go next

Ideas that fit the codebase as designed: persist the counter in UICR to make
replay protection survive reboots; use RSSI on the receiver to build a
"hotter/colder" CLI; add TIMER-based precise inter-channel spacing; or port
`auth` verification into an iOS app with CoreBluetooth. The docs end with
pointers for each.

## Documentation map

| Read this...                                             | To learn...                                            |
| -------------------------------------------------------- | ------------------------------------------------------ |
| [docs/01-toolchain-and-flashing.md](docs/01-toolchain-and-flashing.md) | cross-compilation, probe-rs, SWD, RTT, udev on Fedora |
| [docs/02-boot-and-memory.md](docs/02-boot-and-memory.md) | vector table, linker script, .data/.bss, the road to `main()` |
| [docs/03-ble-advertising.md](docs/03-ble-advertising.md) | channels, access address, whitening, CRC, the PDU, the RADIO peripheral |
| [docs/04-security.md](docs/04-security.md)               | threat model, SipHash, replay/spoof defenses, APPROTECT |
| [docs/05-testing.md](docs/05-testing.md)                 | iPhone testing, Linux verifier, RTT, what "working" looks like |

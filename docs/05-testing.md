# 05 — Testing

*A test ladder from "no hardware needed" to "iPhone in hand". Climb it in
order — each rung isolates a different failure class.*

## Rung 0: host tests (no board)

```sh
make test
```

Compiles `src/siphash.c` — the same file the firmware builds — with your
native compiler and checks it against published SipHash-2-4 vectors,
including the worked example from the SipHash paper itself. Also prints a
sample beacon tag:

```
PASS len= 0  0x726fdb47dd0e0e31
...
sample beacon tag (demo key, MAC e2:2a:8f:50:f5:a3, ctr=1): 85 22 1e 92
all SipHash-2-4 vectors passed
```

Cross-check the independent Python implementation agrees:

```sh
python3 tools/verify_beacon.py --mac e2:2a:8f:50:f5:a3 --data 010100000085221e92
# VALID   tag OK (v1, counter 1)
```

Flip one hex digit of the tag and it must fail. CI runs all of this on
every push.

## Rung 1: it flashes and boots (board, no phone)

Plug USB into **J2** (the connector next to the power switch — not J3 at
the far edge, which is USB power only... and not the nRF USB port), switch
the board **ON**, then:

```sh
probe-rs list        # expect: J-Link (or similar) — 1 probe
make flash
```

Success looks like: probe-rs prints flash progress, then RTT streams:

```
=== Find My Keys beacon ===
name="FindMyKeys" interval=100ms
MAC (random static): e2:xx:xx:xx:xx:xx     ← your chip's unique address
advertising...
adv #64 sent                                ← one line every ~6.4 s
```

**LED1 blinking ≈5 Hz** is the advertising heartbeat — the firmware is
alive even without RTT attached. Write down the MAC; you'll want it below.

| Symptom | Likely cause |
| --- | --- |
| `No debug probes were found` | wrong USB port, power switch off, or udev (re-run `scripts/setup-fedora.sh`, replug) |
| flashes but no RTT output | `make rtt` to re-attach; RTT init runs early, so also try a power-cycle then `make flash` again |
| `Default_Handler` breakpoint / hang | an interrupt fired with no handler — did you edit the vector table? |

## Rung 2: the iPhone sees it

Install **nRF Connect for Mobile** (Nordic Semiconductor, free) on the
iPhone. Scanner tab, pull to refresh:

- **FindMyKeys** appears, updating around 10×/second.
- The RSSI number (e.g. −45 dBm) rises as the phone approaches the board
  and falls as you walk away. −40 ≈ same desk; −90 ≈ other end of the flat.
  Congratulations, that's the "find my keys" feature working: hotter/colder
  by signal strength. (Expect bouncing — 2.4 GHz indoors is multipath soup.)
- Tap the entry → **Manufacturer data (0xFFFF)**: `01` + 4 counter bytes
  (visibly incrementing, little-endian) + 4 tag bytes (changing every
  frame — that's the SipHash).
- Press **Button 1**: RTT prints `lost mode ON`, LED2 lights, and the
  update rate in the app visibly jumps (30 ms interval). Press again to
  leave.

Two iOS quirks worth knowing (they confuse everyone once): iOS **hides the
advertiser's MAC** from apps and substitutes a random UUID, so don't look
for `e2:...` in nRF Connect — identify the beacon by name. And the
advertisement is *non-connectable* (doc 03), so "CONNECT" failing is
correct behavior, not a bug.

### Verifying the tag from the phone (offline mode)

In nRF Connect, long-press the manufacturer data value to copy its hex
(iOS shows the payload after the company id), then on the laptop:

```sh
python3 tools/verify_beacon.py --mac <MAC from RTT> --data <pasted hex>
```

`VALID tag OK (v1, counter N)` closes the whole loop: firmware-computed
tag, phone-relayed bytes, independently verified on the host.

## Rung 3: live spoof/replay detection (Linux BLE)

If your MacBook's BLE works under Asahi (or on any other Linux box):

```sh
pip install --user bleak
python3 tools/verify_beacon.py --scan
```

```
14:02:11 E2:2A:8F:50:F5:A3 rssi= -48 dBm VALID   tag OK (v1, counter 1834)
14:02:11 E2:2A:8F:50:F5:A3 rssi= -47 dBm VALID   tag OK (v1, counter 1835)
```

Now attack yourself — the fun part:

- **Spoof test:** in nRF Connect's *Advertiser* tab, create an advertisement
  named `FindMyKeys` with manufacturer data `FFFF` + 9 junk bytes. The
  scanner flags it `INVALID BAD TAG (spoof?)` while the real board keeps
  validating. The name is trivially forgeable; the tag is not.
- **Replay test:** replay any previously captured (counter, tag) pair the
  same way — `INVALID REPLAY: counter ... <= last seen`.
- **Reset caveat:** power-cycle the DK and its counter restarts at 1, so
  the verifier (if left running) calls the genuine board a replayer until
  the counter catches up. That's the documented limitation in doc 04 — and
  your invitation to persist the counter in UICR.

## Rung 4: CI (no human)

Push to GitHub: the `ci` workflow builds warning-clean firmware, uploads
hex/elf/map with a SHA-256 manifest, runs rungs 0's tests plus a must-fail
forged-tag case, and lints with cppcheck. Green pipeline = every rung a
machine can climb has been climbed; only 1–3 need hands.

## Instrumentation you already have

- `make rtt` — attach to a running beacon without reflashing.
- `make size` — flash/RAM budget after changes.
- `build/firmware.map` — where every byte went.
- Counters in the log: `adv #N sent` every 64 events ≈ every 6.4 s at
  100 ms interval — a quick sanity check that timing is right.

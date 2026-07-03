# 04 — Security

*Threat model first, mechanism second. (You do this at work already; a beacon
just makes the trade-offs unusually crisp.)*

Relevant sources: [`src/auth.c`](../src/auth.c),
[`src/siphash.c`](../src/siphash.c), [`src/config.h`](../src/config.h),
[`tools/verify_beacon.py`](../tools/verify_beacon.py).

## Threat model

A BLE advertisement is a broadcast in cleartext on a public band. Assume an
attacker with a $20 dongle who can:

| Capability | Attack | Our answer |
| --- | --- | --- |
| **Listen** | learn the beacon's presence, name, payload | accepted — it's a beacon; broadcasting is the product |
| **Record & replay** | rebroadcast old frames → "your keys are here" lies | rolling counter; receiver enforces monotonicity |
| **Forge** | craft frames with our MAC & name → spoofed beacon | SipHash tag; forging needs the key |
| **Track** | follow the static MAC around town | *not defended* — discussed honestly below |
| **Jam / flood** | drown the channel | not defendable at this layer at all |

The interesting engineering isn't the crypto — it's deciding, in writing,
which rows you defend.

## The mechanism

Every advertising event:

```
counter += 1
tag      = SipHash-2-4( key128, MAC[6] ‖ version[1] ‖ counter[4 LE] )
frame    = ... ‖ counter ‖ tag[0:4]      # low 4 bytes of the 8-byte output
```

A receiver holding the key recomputes the tag:

- **wrong tag** → forgery (didn't know the key) → reject.
- **valid tag, counter ≤ last seen** → replay of a recorded frame → reject.
- **valid tag, counter advanced** → genuine → update last-seen counter.

`tools/verify_beacon.py` implements exactly this receiver, including the
per-device counter memory.

### Why the MAC is inside the hash

Binding the tag to the MAC means a frame recorded from beacon A cannot be
replayed under beacon B's address, and one leaked (frame, tag) pair says
nothing about any other identity. Cheap, and it closes a real splice attack.

## Why SipHash?

The natural reflex is "use AES" — the nRF52840 even has an AES peripheral.
We chose SipHash-2-4 deliberately:

1. **It's a MAC by design.** SipHash is a keyed PRF built specifically for
   short inputs (our message is 11 bytes). Doing this with AES correctly
   means CMAC — a mode with subkey derivation you can get wrong. SipHash is
   one function, key in, tag out.
2. **~100 lines of portable C, zero tables.** Pure add-rotate-xor. That
   makes it *constant-time by construction*: no data-dependent branches or
   memory lookups, so no timing side channel — relevant for a device an
   attacker can hold. (Table-based AES software implementations famously
   leak through cache timing; we sidestep the whole topic.)
3. **Same source runs on host and target.** The exact file the firmware
   compiles is tested in CI against the published vectors from the SipHash
   paper (`make test`), and independently re-implemented in Python in the
   verifier. Three agreeing implementations across two languages is the
   cheapest strong evidence of correctness you can buy.
4. **It's honest about what it is.** Using the chip's AES-ECB block would
   have taught ECB habits; ECB-as-MAC is a classic vulnerability. SipHash
   used exactly as designed is a better lesson.

You already trust SipHash daily: it guards the hash tables of the Linux
kernel, Python, and Rust against collision-flooding DoS.

### The truncation trade-off

We send 32 of the 64 output bits (AdvData space is precious — doc 03). A
blind forgery therefore succeeds with probability 2⁻³² per attempt. Over
the air that's not a real attack: at BLE rates, even a flat-out flooder
gets ~1000 tries/second → expected ~50 days per hit, for a beacon whose
job is presence, not payments. The counter check also means a lucky forgery
works at most once. Know your budget: 4 bytes here buys spoof-resistance,
not cryptographic non-repudiation.

## Key management (the part that actually decides everything)

The repo ships the SipHash paper's test key in `config.h`, loudly labeled.
That's correct for a learning repo — reproducible, obviously non-secret —
and would be the *worst* line in the codebase if this shipped. For real:

- **Per-device keys**, provisioned at manufacture, e.g. written to **UICR**
  (user config flash at `0x10001000`, survives reflashes) rather than
  compiled into the image — so one leaked firmware binary ≠ all keys, and
  one leaked device key ≠ the fleet. The receiver looks keys up by MAC.
- **Enable APPROTECT** in production: the nRF52840's access-port protection
  fuses off SWD debugger access, so the key can't just be read out over the
  debug port (`probe-rs erase --allow-erase-all` recovers the chip but
  wipes flash — the key dies with the lock, which is the point). Know its
  limits: on some nRF52 revisions APPROTECT bypasses via voltage glitching
  are published. It raises the attacker's cost; it is not an HSM.
- The same discipline you apply to CI secrets applies here: the key never
  appears in logs (note the firmware RTT-logs the MAC and counter, never
  the key), never in git, and rotation is a provisioning workflow, not a
  code change.

## What we deliberately do not defend: tracking

Our beacon uses one static MAC forever — that's the *product*: "let me
recognize my keys." The flip side is anyone can recognize your keys, i.e.
track them. This is exactly why your iPhone rotates a random *resolvable
private address* every ~15 minutes: paired devices can resolve it using a
shared IRK (Identity Resolving Key); strangers see uncorrelatable noise.
Apple's Find My network goes further with rotating public keys per slot.

Building rotating identities on top of this codebase is a great exercise:
derive `addr_i = f(key, i)` with SipHash, rotate every N minutes, and have
the verifier check candidates within a window. The pieces are all here.

## Boring-but-real: the supply chain

The CI pipeline applies standard controls to firmware, where they're rarer
than they should be: read-only workflow token, pinned actions, `-Werror`
builds, static analysis, crypto vectors on every push, and SHA-256 sums
published with every artifact so the image on a board can be traced to a
commit. None of it is novel — that's the point; firmware doesn't get an
exemption from the basics.

---

**Next:** [05 — Testing](05-testing.md) — proving all of the above with a
phone and a Python script.

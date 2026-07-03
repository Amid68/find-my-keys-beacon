#!/usr/bin/env python3
"""
verify_beacon.py - Receiver-side verifier for the Find My Keys beacon.

This is the other half of the security story. The firmware signs every
advertisement with a truncated SipHash-2-4 tag; this script plays the role of
a trusted receiver that holds the same key and can therefore tell a genuine
beacon from a spoof or a replay.

Two modes:

  1. Offline: verify a manufacturer-data payload you copied out of a scanner
     app (nRF Connect on the iPhone shows it as hex under "Manufacturer data"):

         ./verify_beacon.py --mac e2:2a:8f:50:f5:a3 --data 01e80300009c41d2aa

     (--data is the payload AFTER the 2-byte company id 0xFFFF, i.e.
      version(1) + counter(4 LE) + tag(4); nRF Connect strips the company id
      for you. If you pasted all 11 bytes starting with ff ff, that works too.)

  2. Live scan (Linux, needs `pip install bleak` and a BLE adapter):

         ./verify_beacon.py --scan

     Prints every FindMyKeys frame it hears with RSSI, verifies the tag, and
     tracks the counter to flag replays.

The SipHash implementation below is deliberately independent from the C one
(pure Python, written from the spec) - two implementations agreeing across
languages is decent evidence both are right. It self-checks against the same
published vectors as tools/test_siphash.c on every run.
"""

import argparse
import sys
import time

# --- Demo key: MUST match BEACON_SIPHASH_K0/K1 in src/config.h --------------
KEY_K0 = 0x0706050403020100
KEY_K1 = 0x0F0E0D0C0B0A0908

COMPANY_ID = 0xFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF


# --- SipHash-2-4, from the specification -------------------------------------
def _rotl(x: int, b: int) -> int:
    return ((x << b) | (x >> (64 - b))) & MASK64


def _sipround(v0: int, v1: int, v2: int, v3: int):
    v0 = (v0 + v1) & MASK64
    v1 = _rotl(v1, 13) ^ v0
    v0 = _rotl(v0, 32)
    v2 = (v2 + v3) & MASK64
    v3 = _rotl(v3, 16) ^ v2
    v0 = (v0 + v3) & MASK64
    v3 = _rotl(v3, 21) ^ v0
    v2 = (v2 + v1) & MASK64
    v1 = _rotl(v1, 17) ^ v2
    v2 = _rotl(v2, 32)
    return v0, v1, v2, v3


def siphash24(key_k0: int, key_k1: int, data: bytes) -> int:
    v0 = 0x736F6D6570736575 ^ key_k0
    v1 = 0x646F72616E646F6D ^ key_k1
    v2 = 0x6C7967656E657261 ^ key_k0
    v3 = 0x7465646279746573 ^ key_k1

    # Whole 8-byte blocks.
    n_full = len(data) // 8
    for i in range(n_full):
        m = int.from_bytes(data[i * 8 : i * 8 + 8], "little")
        v3 ^= m
        for _ in range(2):
            v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
        v0 ^= m

    # Final block: leftover bytes + total length in the top byte.
    tail = data[n_full * 8 :]
    b = (len(data) & 0xFF) << 56
    b |= int.from_bytes(tail.ljust(8, b"\x00")[:7] + b"\x00", "little")
    v3 ^= b
    for _ in range(2):
        v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)
    v0 ^= b

    v2 ^= 0xFF
    for _ in range(4):
        v0, v1, v2, v3 = _sipround(v0, v1, v2, v3)

    return (v0 ^ v1 ^ v2 ^ v3) & MASK64


def self_test() -> None:
    """Same published vectors as tools/test_siphash.c. Abort if any fail."""
    vectors = {
        0: 0x726FDB47DD0E0E31,
        1: 0x74F839C593DC67FD,
        8: 0x93F5F5799A932462,
        15: 0xA129CA6149BE45E5,
    }
    msg = bytes(range(16))
    for length, expected in vectors.items():
        got = siphash24(KEY_K0, KEY_K1, msg[:length])
        if got != expected:
            sys.exit(f"SipHash self-test FAILED for len={length}: "
                     f"got {got:#018x}, want {expected:#018x}")


# --- Beacon frame verification ------------------------------------------------
def compute_tag(mac_le: bytes, version: int, counter: int) -> bytes:
    """tag = SipHash24(key, MAC(6, on-air order) || version || counter LE)[0:4]"""
    msg = mac_le + bytes([version]) + counter.to_bytes(4, "little")
    return siphash24(KEY_K0, KEY_K1, msg).to_bytes(8, "little")[:4]


def parse_mac(text: str) -> bytes:
    """'e2:2a:8f:50:f5:a3' (display order, MSB first) -> on-air LE bytes."""
    parts = text.replace("-", ":").split(":")
    if len(parts) != 6:
        sys.exit(f"bad MAC: {text}")
    return bytes(int(p, 16) for p in reversed(parts))


def verify_payload(mac_le: bytes, payload: bytes, last_counter: int | None):
    """
    payload = version(1) + counter(4 LE) + tag(4).  Returns (ok, counter, note).
    """
    if len(payload) != 9:
        return False, None, f"expected 9 payload bytes, got {len(payload)}"
    version = payload[0]
    counter = int.from_bytes(payload[1:5], "little")
    tag = payload[5:9]
    expected = compute_tag(mac_le, version, counter)
    if tag != expected:
        return False, counter, (f"BAD TAG: got {tag.hex()}, "
                                f"expected {expected.hex()} (spoof?)")
    if last_counter is not None and counter <= last_counter:
        return False, counter, (f"REPLAY: counter {counter} <= "
                                f"last seen {last_counter}")
    return True, counter, f"tag OK (v{version}, counter {counter})"


# --- Offline mode --------------------------------------------------------------
def run_offline(mac_text: str, data_hex: str) -> None:
    mac_le = parse_mac(mac_text)
    data = bytes.fromhex(data_hex.replace(" ", "").replace("0x", ""))
    # Accept payload with or without the leading ff ff company id.
    if len(data) == 11 and data[0] == 0xFF and data[1] == 0xFF:
        data = data[2:]
    ok, _, note = verify_payload(mac_le, data, last_counter=None)
    print(("VALID   " if ok else "INVALID ") + note)
    sys.exit(0 if ok else 1)


# --- Live scan mode -------------------------------------------------------------
def run_scan(name_filter: str) -> None:
    try:
        import asyncio
        from bleak import BleakScanner
    except ImportError:
        sys.exit("live scan needs bleak:  pip install bleak")

    last_counter: dict[str, int] = {}

    def on_adv(device, adv):
        if adv.local_name != name_filter:
            return
        payload = adv.manufacturer_data.get(COMPANY_ID)
        if payload is None:
            return
        mac_le = parse_mac(device.address)
        ok, counter, note = verify_payload(
            mac_le, bytes(payload), last_counter.get(device.address))
        if counter is not None:
            last_counter[device.address] = max(
                counter, last_counter.get(device.address, 0))
        stamp = time.strftime("%H:%M:%S")
        status = "VALID  " if ok else "INVALID"
        print(f"{stamp} {device.address} rssi={adv.rssi:4d} dBm "
              f"{status} {note}")

    async def main():
        print(f"scanning for '{name_filter}' ... Ctrl-C to stop")
        scanner = BleakScanner(on_adv)
        async with scanner:
            await asyncio.Event().wait()  # run until interrupted

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nbye")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scan", action="store_true",
                    help="scan live over BLE (Linux, requires bleak)")
    ap.add_argument("--name", default="FindMyKeys",
                    help="advertised name to filter on (default: FindMyKeys)")
    ap.add_argument("--mac", help="beacon MAC as shown by a scanner, e.g. "
                                  "e2:2a:8f:50:f5:a3 (offline mode)")
    ap.add_argument("--data", help="manufacturer data hex (offline mode)")
    args = ap.parse_args()

    self_test()

    if args.scan:
        run_scan(args.name)
    elif args.mac and args.data:
        run_offline(args.mac, args.data)
    else:
        ap.print_help()
        sys.exit(2)

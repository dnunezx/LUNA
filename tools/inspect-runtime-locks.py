"""Read-only inspection of stripped PS2 ELFs for the observed lock race.

Usage: python tools/inspect-runtime-locks.py path/to/luna_unc.elf [...]
This identifies instruction ordering, not the cause of a particular freeze.
"""
import json
from pathlib import Path
import struct
import sys


def inspect(path):
    data = Path(path).read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected a little-endian ELF32 executable")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from("<IIIIIIIIII", data,
                header[6] + i * header[11]) for i in range(header[12])]
    words = {
        section[3] + offset: struct.unpack_from("<I", data,
                                             section[4] + offset)[0]
        for section in sections if section[2] & 4 and section[1] != 8
        for offset in range(0, section[5] - 3, 4)
    }
    # PS2 WaitSema wrapper: syscall with v1 = 0x44.
    wait_addresses = set()
    for address, word in words.items():
        if word == 12:
            for previous in range(address - 16, address, 4):
                if words.get(previous) in (0x24030044, 0x34030044):
                    wait_addresses.add(previous)
    matches = []
    for address, word in words.items():
        # Observed compiled layout: owner at +4, recursion count at +8.
        # sw v0,8(s0), preceded by sw v0,4(s0); then j/jal WaitSema.
        if word != 0xAE020008 or words.get(address - 8) != 0xAE020004:
            continue
        for following in range(address + 4, address + 32, 4):
            branch = words.get(following, 0)
            target = ((branch & 0x3FFFFFF) << 2) | ((following + 4) & 0xF0000000)
            if branch >> 26 in (2, 3) and target in wait_addresses:
                matches.append({"count_store": hex(address),
                                "wait_branch": hex(following),
                                "wait_sema": hex(target)})
                break
    return {"file": str(path), "observed_vulnerable_sequence": matches}


def demonstrate_interleaving():
    # A publishes count before acquiring the gate, then is preempted.
    count, gate = 1, 1
    # B sees contention, but takes the still-available gate and adds to count.
    gate, count = 0, count + 1
    # B releases once. A phantom recursion remains, so no signal is issued.
    count -= 1
    if count == 0:
        gate = 1
    assert (count, gate) == (1, 0)
    # A's WaitSema now blocks; no thread owns the gate to release it.
    return {"old_lock_after_B_release": {"count": count, "gate": gate},
            "A_blocks_without_a_gate_owner": True}


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    print(json.dumps({"executables": [inspect(path) for path in sys.argv[1:]],
                      "interleaving_model": demonstrate_interleaving()}, indent=2))

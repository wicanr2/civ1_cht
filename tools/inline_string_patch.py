#!/usr/bin/env python3
"""
Phase 3: in-place patch CIV.EXE inline ASCII strings with Big5.

Approach: for each translation entry, find ALL byte-exact occurrences
in the EXE (must be null-terminated), encode Big5 to cp950, write at
the offset, then NULL-pad to original slot length.

Unlike Phase 1 (RT_DIALOG), inline strings have no walker constraint —
adjacent fields are not parsed by offset+null+1, they're separate
strings or unrelated data. So NULL padding is safe and clean.
"""
import sys, json, struct, os

def find_all_offsets(data, needle_bytes):
    """Find all start offsets where needle appears AND is followed by null."""
    offsets = []
    n = len(needle_bytes)
    i = 0
    while True:
        i = data.find(needle_bytes, i)
        if i < 0: break
        # require null terminator immediately after
        if i + n < len(data) and data[i + n] == 0:
            offsets.append(i)
        i += 1
    return offsets


def main():
    if len(sys.argv) < 4:
        print("usage: inline_string_patch.py CIV.EXE inline_translations.json CIV.EXE.out [--apply]")
        sys.exit(1)
    exe_in, tr_json, exe_out = sys.argv[1:4]
    apply_mode = '--apply' in sys.argv

    with open(exe_in, 'rb') as f:
        data = bytearray(f.read())
    with open(tr_json, encoding='utf-8') as f:
        tr_raw = json.load(f)

    # Strip metadata keys. Convention: metadata keys are lowercase-only
    # (_comment, _batch_a, _policy) — translation keys can start with _ but
    # contain at least one uppercase OR space char (e.g. "_Start a New Game_...").
    def is_metadata_key(k):
        if not k.startswith('_'):
            return False
        # Real translation keys have uppercase or punctuation/space beyond the underscores
        body = k.lstrip('_')
        return body == '' or body.replace('_', '').islower()
    translations = {k: v for k, v in tr_raw.items() if not is_metadata_key(k)}

    print(f"Loaded {len(translations)} translation entries")
    print(f"EXE size: {len(data)} bytes")
    print()

    patches = []     # (offset, slot_len, eng_bytes, big5_bytes_padded, eng_str, big5_str)
    skipped = []
    missing = []

    for eng, big5 in translations.items():
        eng_bytes = eng.encode('ascii', errors='replace')
        try:
            big5_bytes = big5.encode('cp950')
        except UnicodeEncodeError as e:
            skipped.append((eng, big5, f'cp950 encode fail: {e}'))
            continue

        slot = len(eng_bytes)
        if len(big5_bytes) > slot:
            skipped.append((eng, big5, f'big5 {len(big5_bytes)}B > slot {slot}B'))
            continue

        offsets = find_all_offsets(data, eng_bytes)
        if not offsets:
            missing.append(eng)
            continue

        pad_len = slot - len(big5_bytes)
        new_bytes = big5_bytes + (b'\x00' * pad_len)
        assert len(new_bytes) == slot

        for off in offsets:
            patches.append((off, slot, eng_bytes, new_bytes, eng, big5))

    # Report
    print(f"=== Patches ready: {len(patches)} (across {len(translations) - len(skipped) - len(missing)} unique strings) ===")
    print(f"Skipped (overlong or encoding fail): {len(skipped)}")
    print(f"Missing (string not found in EXE):    {len(missing)}")
    print()

    if skipped:
        print("SKIPPED:")
        for eng, big5, reason in skipped:
            print(f"  - {eng!r} -> {big5!r}: {reason}")
        print()
    if missing:
        print("MISSING:")
        for eng in missing:
            print(f"  - {eng!r}")
        print()

    print("=== Patch preview (sample) ===")
    for off, slot, eng_b, new_b, eng, big5 in patches[:15]:
        print(f"  @0x{off:x} slot={slot:3d}B: {eng!r}")
        print(f"           -> {big5!r}   bytes={new_b.hex(' ')[:50]}...")

    if not apply_mode:
        print()
        print("(dry-run; pass --apply to write output)")
        return

    # Apply (sort by offset for deterministic output)
    patches.sort(key=lambda p: p[0])
    for off, slot, eng_b, new_b, _, _ in patches:
        data[off:off+slot] = new_b

    with open(exe_out, 'wb') as f:
        f.write(bytes(data))

    in_sz = os.path.getsize(exe_in)
    out_sz = os.path.getsize(exe_out)
    assert in_sz == out_sz, f"size drift: {in_sz} != {out_sz}"
    print()
    print(f"Wrote {exe_out} ({out_sz} bytes, identical size to input)")
    print(f"Total bytes modified: {sum(p[1] for p in patches)}")


if __name__ == '__main__':
    main()

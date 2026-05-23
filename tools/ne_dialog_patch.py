#!/usr/bin/env python3
"""
Phase 1: in-place patch CIV.EXE RT_DIALOG translatable strings to Big5.

Strategy: pad Big5 with ASCII spaces (0x20) to EXACTLY the original slot length,
so the Win16 dialog walker (null-terminated) advances the same number of bytes
and the subsequent createInfoSize byte is read from the correct position.

Inputs:
  - CIV.EXE (input) — decompressed Win16 NE
  - civ_dialogs.json — catalogue from ne_dialog_extract.py
  - dialog_translations.json — english -> big5 utf8 string map

Output:
  - CIV.EXE.patched — same byte length, with patched dialog strings
"""
import sys, json, struct, os, shutil

def main():
    if len(sys.argv) < 5:
        print("usage: ne_dialog_patch.py CIV.EXE civ_dialogs.json dialog_translations.json CIV.EXE.out")
        sys.exit(1)
    exe_in, dlg_json, tr_json, exe_out = sys.argv[1:5]

    with open(exe_in, 'rb') as f:
        data = bytearray(f.read())
    with open(dlg_json, encoding='utf-8') as f:
        dialogs = json.load(f)
    with open(tr_json, encoding='utf-8') as f:
        tr_raw = json.load(f)

    # strip comment keys
    translations = {k: v for k, v in tr_raw.items() if not k.startswith('_')}

    print(f"Loaded {len(dialogs)} dialog records, {len(translations)} translation entries.")
    print()

    # Walk all dialog strings, apply translations
    patches = []  # (file_offset, old_bytes, new_bytes, english)
    skipped = []
    missing = []

    for d in dialogs:
        rid = d.get('resource_id')
        for s in d.get('strings', []):
            role = s['role']
            if role in ('class', 'menu'):
                continue
            eng = s['text']
            slot = s['byte_len']
            off = s['file_offset']
            if eng not in translations:
                missing.append((rid, role, eng, slot))
                continue
            big5_utf = translations[eng]
            # encode to cp950 (big5)
            try:
                big5_bytes = big5_utf.encode('cp950')
            except UnicodeEncodeError as e:
                print(f"  !! cp950 encode FAIL for {big5_utf!r}: {e}")
                skipped.append((rid, role, eng, big5_utf, 'encode_fail'))
                continue
            if len(big5_bytes) > slot:
                print(f"  !! OVERLONG dlg#{rid} {role}: en={eng!r} big5={big5_utf!r} = {len(big5_bytes)}B > slot {slot}B")
                skipped.append((rid, role, eng, big5_utf, f'overlong {len(big5_bytes)}>{slot}'))
                continue
            # pad with ASCII spaces to exactly slot bytes
            pad_len = slot - len(big5_bytes)
            new_bytes = big5_bytes + (b'\x20' * pad_len)
            assert len(new_bytes) == slot, f"pad math broken: {len(new_bytes)} != {slot}"

            # sanity: verify file currently has the english there
            current = bytes(data[off:off+slot])
            if current != eng.encode('ascii', errors='replace'):
                print(f"  !! MISMATCH dlg#{rid} {role} @0x{off:x}: expected {eng!r} got {current!r}; skipping")
                skipped.append((rid, role, eng, big5_utf, 'file_mismatch'))
                continue

            patches.append((off, current, new_bytes, eng, big5_utf, rid, role))

    # Report
    print(f"Pending patches: {len(patches)}")
    print(f"Skipped: {len(skipped)}")
    print(f"Missing translations: {len(missing)}")
    print()
    if missing:
        print("Missing translations (add to dialog_translations.json):")
        for rid, role, eng, slot in missing:
            print(f"  dlg#{rid} {role}: ({slot}B) {eng!r}")
        print()

    # Show preview
    print("Patches preview (first 20):")
    for p in patches[:20]:
        off, _, new, eng, big5, rid, role = p
        print(f"  dlg#{rid} {role} @0x{off:x}: {eng!r}  ->  {big5!r}  (padded to {len(new)}B: {new!r})")
    print()

    # Apply patches
    for off, _, new, _, _, _, _ in patches:
        data[off:off+len(new)] = new

    # Write output
    with open(exe_out, 'wb') as f:
        f.write(bytes(data))
    print(f"Wrote {exe_out} ({len(data)} bytes, same as input)")

    # Verify sizes match
    in_size = os.path.getsize(exe_in)
    out_size = os.path.getsize(exe_out)
    assert in_size == out_size, f"size mismatch: in={in_size} out={out_size}"
    print(f"OK: input/output size identical ({in_size} bytes)")

if __name__ == '__main__':
    main()

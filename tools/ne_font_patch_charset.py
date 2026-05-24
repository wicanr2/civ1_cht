#!/usr/bin/env python3
"""
Phase 2: patch CIVFONTS.FON RT_FONT dfCharSet field.
For each font in --faces list (default: CIVTIMES family), set dfCharSet
from 0x00 (ANSI_CHARSET) to 0x88 (CHINESEBIG5_CHARSET).

This signals to Win16 GDI that the font is Big5 DBCS-aware. Combined with
ACP=950 and FontSubstitutes registry, this causes GDI to walk Big5
lead/trail byte pairs when drawing text through these fonts.
"""
import sys, struct, json, os

NE_RESOURCE_TYPES = {0x8008: 'RT_FONT'}

DEFAULT_FACES_TO_PATCH = {
    'CIVTIMES10', 'CIVTIMES12', 'CIVTIMES14', 'CIVTIMES18',
    'CIVTIMES24', 'CIVTIMES30', 'CIVTIMES36'
}

NEW_CHARSET = 0x88  # CHINESEBIG5_CHARSET


def main():
    if len(sys.argv) < 3:
        print("usage: ne_font_patch_charset.py CIVFONTS.FON CIVFONTS.FON.cht [face1,face2,...]")
        sys.exit(1)
    src, dst = sys.argv[1], sys.argv[2]
    if len(sys.argv) >= 4:
        faces = set(sys.argv[3].split(','))
    else:
        faces = DEFAULT_FACES_TO_PATCH

    with open(src, 'rb') as f:
        data = bytearray(f.read())

    e_lfanew = struct.unpack('<I', data[0x3C:0x40])[0]
    ne_off = e_lfanew
    assert data[ne_off:ne_off+2] == b'NE', 'not NE'

    res_off_rel = struct.unpack('<H', data[ne_off+0x24:ne_off+0x26])[0]
    res_abs = ne_off + res_off_rel
    shift = struct.unpack('<H', data[res_abs:res_abs+2])[0]
    unit = 1 << shift

    idx = res_abs + 2
    patches_applied = 0
    while idx < len(data):
        type_id = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        if type_id == 0: break
        count = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        idx += 4
        for r in range(count):
            r_off_u = struct.unpack('<H', data[idx:idx+2])[0]
            r_len_u = struct.unpack('<H', data[idx+2:idx+4])[0]
            idx += 12
            if type_id != 0x8008:
                continue
            font_off = r_off_u * unit
            # Parse face name (dfFace at +0x69, offset relative to font resource start)
            df_face_rel = struct.unpack('<I', data[font_off+0x69:font_off+0x6D])[0]
            face_off = font_off + df_face_rel
            end = data.index(0, face_off) if 0 in data[face_off:face_off+64] else face_off + 32
            face = bytes(data[face_off:end]).decode('ascii', errors='replace')
            cs_off = font_off + 0x55
            old_cs = data[cs_off]
            if face in faces:
                if old_cs == NEW_CHARSET:
                    print(f"  SKIP {face}: already 0x{NEW_CHARSET:02x}")
                    continue
                data[cs_off] = NEW_CHARSET
                print(f"  PATCH {face}: dfCharSet 0x{old_cs:02x} -> 0x{NEW_CHARSET:02x} @ file offset 0x{cs_off:x}")
                patches_applied += 1
            else:
                print(f"  skip {face}: not in target set (current dfCharSet=0x{old_cs:02x})")

    with open(dst, 'wb') as f:
        f.write(bytes(data))

    in_size = os.path.getsize(src)
    out_size = os.path.getsize(dst)
    assert in_size == out_size
    print()
    print(f"Done: patched {patches_applied} fonts. Wrote {dst} ({out_size} bytes; identical size).")


if __name__ == '__main__':
    main()

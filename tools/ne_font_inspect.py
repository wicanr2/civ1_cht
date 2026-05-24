#!/usr/bin/env python3
"""
Inspect Win16 NE .FON file (font library) to enumerate RT_FONT resources
and dump key Win16 FONTINFO fields, especially dfCharSet and dfFace.

Win16 FONTINFO (v2.0, the typical bitmap font format):
  Offset Size  Field
  0x00   2     dfVersion (0x0200 for v2, 0x0300 for v3)
  0x02   4     dfSize (total font size)
  0x06   60    dfCopyright (zero-terminated)
  0x42   2     dfType (bitmap=0, vector=1)
  0x44   2     dfPoints
  0x46   2     dfVertRes
  0x48   2     dfHorizRes
  0x4A   2     dfAscent
  0x4C   2     dfInternalLeading
  0x4E   2     dfExternalLeading
  0x50   1     dfItalic
  0x51   1     dfUnderline
  0x52   1     dfStrikeOut
  0x53   2     dfWeight
  0x55   1     dfCharSet  <-- KEY FIELD FOR DBCS
  0x56   2     dfPixWidth
  0x58   2     dfPixHeight
  0x5A   1     dfPitchAndFamily
  0x5B   2     dfAvgWidth
  0x5D   2     dfMaxWidth
  0x5F   1     dfFirstChar
  0x60   1     dfLastChar
  0x61   1     dfDefaultChar
  0x62   1     dfBreakChar
  0x63   2     dfWidthBytes
  0x65   4     dfDevice (offset to device name)
  0x69   4     dfFace   <-- offset (from file start) to face name string
  0x6D   4     dfBitsPointer (always 0 in file)
  0x71   4     dfBitsOffset (offset to glyph bitmap data)
  0x75   1     dfReserved
  v3 only:
  0x76   4     dfFlags
  0x7A   2     dfAspace
  0x7C   2     dfBspace
  0x7E   2     dfCspace
  0x80   4     dfColorPointer
  0x84   16    dfReserved1[4]
"""
import sys, struct

CHARSET_NAMES = {
    0x00: 'ANSI_CHARSET',
    0x01: 'DEFAULT_CHARSET',
    0x02: 'SYMBOL_CHARSET',
    0x4D: 'MAC_CHARSET',
    0x80: 'SHIFTJIS_CHARSET',  # Japanese
    0x81: 'HANGEUL_CHARSET',
    0x82: 'JOHAB_CHARSET',
    0x86: 'GB2312_CHARSET',    # Simplified Chinese
    0x88: 'CHINESEBIG5_CHARSET',  # Traditional Chinese
    0xA1: 'GREEK_CHARSET',
    0xA2: 'TURKISH_CHARSET',
    0xA3: 'VIETNAMESE_CHARSET',
    0xB1: 'HEBREW_CHARSET',
    0xB2: 'ARABIC_CHARSET',
    0xBA: 'BALTIC_CHARSET',
    0xCC: 'RUSSIAN_CHARSET',
    0xDE: 'THAI_CHARSET',
    0xEE: 'EASTEUROPE_CHARSET',
    0xFF: 'OEM_CHARSET',
}


def parse_fontinfo(data, base_off):
    """Parse a Win16 FONTINFO header starting at base_off (file-absolute)."""
    if base_off + 0x76 > len(data):
        return None
    f = {}
    f['file_offset'] = base_off
    f['dfVersion'] = struct.unpack('<H', data[base_off:base_off+2])[0]
    f['dfSize'] = struct.unpack('<I', data[base_off+0x02:base_off+0x06])[0]
    # Copyright string at 0x06 (60 bytes, zero-terminated)
    copy_bytes = data[base_off+0x06:base_off+0x06+60]
    f['dfCopyright'] = copy_bytes.split(b'\0')[0].decode('ascii', errors='replace')
    f['dfType'] = struct.unpack('<H', data[base_off+0x42:base_off+0x44])[0]
    f['dfPoints'] = struct.unpack('<H', data[base_off+0x44:base_off+0x46])[0]
    f['dfAscent'] = struct.unpack('<H', data[base_off+0x4A:base_off+0x4C])[0]
    f['dfWeight'] = struct.unpack('<H', data[base_off+0x53:base_off+0x55])[0]
    f['dfCharSet_raw'] = data[base_off+0x55]
    f['dfCharSet_name'] = CHARSET_NAMES.get(f['dfCharSet_raw'], f"unknown_0x{f['dfCharSet_raw']:02x}")
    f['dfPixWidth'] = struct.unpack('<H', data[base_off+0x56:base_off+0x58])[0]
    f['dfPixHeight'] = struct.unpack('<H', data[base_off+0x58:base_off+0x5A])[0]
    f['dfPitchAndFamily'] = data[base_off+0x5A]
    f['dfAvgWidth'] = struct.unpack('<H', data[base_off+0x5B:base_off+0x5D])[0]
    f['dfMaxWidth'] = struct.unpack('<H', data[base_off+0x5D:base_off+0x5F])[0]
    f['dfFirstChar'] = data[base_off+0x5F]
    f['dfLastChar'] = data[base_off+0x60]
    f['dfDefaultChar'] = data[base_off+0x61]
    f['dfBreakChar'] = data[base_off+0x62]
    f['dfWidthBytes'] = struct.unpack('<H', data[base_off+0x63:base_off+0x65])[0]
    f['dfDevice_off'] = struct.unpack('<I', data[base_off+0x65:base_off+0x69])[0]
    f['dfFace_off'] = struct.unpack('<I', data[base_off+0x69:base_off+0x6D])[0]
    f['dfBitsOffset'] = struct.unpack('<I', data[base_off+0x71:base_off+0x75])[0]

    # dfFace is offset RELATIVE TO THE START OF THE FONT RESOURCE (base_off)
    face_off = base_off + f['dfFace_off']
    if 0 < f['dfFace_off'] < f['dfSize'] and face_off < len(data):
        end = data.index(0, face_off) if 0 in data[face_off:face_off+64] else face_off + 32
        f['dfFace_text'] = data[face_off:end].decode('ascii', errors='replace')
        f['dfFace_abs_offset'] = face_off
    else:
        f['dfFace_text'] = '<invalid>'
        f['dfFace_abs_offset'] = None

    # dfCharSet patch offset (for Phase 2)
    f['dfCharSet_file_offset'] = base_off + 0x55

    return f


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/extracted/CIVFONTS.FON'
    with open(path, 'rb') as f:
        data = f.read()

    if data[:2] != b'MZ':
        print("Not MZ"); return
    e_lfanew = struct.unpack('<I', data[0x3C:0x40])[0]
    if data[e_lfanew:e_lfanew+2] != b'NE':
        print("Not NE"); return
    ne_off = e_lfanew

    res_off_rel = struct.unpack('<H', data[ne_off+0x24:ne_off+0x26])[0]
    res_abs = ne_off + res_off_rel
    shift = struct.unpack('<H', data[res_abs:res_abs+2])[0]
    unit = 1 << shift

    idx = res_abs + 2
    fonts = []
    fontdirs = []
    while idx < len(data):
        type_id = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        if type_id == 0: break
        count = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        idx += 4
        for r in range(count):
            r_off_u = struct.unpack('<H', data[idx:idx+2])[0]
            r_len_u = struct.unpack('<H', data[idx+2:idx+4])[0]
            r_flags = struct.unpack('<H', data[idx+4:idx+6])[0]
            r_id = struct.unpack('<H', data[idx+6:idx+8])[0]
            idx += 12
            abs_off = r_off_u * unit
            abs_len = r_len_u * unit
            if type_id == 0x8008:  # RT_FONT
                fonts.append((r_id, abs_off, abs_len))
            elif type_id == 0x8007:  # RT_FONTDIR
                fontdirs.append((r_id, abs_off, abs_len))

    print(f"File: {path}")
    print(f"RT_FONTDIR count: {len(fontdirs)}")
    print(f"RT_FONT count: {len(fonts)}")
    print()

    for fid, off, fsz in fonts:
        info = parse_fontinfo(data, off)
        if info is None:
            print(f"  RT_FONT #{fid} @0x{off:x}: parse failed"); continue
        print(f"=== RT_FONT id={fid & 0x7FFF} @0x{off:x} ({fsz} bytes alloc) ===")
        print(f"  dfVersion   = 0x{info['dfVersion']:04x}")
        print(f"  dfSize      = {info['dfSize']}")
        print(f"  dfCopyright = {info['dfCopyright']!r}")
        print(f"  dfType      = {info['dfType']} ({'bitmap' if info['dfType']==0 else 'vector/other'})")
        print(f"  dfPoints    = {info['dfPoints']}")
        print(f"  dfAscent    = {info['dfAscent']}")
        print(f"  dfWeight    = {info['dfWeight']} (400=normal,700=bold)")
        print(f"  dfCharSet   = 0x{info['dfCharSet_raw']:02x} ({info['dfCharSet_name']})  [PATCH OFFSET 0x{info['dfCharSet_file_offset']:x}]")
        print(f"  dfPixWidth  = {info['dfPixWidth']}")
        print(f"  dfPixHeight = {info['dfPixHeight']}")
        print(f"  dfAvgWidth  = {info['dfAvgWidth']}")
        print(f"  dfMaxWidth  = {info['dfMaxWidth']}")
        print(f"  dfFirstChar = {info['dfFirstChar']} (0x{info['dfFirstChar']:02x})")
        print(f"  dfLastChar  = {info['dfLastChar']} (0x{info['dfLastChar']:02x})")
        print(f"  dfFace text = {info['dfFace_text']!r}  @ abs offset 0x{info['dfFace_abs_offset']:x}" if info['dfFace_abs_offset'] else f"  dfFace text = <invalid>")
        print()


if __name__ == '__main__':
    main()

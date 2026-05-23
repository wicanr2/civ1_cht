#!/usr/bin/env python3
"""EDILZSS2 decompressor — save best variant output and check NE validity."""
import sys, struct

def lzss_decode(data, idx_start, n=4096):
    window = bytearray([0x20] * n)
    wpos = n - 18
    out = bytearray()
    idx = idx_start
    while idx < len(data):
        ctrl = data[idx]; idx += 1
        for bit in range(8):
            if idx >= len(data):
                return bytes(out), idx
            if ctrl & (1 << bit):
                b = data[idx]; idx += 1
                out.append(b)
                window[wpos] = b
                wpos = (wpos + 1) % n
            else:
                if idx + 1 >= len(data):
                    return bytes(out), idx
                b1 = data[idx]; b2 = data[idx+1]; idx += 2
                offset = ((b2 & 0xF0) << 4) | b1
                length = (b2 & 0x0F) + 3
                for _ in range(length):
                    b = window[offset % n]
                    out.append(b)
                    window[wpos] = b
                    wpos = (wpos + 1) % n
                    offset = (offset + 1) % n
    return bytes(out), idx


def parse_ne_header(data):
    if len(data) < 0x40:
        return "too short for MZ"
    if data[:2] != b'MZ':
        return "no MZ"
    e_lfanew = struct.unpack('<I', data[0x3C:0x40])[0]
    print(f"  e_lfanew (NE header offset) = 0x{e_lfanew:x}")
    if e_lfanew + 64 > len(data):
        return f"e_lfanew 0x{e_lfanew:x} beyond data len 0x{len(data):x}"
    ne_magic = data[e_lfanew:e_lfanew+2]
    print(f"  NE magic at 0x{e_lfanew:x}: {ne_magic} ({'OK' if ne_magic == b'NE' else 'FAIL'})")
    if ne_magic != b'NE':
        return "no NE magic"
    # NE header: at offset 0x36-0x37 is number of entries in segment table
    # At 0x22 is offset to segment table (from NE header)
    ne_off = e_lfanew
    seg_table_count = struct.unpack('<H', data[ne_off+0x1C:ne_off+0x1E])[0]
    seg_table_off = struct.unpack('<H', data[ne_off+0x22:ne_off+0x24])[0]
    res_table_off = struct.unpack('<H', data[ne_off+0x24:ne_off+0x26])[0]
    rname_table_off = struct.unpack('<H', data[ne_off+0x26:ne_off+0x28])[0]
    modref_table_off = struct.unpack('<H', data[ne_off+0x28:ne_off+0x2A])[0]
    iname_table_off = struct.unpack('<H', data[ne_off+0x2A:ne_off+0x2C])[0]
    modref_count = struct.unpack('<H', data[ne_off+0x1E:ne_off+0x20])[0]
    iname_size = struct.unpack('<H', data[ne_off+0x14:ne_off+0x16])[0]
    print(f"  segment table: off=0x{seg_table_off:x} (abs 0x{ne_off+seg_table_off:x}) count={seg_table_count}")
    print(f"  modref table: off=0x{modref_table_off:x} count={modref_count}")
    print(f"  iname table: off=0x{iname_table_off:x} size=0x{iname_size:x}")
    print(f"  rname table: off=0x{rname_table_off:x}")

    # Module reference table is array of u16 offsets into imported names table
    print(f"\n  Module references:")
    for i in range(modref_count):
        off = struct.unpack('<H', data[ne_off+modref_table_off+i*2:ne_off+modref_table_off+i*2+2])[0]
        # Each entry in iname table: 1-byte length + name bytes
        name_off = ne_off + iname_table_off + off
        if name_off >= len(data):
            print(f"    [{i}] @0x{off:x}: out of bounds")
            continue
        nlen = data[name_off]
        name = data[name_off+1:name_off+1+nlen].decode('ascii', errors='replace')
        print(f"    [{i}] {name}")

    return "NE parsed"


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/CIV.EX$'
    outpath = sys.argv[2] if len(sys.argv) > 2 else '/tmp/civ1/CIV.EXE.try1'
    with open(path, 'rb') as f:
        data = f.read()

    if data[:8] != b'EDILZSS2':
        print("Not EDILZSS2")
        return

    # Stream starts at 0x19 per hypothesis
    out, consumed_idx = lzss_decode(data, 0x19)
    print(f"Input {len(data)} bytes, consumed up to 0x{consumed_idx:x} ({consumed_idx} bytes)")
    print(f"Output {len(out)} bytes")
    print()

    with open(outpath, 'wb') as f:
        f.write(out)
    print(f"Saved to {outpath}")
    print()

    print("=== NE structure check ===")
    msg = parse_ne_header(out)
    print(f"  result: {msg}")

    print(f"\n=== First 128 bytes ===")
    for i in range(0, 128, 16):
        chunk = out[i:i+16]
        hex_str = ' '.join(f'{b:02x}' for b in chunk)
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f"  {i:08x}: {hex_str}  {ascii_str}")


if __name__ == '__main__':
    main()

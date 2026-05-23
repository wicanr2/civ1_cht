#!/usr/bin/env python3
"""
Extract all RT_DIALOG resources from a Win16 NE binary into a JSON catalogue.

Win16 (Windows 3.x) DLGTEMPLATE format:
  DLGTEMPLATE_HEADER:
    DWORD lStyle
    BYTE  cdit              (item count)
    WORD  x, y, cx, cy
    sz    menu              (variable: 0x00=none, 0xFF+WORD=ordinal, else zero-term str)
    sz    class             (same encoding as menu)
    sz    caption           (zero-term str, may be empty)
    if (lStyle & DS_SETFONT):
        WORD font_height
        sz font_face_name
  Then cdit DLGITEMTEMPLATE:
    WORD x, y, cx, cy
    WORD id
    DWORD lStyle
    sz   class              (variable: 0x80+ord_byte=standard, else zero-term str)
    sz   text               (variable: 0xFF+WORD=ord, else zero-term str)
    BYTE szCreationData     (1-byte length, then data)

Output: JSON dict keyed by dialog id with all strings + their absolute byte offsets
        so we can patch them later.
"""
import sys, struct, json, os

DS_SETFONT = 0x40

NE_RESOURCE_TYPES = {
    0x8005: 'RT_DIALOG',
}

# Standard Windows control classes (Win16 used 0x80-0x85 ordinals)
STANDARD_CLASSES = {
    0x80: 'BUTTON',
    0x81: 'EDIT',
    0x82: 'STATIC',
    0x83: 'LISTBOX',
    0x84: 'SCROLLBAR',
    0x85: 'COMBOBOX',
}


def read_sz_or_ord(data, idx, kind='item_class'):
    """
    Read a variable-length name/text per Win16 dialog spec.
    Returns (kind_str, value, bytes_consumed, payload_offset, payload_len)
      kind_str: 'empty', 'ord', 'str', 'std_class'
      payload_offset/_len: for 'str', the byte range of the ASCII string data
                          (NOT including null terminator); used for patching.
    For item.class:
        0x00 = error/none
        0x80-0x85 = standard control class ordinal
        else = zero-terminated string
    For item.text and dlg.caption/menu/class:
        0x00 = empty (just a single 0 byte)
        0xFF then WORD = ordinal
        else = zero-terminated string
    """
    if idx >= len(data):
        return ('eof', None, 0, idx, 0)
    b = data[idx]

    if kind == 'item_class':
        # 0x80-0x85 single byte standard class ordinal
        if 0x80 <= b <= 0x8F:
            return ('std_class', STANDARD_CLASSES.get(b, f'0x{b:02x}'), 1, idx, 0)
        # else zero-term string
        end = data.index(0, idx) if 0 in data[idx:] else len(data)
        s = data[idx:end].decode('ascii', errors='replace')
        return ('str', s, end - idx + 1, idx, end - idx)

    # 'text' / 'caption' / 'menu' / 'class' (dialog-header variants)
    if b == 0x00:
        return ('empty', '', 1, idx, 0)
    if b == 0xFF:
        if idx + 3 > len(data):
            return ('eof', None, 0, idx, 0)
        ordval = struct.unpack('<H', data[idx+1:idx+3])[0]
        return ('ord', ordval, 3, idx, 0)
    # zero-term string
    end = data.index(0, idx) if 0 in data[idx:] else len(data)
    s = data[idx:end].decode('ascii', errors='replace')
    return ('str', s, end - idx + 1, idx, end - idx)


def parse_dialog_template(data, abs_off, res_len):
    """Parse one RT_DIALOG resource. Returns dict."""
    base = abs_off
    end = abs_off + res_len
    idx = abs_off

    if idx + 13 > end:
        return {'error': 'too short'}

    lStyle = struct.unpack('<I', data[idx:idx+4])[0]; idx += 4
    cdit = data[idx]; idx += 1
    x, y, cx, cy = struct.unpack('<HHHH', data[idx:idx+8]); idx += 8

    dlg = {
        'style': f'0x{lStyle:08x}',
        'rect': [x, y, cx, cy],
        'item_count': cdit,
        'strings': [],   # list of {'role': str, 'text': str, 'file_offset': int, 'byte_len': int}
        'items': [],
    }

    # menu
    kind, val, cons, poff, plen = read_sz_or_ord(data, idx, kind='dlg')
    idx += cons
    dlg['menu'] = {'kind': kind, 'value': val}
    if kind == 'str' and val:
        dlg['strings'].append({'role': 'menu', 'text': val, 'file_offset': poff, 'byte_len': plen})

    # class
    kind, val, cons, poff, plen = read_sz_or_ord(data, idx, kind='dlg')
    idx += cons
    dlg['class'] = {'kind': kind, 'value': val}
    if kind == 'str' and val:
        dlg['strings'].append({'role': 'class', 'text': val, 'file_offset': poff, 'byte_len': plen})

    # caption
    kind, val, cons, poff, plen = read_sz_or_ord(data, idx, kind='dlg')
    idx += cons
    dlg['caption'] = {'kind': kind, 'value': val}
    if kind == 'str' and val:
        dlg['strings'].append({'role': 'caption', 'text': val, 'file_offset': poff, 'byte_len': plen})

    # font (only if DS_SETFONT bit set in style)
    if lStyle & DS_SETFONT:
        if idx + 2 > end:
            return dlg
        font_h = struct.unpack('<H', data[idx:idx+2])[0]
        idx += 2
        # font face name (zero-term)
        end_n = data.index(0, idx) if 0 in data[idx:] else len(data)
        font_face = data[idx:end_n].decode('ascii', errors='replace')
        dlg['font'] = {'height': font_h, 'face': font_face, 'file_offset': idx, 'byte_len': end_n - idx}
        # NOTE: font face is NOT added to translatable strings (we don't want to translate font names)
        idx = end_n + 1

    # Parse cdit items
    for i in range(cdit):
        if idx + 14 > end:
            dlg['items'].append({'error': f'item {i} truncated'})
            break
        ix, iy, icx, icy = struct.unpack('<HHHH', data[idx:idx+8]); idx += 8
        iid = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        iStyle = struct.unpack('<I', data[idx:idx+4])[0]; idx += 4

        item = {
            'index': i,
            'rect': [ix, iy, icx, icy],
            'id': iid,
            'style': f'0x{iStyle:08x}',
        }

        # class
        kind, val, cons, poff, plen = read_sz_or_ord(data, idx, kind='item_class')
        idx += cons
        item['class'] = {'kind': kind, 'value': val}

        # text
        kind, val, cons, poff, plen = read_sz_or_ord(data, idx, kind='dlg')
        idx += cons
        item['text'] = {'kind': kind, 'value': val}
        if kind == 'str' and val:
            dlg['strings'].append({
                'role': f'item[{i}].text',
                'text': val,
                'file_offset': poff,
                'byte_len': plen,
                'item_id': iid,
                'item_class': item['class']['value'],
            })

        # creation data (1-byte len + data)
        if idx < end:
            cd_len = data[idx]; idx += 1
            item['creation_data_len'] = cd_len
            if cd_len:
                idx += cd_len

        dlg['items'].append(item)

    dlg['parsed_end_offset'] = idx
    dlg['expected_end_offset'] = end
    return dlg


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/extracted/CIV.EXE'
    out_json = sys.argv[2] if len(sys.argv) > 2 else '/tmp/civ1/civ_dialogs.json'

    with open(path, 'rb') as f:
        data = f.read()

    e_lfanew = struct.unpack('<I', data[0x3C:0x40])[0]
    ne_off = e_lfanew
    assert data[ne_off:ne_off+2] == b'NE', 'not NE'

    res_off_rel = struct.unpack('<H', data[ne_off+0x24:ne_off+0x26])[0]
    res_abs = ne_off + res_off_rel
    shift = struct.unpack('<H', data[res_abs:res_abs+2])[0]
    unit = 1 << shift

    idx = res_abs + 2
    dialogs = []
    while idx < len(data):
        type_id = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        if type_id == 0: break
        count = struct.unpack('<H', data[idx:idx+2])[0]; idx += 2
        idx += 4  # reserved
        for r in range(count):
            r_off_units = struct.unpack('<H', data[idx:idx+2])[0]
            r_len_units = struct.unpack('<H', data[idx+2:idx+4])[0]
            r_flags = struct.unpack('<H', data[idx+4:idx+6])[0]
            r_id = struct.unpack('<H', data[idx+6:idx+8])[0]
            idx += 12

            if type_id == 0x8005:  # RT_DIALOG
                abs_off = r_off_units * unit
                abs_len = r_len_units * unit
                parsed = parse_dialog_template(data, abs_off, abs_len)
                parsed['resource_id'] = r_id & 0x7FFF
                parsed['file_offset'] = abs_off
                parsed['allocated_bytes'] = abs_len
                dialogs.append(parsed)

    # Write JSON
    with open(out_json, 'w', encoding='utf-8') as f:
        json.dump(dialogs, f, ensure_ascii=False, indent=2)

    # Print summary
    print(f"Found {len(dialogs)} RT_DIALOG resources in {path}")
    print(f"Catalogue written: {out_json}")
    print()

    all_strings = []
    for d in dialogs:
        rid = d.get('resource_id', '?')
        cap = d.get('caption', {}).get('value', '')
        print(f"  Dialog #{rid} @ 0x{d['file_offset']:x} ({d['allocated_bytes']} alloc bytes, {d.get('item_count', 0)} items)")
        if cap:
            print(f"      caption: {cap!r}")
        for s in d.get('strings', []):
            all_strings.append((rid, s['role'], s['text'], s['file_offset'], s['byte_len']))

    print()
    print(f"Total translatable strings: {len(all_strings)}")
    print(f"Unique strings: {len(set(s[2] for s in all_strings))}")
    print()
    print("== Top-30 longest strings ==")
    for s in sorted(all_strings, key=lambda x: -x[4])[:30]:
        rid, role, text, off, blen = s
        print(f"  [#{rid} {role}] ({blen}B @0x{off:x}): {text!r}")


if __name__ == '__main__':
    main()

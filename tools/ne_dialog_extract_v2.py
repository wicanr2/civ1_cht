#!/usr/bin/env python3
"""
Phase 1 translation candidate generator.
Reads civ_dialogs.json from ne_dialog_extract.py and emits a TSV
translation worksheet where role != 'class' (CIVDIALOG class name must
not be translated).
"""
import sys, json

def main():
    json_path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/civ_dialogs.json'
    with open(json_path, encoding='utf-8') as f:
        dialogs = json.load(f)

    # Collect translatable strings (skip class + menu + font face)
    rows = []
    for d in dialogs:
        rid = d.get('resource_id')
        for s in d.get('strings', []):
            role = s['role']
            if role in ('class', 'menu'):
                continue
            rows.append({
                'dlg_id': rid,
                'role': role,
                'file_offset': s['file_offset'],
                'slot_bytes': s['byte_len'],
                'english': s['text'],
                'item_id': s.get('item_id'),
                'item_class': s.get('item_class'),
            })

    # Group by english to find duplicates
    by_english = {}
    for r in rows:
        by_english.setdefault(r['english'], []).append(r)

    print(f"Total translatable string instances: {len(rows)}")
    print(f"Unique English strings: {len(by_english)}")
    print()
    print("=== All translatable strings (grouped by english, sorted by byte budget desc) ===")
    print()
    sorted_uniq = sorted(by_english.items(), key=lambda kv: -kv[1][0]['slot_bytes'])
    for eng, instances in sorted_uniq:
        first = instances[0]
        dups = f" (×{len(instances)} instances)" if len(instances) > 1 else ""
        ctx = f"dlg#{first['dlg_id']} {first['role']}"
        if len(instances) > 1:
            ctx = "[multi] " + ", ".join(f"dlg#{i['dlg_id']}" for i in instances)
        print(f"  slot={first['slot_bytes']:3d}B  {ctx:48s}  {eng!r}")

    # Also dump the full per-dialog walk
    print()
    print("=== Per-dialog item breakdown ===")
    for d in dialogs:
        rid = d.get('resource_id')
        cap = d.get('caption', {}).get('value', '') or ''
        print(f"\n--- Dialog #{rid} ({d.get('item_count', 0)} items, cap={cap!r}) ---")
        for it in d.get('items', []):
            tk = it.get('text', {})
            if tk.get('kind') == 'str' and tk.get('value'):
                cls = it.get('class', {}).get('value', '?')
                rect = it.get('rect', [])
                print(f"  [{it.get('index')}] id={it.get('id')} class={cls} rect={rect}  text={tk['value']!r}")
            elif tk.get('kind') == 'ord':
                print(f"  [{it.get('index')}] id={it.get('id')} class={it.get('class',{}).get('value','?')}  text=<ord {tk['value']}>")
            elif tk.get('kind') == 'empty':
                pass  # too noisy

if __name__ == '__main__':
    main()

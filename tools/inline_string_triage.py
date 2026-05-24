#!/usr/bin/env python3
"""Triage: classify inline strings into translation buckets + emit worksheet TSV."""
import sys, json, re

EXCLUDE_PATTERNS = [
    re.compile(r'^[A-Z][A-Z0-9_]+$'),                          # ALL_CAPS_IDENTIFIER
    re.compile(r'\.(wav|bmp|exe|dll|ini|sav|hlp|txt|rsc|fon|dat|cfg|tmp|RSC|FON|EXE|DLL|WAV|BMP)$'),
    re.compile(r'^(CIVDIALOG|CIVTIMES\d+|CIVBABYLON|CIVZULU|CIVEGYPT|CIVENGLISH|CIVGREEK|CIVRUSSIAN|CIVGERMAN|CIVCHINESE|CIVFRENCH|CIVROMAN|CIVINDIAN|CIVAMERICAN|CIVAZTEC|CIVMONGOL)$'),
    re.compile(r'^(TRandom|PRandom|RandomStatic|RandomRadio|RandomCheck)'),
    re.compile(r'^[^A-Za-z]+$'),                                # pure punctuation/digits
    re.compile(r'^\s+$'),                                       # whitespace only
    re.compile(r'^[a-z][a-z0-9_]+$'),                          # lowercase_identifier (rare)
]

# These are visible to player as labels even if all-caps — keep for translation
KEEP_ALLCAPS = {
    'OK', 'CANCEL', 'HELP', 'EXIT', 'YES', 'NO', 'ABORT',
    'CIVILIZATION', 'CIVILIZATION SCORE', 'CIVILIZATION QUIZ', 'CIVILIZATION RATING: ',
    'CIVILIZATION POWERGraph', 'CIVIL DISORDER', 'CONVERT CIVILIZATION ARTWORK?',
    'SCORING COMPLETED', 'CIVILIZATION for Windows', 'CIVILIZATION Error',
    'EXTRA!', 'FINAL WARNING', 'DISORDER',
}


def is_translatable(text):
    if text in KEEP_ALLCAPS:
        return True
    for pat in EXCLUDE_PATTERNS:
        if pat.search(text):
            return False
    return True


def main():
    in_json = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/inline_strings.json'
    with open(in_json, encoding='utf-8') as f:
        d = json.load(f)
    strings = d['strings']

    # Group by text
    by_text = {}
    for s in strings:
        by_text.setdefault(s['text'], []).append(s)

    # Filter to translatable
    translatable = {}
    skipped = []
    for text, occurrences in by_text.items():
        if is_translatable(text):
            translatable[text] = occurrences
        else:
            skipped.append(text)

    print(f"Total unique strings: {len(by_text)}")
    print(f"Translatable: {len(translatable)}")
    print(f"Skipped (identifier/filename/etc): {len(skipped)}")
    print()

    # Categorize translatable by characteristic
    menu_strings = []      # contain underscore (likely menu groups)
    short_label = []       # <= 20 chars no _
    status_msg = []        # 20-60 chars no _
    long_prose = []        # > 60 chars no _

    for text, occs in translatable.items():
        if '_' in text and len(text) > 8:
            menu_strings.append((text, occs))
        elif len(text) <= 20:
            short_label.append((text, occs))
        elif len(text) <= 60:
            status_msg.append((text, occs))
        else:
            long_prose.append((text, occs))

    print(f"  - menu_strings (with _ delimiter):  {len(menu_strings)}")
    print(f"  - short_label (≤20B):                {len(short_label)}")
    print(f"  - status_msg (20-60B):               {len(status_msg)}")
    print(f"  - long_prose (>60B):                 {len(long_prose)}")
    print()

    # Show samples
    print("=== MENU strings (split by _ for inspection) ===")
    for text, occs in sorted(menu_strings, key=lambda x: -len(x[0]))[:15]:
        parts = text.split('_')
        print(f"  ({len(text):3d}B @0x{occs[0]['offset']:x}):")
        for p in parts:
            print(f"     | {p!r}")
        print()

    print("=== SHORT LABELS (sample 30) ===")
    for text, occs in sorted(short_label, key=lambda x: -len(x[0]))[:30]:
        off = ", ".join(f"0x{o['offset']:x}" for o in occs[:3])
        print(f"  ({len(text):3d}B x{len(occs):2d} @{off}): {text!r}")
    print()

    print("=== STATUS MSG (top 25) ===")
    for text, occs in sorted(status_msg, key=lambda x: -len(x[0]))[:25]:
        print(f"  ({len(text):3d}B): {text!r}")
    print()

    print(f"=== LONG PROSE (>60B, all {len(long_prose)}) ===")
    for text, occs in sorted(long_prose, key=lambda x: -len(x[0])):
        print(f"  ({len(text):3d}B): {text!r}")

if __name__ == '__main__':
    main()

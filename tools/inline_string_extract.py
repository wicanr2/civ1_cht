#!/usr/bin/env python3
"""
Phase 3: extract translatable inline ASCII strings from CIV.EXE data segments.

Strategy:
  1. Scan the whole NE binary for printable ASCII runs >= min_len, null-terminated
  2. Classify each into buckets:
     - api_or_symbol  : all-caps with no spaces, looks like a fn/class name
     - format_only    : has %d %s ^0 ^1 etc but no real prose
     - filename       : has .wav .bmp .exe etc
     - prose          : likely user-visible game text (target for translation)
     - short_word     : single English word, may or may not be UI
  3. Emit JSON catalogue suitable for Phase 3 translation worksheet
"""
import sys, json, re, struct
from collections import Counter

PRINTABLE = set(range(0x20, 0x7F))  # ASCII printable (space to ~)

def extract_strings(data, min_len=6):
    """Find all null-terminated ASCII strings of at least min_len chars."""
    strings = []
    i = 0
    n = len(data)
    while i < n:
        b = data[i]
        if b in PRINTABLE:
            start = i
            while i < n and data[i] in PRINTABLE:
                i += 1
            length = i - start
            # Must be null-terminated for Win16-style C string
            if i < n and data[i] == 0 and length >= min_len:
                strings.append({
                    'offset': start,
                    'length': length,
                    'text': bytes(data[start:i]).decode('ascii'),
                })
        i += 1
    return strings


def classify(text):
    """Classify a string into translation-priority buckets."""
    has_space = ' ' in text
    has_lowercase = any(c.islower() for c in text)
    has_format = bool(re.search(r'%[diuoxXcsfgep]|\^\d|\{\d+\}', text))
    has_filename = bool(re.search(r'\.(wav|bmp|exe|dll|ini|sav|hlp|txt|rsc|fon|dat|cfg|tmp)$', text, re.I))
    is_path_like = bool(re.search(r'^[A-Z]:\\|[/\\]', text))
    all_upper = text.isupper() or (not has_lowercase and text.replace(' ', '').replace('_', '').isalnum())
    is_word_only = re.fullmatch(r'[A-Za-z]+', text) is not None
    likely_id = bool(re.fullmatch(r'[A-Z][A-Z0-9_]+', text))  # IDENTIFIER_STYLE
    has_punct = bool(re.search(r'[.,?!:;]', text))

    if has_filename or is_path_like:
        return 'filename'
    if likely_id:
        return 'api_or_symbol'
    if has_format and not has_lowercase:
        return 'format_only'
    if has_space and has_lowercase:
        return 'prose'
    if has_lowercase and not has_space:
        return 'short_word'
    if has_lowercase:
        return 'prose'
    return 'short_word'


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/civ1/extracted/CIV.EXE'
    out_json = sys.argv[2] if len(sys.argv) > 2 else '/tmp/civ1/inline_strings.json'
    min_len = int(sys.argv[3]) if len(sys.argv) > 3 else 6

    with open(path, 'rb') as f:
        data = f.read()

    print(f"File: {path} ({len(data)} bytes), min_len={min_len}")

    strings = extract_strings(data, min_len)
    print(f"Found {len(strings)} null-terminated printable runs >= {min_len} chars")

    # Classify
    for s in strings:
        s['bucket'] = classify(s['text'])

    by_bucket = Counter(s['bucket'] for s in strings)
    print()
    print("=== bucket distribution ===")
    for bucket, count in by_bucket.most_common():
        print(f"  {bucket:20s}: {count}")

    # Dedupe by text (an English string may appear multiple times at different offsets)
    by_text = {}
    for s in strings:
        by_text.setdefault(s['text'], []).append(s)

    unique = len(by_text)
    print(f"\nUnique texts: {unique}")
    print(f"Texts in 'prose' bucket: {len([t for t,ss in by_text.items() if ss[0]['bucket']=='prose'])}")
    print(f"Texts in 'short_word' bucket: {len([t for t,ss in by_text.items() if ss[0]['bucket']=='short_word'])}")

    with open(out_json, 'w', encoding='utf-8') as f:
        json.dump({
            'source': path,
            'min_len': min_len,
            'strings': strings,
        }, f, ensure_ascii=False, indent=1)

    # Top samples per bucket
    print()
    for bucket in ['prose', 'short_word', 'format_only', 'api_or_symbol', 'filename']:
        items = [t for t,ss in by_text.items() if ss[0]['bucket']==bucket]
        items.sort(key=len, reverse=True)
        print(f"=== bucket '{bucket}' — top 15 by length (of {len(items)} unique) ===")
        for t in items[:15]:
            occ = len(by_text[t])
            print(f"  ({len(t):3d}B, x{occ}): {t!r}")
        print()


if __name__ == '__main__':
    main()

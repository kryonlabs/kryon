#!/usr/bin/env python3
"""Render docs/FEATURE_MATRIX.md to docs/FEATURE_MATRIX.html.

The markdown tables are the source of truth; this gives them a browsable
view with color-coded status cells (green = full, amber = partial, red =
missing). Run it whenever the matrix changes:

    python3 scripts/feature-matrix-html.py

CI can verify the generated HTML is current:

    python3 scripts/feature-matrix-html.py --check
"""

import argparse
import difflib
import html
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def cell_class(c):
    t = c.strip()
    if '✅' in t:
        return 'ok'
    if '◐' in t:
        return 'part'
    if t.startswith('✗'):
        return 'no'
    if t in ('—', '— single framebuffer'):
        return 'na'
    return ''


def inline(s):
    s = html.escape(s)
    s = re.sub(r'`([^`]+)`', r'<code>\1</code>', s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', s)
    return s


def convert(src):
    out = ['''<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Kryon Feature Matrix</title>
<style>
body{font-family:system-ui,sans-serif;max-width:1100px;margin:2rem auto;padding:0 1rem;color:#1a1a1a;background:#fafafa}
h1{border-bottom:3px solid #333;padding-bottom:.4rem} h2{margin-top:2.2rem;border-bottom:1px solid #ccc;padding-bottom:.25rem}
h3{margin-top:1.6rem;color:#333}
table{border-collapse:collapse;margin:1rem 0;font-size:.92rem;width:100%}
th,td{border:1px solid #bbb;padding:.42rem .6rem;text-align:left;vertical-align:top}
th{background:#2b2b2b;color:#fff;position:sticky;top:0}
tr:nth-child(even) td{background:#f2f2f2}
td.ok{background:#d9f2dd !important} td.part{background:#fdeebc !important}
td.no{background:#fbd5d2 !important} td.na{background:#e8e8e8 !important;color:#888}
code{background:#eee;padding:.08rem .3rem;border-radius:3px;font-size:.85em}
pre{background:#222;color:#ddd;padding:1rem;border-radius:6px;overflow-x:auto}
li{margin:.25rem 0}
</style></head><body>''']
    para = []

    def flush():
        if para:
            out.append('<p>' + ' '.join(inline(x) for x in para) + '</p>')
            para.clear()

    i = 0
    while i < len(src):
        line = src[i]
        if line.startswith('|'):
            flush()
            rows = []
            while i < len(src) and src[i].startswith('|'):
                cells = [c.strip() for c in src[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-{2,}:?', c) for c in cells):
                    rows.append(cells)
                i += 1
            out.append('<table>')
            out.append('<tr>' + ''.join(f'<th>{inline(c)}</th>'
                                        for c in rows[0]) + '</tr>')
            for r in rows[1:]:
                out.append('<tr>' + ''.join(
                    f'<td class="{cell_class(c)}">{inline(c)}</td>'
                    for c in r) + '</tr>')
            out.append('</table>')
            continue
        if line.startswith('```'):
            flush()
            i += 1
            buf = []
            while i < len(src) and not src[i].startswith('```'):
                buf.append(src[i])
                i += 1
            i += 1
            out.append('<pre>' + html.escape('\n'.join(buf)) + '</pre>')
            continue
        if line.startswith('#'):
            flush()
            n = len(line) - len(line.lstrip('#'))
            out.append(f'<h{n}>{inline(line.lstrip("# ").strip())}</h{n}>')
        elif line.startswith('- '):
            flush()
            out.append('<ul>')
            while i < len(src) and src[i].startswith('- '):
                out.append('<li>' + inline(src[i][2:]) + '</li>')
                i += 1
            out.append('</ul>')
            continue
        elif line.strip():
            para.append(line.strip())
        else:
            flush()
        i += 1
    flush()
    out.append('</body></html>')
    return '\n'.join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true',
                        help='verify docs/FEATURE_MATRIX.html is current')
    args = parser.parse_args()

    src_path = os.path.join(ROOT, 'docs', 'FEATURE_MATRIX.md')
    dst_path = os.path.join(ROOT, 'docs', 'FEATURE_MATRIX.html')
    with open(src_path) as f:
        src = f.read().splitlines()
    rendered = convert(src) + '\n'
    if args.check:
        with open(dst_path) as f:
            current = f.read()
        if current != rendered:
            diff = difflib.unified_diff(
                current.splitlines(),
                rendered.splitlines(),
                fromfile='docs/FEATURE_MATRIX.html',
                tofile='generated docs/FEATURE_MATRIX.html',
                lineterm='')
            print('docs/FEATURE_MATRIX.html is stale; run '
                  'tests/feature_matrix_docs_test.sh', file=sys.stderr)
            print('\n'.join(diff), file=sys.stderr)
            return 1
    else:
        with open(dst_path, 'w') as f:
            f.write(rendered)
    print(f'rendered {dst_path}: '
           f'{rendered.count("<table>")} tables, '
           f'{rendered.count("<tr>")} rows')


if __name__ == '__main__':
    sys.exit(main())

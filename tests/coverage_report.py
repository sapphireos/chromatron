#!/usr/bin/env python3
# <license>
#
#     This file is part of the Chromatron project.
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# </license>

# Coverage report for the chromatron host unit tests.
#
# Drives `gcov --json-format` (GCC >= 9, ships with the compiler, so it is
# always version-matched) over the .gcno/.gcda files produced by the test
# binaries, and aggregates line coverage for the sources under test.
# Standard library only - no gcovr required. The artifacts are equivalent
# to a gcovr run: summary.txt, summary.json and a Cobertura coverage.xml.
#
# Outputs (in --out):
#   summary.txt    per-file table + overall line coverage
#   summary.json   machine readable (consumed by cov_check.py)
#   coverage.xml   Cobertura (for CI coverage services)
#
# usage: coverage_report.py --objdir <dir>... --root <repo> --out <dir>

import argparse
import gzip
import json
import os
import subprocess
import sys
import tempfile
import time
from xml.sax.saxutils import escape

# report these top-level paths (the Phase 1 scope, incl. the FX4
# compiler/VM exercised by its own test suite)
REPORT_PREFIXES = ('src/sapphireos/', 'src/lib_gfx/', 'src/FX4/')

# harness code, not sources under test (even when it lives in a reported dir)
EXCLUDE_BASENAMES = ('unity.c', 'unity.h')


def to_report_rel(src, *bases):
    """Map a compile-time source path to its repo-relative report form.

    The .gcno files embed the path the source was compiled under, which
    differs between machines and checkouts (this repo has been built
    under several roots), and can be relative when the build ran from
    the source dir (FX4). Try the path as-is, then resolved against the
    given bases (object dir, repo root), anchoring on the reported
    prefix rather than assuming any particular root.
    """
    cands = [src]
    if not os.path.isabs(src):
        cands += [os.path.normpath(os.path.join(b, src)) for b in bases if b]

    for cand in cands:
        for p in REPORT_PREFIXES:
            i = cand.find('/' + p)
            if i >= 0 and cand[i + 1:].startswith(p):
                return cand[i + 1:]

    return None


def collect_lines(gcov, objdir):
    """Return {absolute source path: {line_no: hit_count}} for one suite."""
    data = {}

    gcno_files = []
    for dirpath, _, filenames in os.walk(objdir):
        gcno_files += [os.path.join(dirpath, f) for f in filenames
                       if f.endswith('.gcno')]

    for gcno in sorted(gcno_files):
        with tempfile.TemporaryDirectory() as td:
            r = subprocess.run(
                [gcov, '-o', os.path.dirname(gcno), '--json-format',
                 gcno],
                cwd=td, capture_output=True, text=True)

            # gcc >= 10 writes <name>.gcov.json.gz, older ones <name>.json
            base = os.path.basename(gcno)[:-len('.gcno')]
            jf = None
            for cand in (os.path.join(td, base + '.gcov.json.gz'),
                         os.path.join(td, base + '.json')):
                if os.path.exists(cand):
                    jf = cand
                    break

            if r.returncode != 0 or jf is None:
                sys.exit('gcov failed for %s:\n%s' % (gcno, r.stderr or r.stdout))

            if jf.endswith('.gz'):
                doc = json.load(gzip.open(jf, 'rt'))
            else:
                doc = json.load(open(jf))

        for f in doc.get('files', []):
            lines = {}
            for l in f.get('lines', []):
                if 'count' in l:
                    lines[l['line_number']] = max(l['count'], 0)

            if lines:
                agg = data.setdefault(f['file'], {})
                for ln, c in lines.items():
                    # a line covered by any suite counts as covered
                    agg[ln] = max(agg.get(ln, 0), c)

    return data


def write_cobertura(path, root, files):
    tot = sum(f['lines_total'] for f in files.values())
    cov = sum(f['lines_covered'] for f in files.values())
    rate = (cov / tot) if tot else 0.0

    # group into packages by the directory after src/
    packages = {}
    for rel, f in files.items():
        parts = rel.split('/')
        pkg = parts[1] if len(parts) > 2 and parts[0] == 'src' else 'root'
        packages.setdefault(pkg, []).append((rel, f))

    x = []
    x.append('<?xml version="1.0" ?>')
    x.append('<coverage line-rate="%.4f" branch-rate="0" '
             'lines-covered="%d" lines-valid="%d" '
             'branch-covered="0" branches-valid="0" complexity="0" '
             'version="chromatron-1" timestamp="%d">' %
             (rate, cov, tot, int(time.time())))
    x.append('  <sources><source>%s</source></sources>' % escape(root))
    x.append('  <packages>')
    for pkg in sorted(packages):
        ptot = sum(f['lines_total'] for _, f in packages[pkg])
        pcov = sum(f['lines_covered'] for _, f in packages[pkg])
        x.append('    <package name="%s" line-rate="%.4f">' %
                 (pkg, (pcov / ptot) if ptot else 0.0))
        x.append('      <classes>')
        for rel, f in sorted(packages[pkg]):
            d = os.path.dirname(rel)
            n = os.path.basename(rel)
            x.append('        <class name="%s" filename="%s/%s" '
                     'line-rate="%.4f">' % (escape(n), escape(d),
                                             escape(n), f['line_rate']))
            x.append('          <lines>')
            for ln in sorted(f['lines']):
                x.append('            <line number="%d" hits="%d" '
                         'branch="false"/>' % (ln, f['lines'][ln]))
            x.append('          </lines>')
            x.append('        </class>')
        x.append('      </classes>')
        x.append('    </package>')
    x.append('  </packages>')
    x.append('</coverage>')

    with open(path, 'w') as fh:
        fh.write('\n'.join(x) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--objdir', action='append', required=True)
    ap.add_argument('--root', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--gcov', default='gcov')
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    os.makedirs(args.out, exist_ok=True)

    # gcov dislikes relative -o paths; normalize
    objdirs = [os.path.abspath(d) for d in args.objdir]

    # {relative path: {line: hits}}
    merged = {}
    for objdir in objdirs:
        for src, lines in collect_lines(args.gcov, objdir).items():
            rel = to_report_rel(src, objdir, root)
            if rel is None or os.path.basename(src) in EXCLUDE_BASENAMES:
                continue
            agg = merged.setdefault(rel, {})
            for ln, c in lines.items():
                agg[ln] = max(agg.get(ln, 0), c)

    files = {}
    for rel, lines in merged.items():
        tot = len(lines)
        cov = sum(1 for c in lines.values() if c > 0)
        files[rel] = {'lines_total': tot, 'lines_covered': cov,
                      'line_rate': (cov / tot) if tot else 0.0,
                      'lines': lines}

    tot = sum(f['lines_total'] for f in files.values())
    cov = sum(f['lines_covered'] for f in files.values())
    overall = {'lines_total': tot, 'lines_covered': cov,
               'line_rate': (cov / tot) if tot else 0.0}

    if not files:
        sys.exit('no instrumented sources found - is a suite missing '
                 'from --objdir, or did nothing execute?')

    # --- summary.txt ---
    rows = []
    for rel in sorted(files):
        f = files[rel]
        rows.append('%-48s %6d %6d %8.1f%%' %
                    (rel, f['lines_covered'], f['lines_total'],
                     f['line_rate'] * 100))
    txt = []
    txt.append('chromatron host unit tests - line coverage')
    txt.append('generated: %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
    txt.append('')
    txt.append('%-48s %6s %6s %8s' % ('file', 'cover', 'total', 'pct'))
    txt.extend(rows)
    txt.append('')
    txt.append('%-48s %6d %6d %8.1f%%' %
               ('OVERALL', cov, tot, overall['line_rate'] * 100))
    txt.append('')
    with open(os.path.join(args.out, 'summary.txt'), 'w') as fh:
        fh.write('\n'.join(txt) + '\n')
    print('\n'.join(txt))

    # --- summary.json (no per-line detail; that is the xml's job) ---
    summary = {
        'tool': 'coverage_report.py (gcov)',
        'generated': time.strftime('%Y-%m-%d %H:%M:%S'),
        'overall': {k: overall[k] for k in
                    ('lines_total', 'lines_covered', 'line_rate')},
        'files': {rel: {k: files[rel][k] for k in
                        ('lines_total', 'lines_covered', 'line_rate')}
                  for rel in sorted(files)},
    }
    with open(os.path.join(args.out, 'summary.json'), 'w') as fh:
        json.dump(summary, fh, indent=2)
        fh.write('\n')

    # --- Cobertura xml ---
    write_cobertura(os.path.join(args.out, 'coverage.xml'), root, files)
    print('wrote %s/{summary.txt,summary.json,coverage.xml}' % args.out)


if __name__ == '__main__':
    main()

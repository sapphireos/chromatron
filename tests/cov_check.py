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

# Coverage regression gate. Compares a summary.json (produced by
# coverage_report.py) against the committed baseline.json and fails when
# overall line coverage, or any single file's line coverage, dropped by
# more than the tolerance (default 0.005 = 0.5pp).
#
# This runs on every `make`, independent of whether the report itself was
# regenerated, so the gate cannot be skipped by an up-to-date build.
#
# usage: cov_check.py <summary.json> <baseline.json> [--tolerance 0.005]

import argparse
import json
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('summary')
    ap.add_argument('baseline')
    ap.add_argument('--tolerance', type=float, default=0.005)
    args = ap.parse_args()

    try:
        baseline = json.load(open(args.baseline))

    except (OSError, ValueError):
        print('coverage baseline: none yet '
              '(run `make cov-baseline` to create it)')
        return

    cur = json.load(open(args.summary))
    tol = args.tolerance

    b_all = baseline['overall']
    c_all = cur['overall']

    rows = []
    regressed = False

    for path in sorted(set(cur['files']) | set(baseline['files'])):
        c = cur['files'].get(path)
        b = baseline['files'].get(path)

        if c is None:
            rows.append((path, None, None, 'removed'))
            continue
        if b is None:
            rows.append((path, None, c['line_rate'], 'new'))
            continue

        delta = c['line_rate'] - b['line_rate']
        if delta < -tol:
            regressed = True
        rows.append((path, b['line_rate'], c['line_rate'],
                     '%+.2fpp' % (delta * 100)))

    d_all = c_all['line_rate'] - b_all['line_rate']
    if d_all < -tol:
        regressed = True

    print('coverage vs baseline (tolerance %.1fpp):' % (tol * 100))
    print('  %-40s %10s %10s %10s' % ('file', 'baseline', 'current', 'delta'))
    for path, b, c, note in rows:
        bs = '%.1f%%' % (b * 100) if b is not None else '    -'
        cs = '%.1f%%' % (c * 100) if c is not None else '    -'
        print('  %-40s %10s %10s %10s' % (path, bs, cs, note))
    print('  %-40s %10.1f%% %10.1f%% %9.2fpp' % (
        'OVERALL', b_all['line_rate'] * 100, c_all['line_rate'] * 100,
        d_all * 100))

    if regressed:
        sys.exit('coverage dropped below baseline (see table above)')


if __name__ == '__main__':
    main()

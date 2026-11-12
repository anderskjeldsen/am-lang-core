#!/usr/bin/env python3
"""Audit #noThrow on am-lang-core's native functions (Redmine #183).

#noThrow on a native is trusted, not verified: an exception the C raises anyway
stops at the call site, silently. This script reads every platform's C
implementation (src/native-c/<platform>/...) of each native declared in
src/am-lang and reports:

  WRONG      #noThrow, but some implementation throws (exit status 1)
  CANDIDATE  no #noThrow, and no implementation throws
  THROWS     no #noThrow, and some implementation throws (fine)
             A candidate that implements an abstract or base function (FileStream.read
             implements Stream.read) can take #noThrow only if every implementation of
             it can; the compiler rejects it otherwise. FileStream's read, write,
             readByte and writeByte stay unannotated for that reason.
  NOIMPL     no implementation found under src/native-c (core.c / reflection.c
             hand-dispatch, or not implemented); not judged

"Throws" means the C function body calls __throw*, sets .exception / ->exception,
tests an exception it got from a call, or hands &__result to a helper. Read the C
before adding #noThrow to a CANDIDATE: a body can also run user code, and an
implementation can be added for a new platform later.

Usage: tools/check-nothrow.py [--all]   (--all also prints THROWS and NOIMPL)
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THROW = re.compile(r'__throw|\.exception\s*=|->exception\s*=|exception\s*!=\s*NULL|&__result')


def natives():
    out = []
    base = os.path.join(ROOT, 'src', 'am-lang')
    for dp, _, fs in os.walk(base):
        for f in fs:
            if not f.endswith('.aml'):
                continue
            p = os.path.join(dp, f)
            lines = open(p, errors='replace').read().split('\n')
            in_block = False
            for i, line in enumerate(lines):
                s = line.strip()
                if in_block:
                    if '*/' in s:
                        in_block = False
                    continue
                if s.startswith('/*') and '*/' not in s:
                    in_block = True
                    continue
                if s.startswith('//') or s.startswith('*'):
                    continue
                m = re.search(r'\bnative\b(?:\s+\w+)*\s+fun\s+(\w+)', line)
                if not m or re.search(r'native\s+class', line):
                    continue
                j, no_throw, in_doc = i - 1, False, False
                while j >= 0:
                    t = lines[j].strip()
                    if t.endswith('*/'):
                        in_doc = True
                    if in_doc:
                        if t.startswith('/*'):
                            in_doc = False
                        j -= 1
                        continue
                    if t == '' or t.startswith('//'):
                        j -= 1
                        continue
                    if t.startswith('#'):
                        no_throw = no_throw or t.startswith('#noThrow')
                        j -= 1
                        continue
                    break
                rel = os.path.relpath(p, base)
                out.append((rel, i + 1, m.group(1), no_throw, bool(re.search(r'\boverride\b', line))))
    return out


def implementations():
    defs = {}
    base = os.path.join(ROOT, 'src', 'native-c')
    for dp, _, fs in os.walk(base):
        for f in fs:
            if not f.endswith('.c'):
                continue
            p = os.path.join(dp, f)
            text = open(p, errors='replace').read()
            for m in re.finditer(r'^\w[\w \*]*?\b(Am_\w+?)_(\d+)\s*\(([^)]*)\)\s*\{', text, re.M):
                i, depth = m.end(), 1
                while depth and i < len(text):
                    depth += (text[i] == '{') - (text[i] == '}')
                    i += 1
                platform = os.path.relpath(p, base).split(os.sep)[0]
                defs.setdefault(m.group(1), []).append((platform, text[m.end():i]))
    return defs


def main():
    show_all = '--all' in sys.argv
    defs = implementations()
    wrong = 0
    counts = {}
    for rel, line, name, no_throw, is_override in natives():
        key = rel[:-4].replace(os.sep, '_') + '_' + name
        impls = defs.get(key, [])
        throwing = sorted({plat for plat, body in impls if THROW.search(body)})
        if not impls:
            verdict = 'NOIMPL'
        elif no_throw:
            verdict = 'WRONG' if throwing else 'ok'
        else:
            verdict = 'THROWS' if throwing else 'CANDIDATE'
        counts[verdict] = counts.get(verdict, 0) + 1
        if verdict == 'WRONG':
            wrong += 1
        if verdict in ('WRONG', 'CANDIDATE') or (show_all and verdict in ('THROWS', 'NOIMPL')):
            detail = ' (throws on: ' + ', '.join(throwing) + ')' if throwing else ''
            print(f'{verdict:9} {rel}:{line} {name}{detail}')
    print(', '.join(f'{k} {v}' for k, v in sorted(counts.items())))
    if counts.get('CANDIDATE'):
        print('A candidate that implements an abstract/base function needs every implementation to be #noThrow (the compiler checks).')
    return 1 if wrong else 0


if __name__ == '__main__':
    sys.exit(main())

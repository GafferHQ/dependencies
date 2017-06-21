#
# qmake mini-parser for config vars
#
# Copyright (c) 2015 Tismer Software Consulting
#
# This code can be redistributed freely, as long as the authorship
# of Tismer Software Consulting is kept in this file.
#

from __future__ import print_function, unicode_literals, division, absolute_import

import sys
import os
import re
import itertools
import collections
import subprocess

'''
This module reads qmake files and their include files.
The reason that this module exists is primarily:

There is a windows setting that defaults to "-Zc:wchar_t" in the Qt5 downloads
for Windows, but for some reason, PySide had since 2011 the default "-Zc:wchar_t-".

To make this choice automatic, after the qmake executable has been set, I thought
to remove this source of incomprehensible compile crashes and wrote this little
interpreter for qmake config files.

Right now, this script is quite primitive and cannot parse all of the qmake settings.
Therefore, I use it only to figure out, which version of wchar_t was used.
In the future, this could be the basis of a much more sophisticated qmake parser.

Update 2015-09-20:
Recently, I found out that the wchar_t setting was totally correct for Qt4, it
just had been changed for Qt5. So there is no need to have a parser, we just change it.
The parser still may make some sense, so I kept it here, for those who are interested.
'''

# this nice piece of code is from Alex Martelli 2009
# http://stackoverflow.com/a/1518097
class IteratorWithLookahead(collections.Iterator):
    def __init__(self, it):
        self.it, self.nextit = itertools.tee(iter(it))
        self._advance()
    def _advance(self):
        self.lookahead = next(self.nextit, None)
    def __next__(self):
        self._advance()
        return next(self.it)
    next = __next__ # for python 2.7

# small addition to give boolean behavior
class BoolIterator(IteratorWithLookahead):
    def __bool__(self):
        return False if self.lookahead is None else True
    __nonzero__ = __bool__              # python 2.x


class SourceFile(object):
    '''
    A source file reads all lines.

    '''
    def __init__(self, fname):
        self.filename = fname
        self.name = os.path.basename(fname)
        with open(fname) as fin:
            self.lines = fin.readlines()

    def _parsed_lines(self):
        '''
        reads all lines, concatenates continuations,
        strips and removes comments.
        '''
        line_seq = enumerate(self.lines)
        for idx, line in line_seq:
            while line.endswith('\\\n'):
                _, xline = next(line_seq)
                line = line[:-2] + xline
            line = line.rstrip()
            yield(idx, line)

    def parsed_lines(self):
        return BoolIterator(self._parsed_lines())


def parse_qmake(initial_fname):
    source_stack = list()
    def push(ob):
        source_stack.append(ob)
    def pop():
        return source_stack.pop()
    def filter_line(line):
        if '#' in line:
            line = line[:line.index('#')]
        return line.rstrip()
    def test_include(line):
        pat = r'\s*include\('
        return re.match(pat, line)
    def get_include(line):
        line = line.strip()
        pat = r'include\((.*)\)'
        found = re.match(pat, line)
        if found:
            return found.group(1)
        # XXX handle quotes

    curr_file = SourceFile(initial_fname)
    source_seq = curr_file.parsed_lines()
    push((curr_file, source_seq))
    while source_stack:
        curr_file, source_seq = pop()
        while source_seq:
            idx, line = next(source_seq)
            filtered = filter_line(line)
            if filtered:
                yield(curr_file.name, idx, filtered)
                if test_include(filtered):
                    fname = get_include(filtered)
                    if not os.path.isabs(fname):
                        folder, _ = os.path.split(curr_file.filename)
                        fname = os.path.normpath(os.path.join(folder, fname))
                    push((curr_file, source_seq))
                    curr_file = SourceFile(fname)
                    source_seq = curr_file.parsed_lines()

VARS = {}

def collect_var(line, dic=VARS):
    '''
    we are roughly looking for "name = value" or "name += value".
    Comments should be already filtered.
    '''
    pat = r'^\s*(\w+)\s*(\+?)=\s*(.*)$'
    found = re.match(pat, line)
    if found:
        key, addflag, val = found.groups()
        if addflag:
            oldval = VARS.get(key, None)
            if oldval is not None:
                val = oldval + ' ' + val
        else:
            # assert key not in VARS
            if key in VARS:
                print('*** duplicate', VARS[key], 'overwrite', val)
        VARS[key] = val


if __name__ == '__main__':
    # startpath = '/Users/tismer/Qt5.4.2/5.4/clang_64/mkspecs/macx-clang/qmake.conf'
    startpath = '/Users/tismer/Qt5.4.2/5.4/clang_64/mkspecs/win32-msvc2010/qmake.conf'
    # we need the qmake exe to obtain the QMAKE_SPEC variable.
    # Then, we use <qmakebin>/../mkspecs/${QMAKE_SPEC}/qmake.conf
    if len(sys.argv) > 1:
        qmake_exepath = sys.argv[1]
        if not os.path.exists(qmake_exepath):
            raise SystemError("qmake.exe not found at" + qmake_exepath)
        output = subprocess.check_output([qmake_exepath, '-query'])
        print(output)
    seq = parse_qmake(startpath)
    for tup in seq:
        ##print(*tup)
        collect_var(tup[-1])
    assert "QMAKE_CFLAGS" in VARS
    print(VARS["QMAKE_CFLAGS"])
    import pprint
    pprint.pprint(VARS)
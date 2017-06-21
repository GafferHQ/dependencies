#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import json
import os
import re
import subprocess
import sys
import unittest

import PRESUBMIT
from PRESUBMIT_test_mocks import MockChange, MockFile, MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_TEST_DATA_DIR = 'base/test/data/presubmit'

class IncludeOrderTest(unittest.TestCase):
  def testSystemHeaderOrder(self):
    scope = [(1, '#include <csystem.h>'),
             (2, '#include <cppsystem>'),
             (3, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(0, len(warnings))

  def testSystemHeaderOrderMismatch1(self):
    scope = [(10, '#include <cppsystem>'),
             (20, '#include <csystem.h>'),
             (30, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(1, len(warnings))
    self.assertTrue('20' in warnings[0])

  def testSystemHeaderOrderMismatch2(self):
    scope = [(10, '#include <cppsystem>'),
             (20, '#include "acustom.h"'),
             (30, '#include <csystem.h>')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(1, len(warnings))
    self.assertTrue('30' in warnings[0])

  def testSystemHeaderOrderMismatch3(self):
    scope = [(10, '#include "acustom.h"'),
             (20, '#include <csystem.h>'),
             (30, '#include <cppsystem>')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(2, len(warnings))
    self.assertTrue('20' in warnings[0])
    self.assertTrue('30' in warnings[1])

  def testAlphabeticalOrderMismatch(self):
    scope = [(10, '#include <csystem.h>'),
             (15, '#include <bsystem.h>'),
             (20, '#include <cppsystem>'),
             (25, '#include <bppsystem>'),
             (30, '#include "bcustom.h"'),
             (35, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(3, len(warnings))
    self.assertTrue('15' in warnings[0])
    self.assertTrue('25' in warnings[1])
    self.assertTrue('35' in warnings[2])

  def testSpecialFirstInclude1(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude2(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/other/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude3(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo_platform.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude4(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/bar.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo_platform.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))
    self.assertTrue('2' in warnings[0])

  def testSpecialFirstInclude5(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/other/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo-suffix.h', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude6(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/other/path/foo_win.h"',
                '#include <set>',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo_unittest_win.h', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testOrderAlreadyWrong(self):
    scope = [(1, '#include "b.h"'),
             (2, '#include "a.h"'),
             (3, '#include "c.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [3])
    self.assertEqual(0, len(warnings))

  def testConflictAdded1(self):
    scope = [(1, '#include "a.h"'),
             (2, '#include "c.h"'),
             (3, '#include "b.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [2])
    self.assertEqual(1, len(warnings))
    self.assertTrue('3' in warnings[0])

  def testConflictAdded2(self):
    scope = [(1, '#include "c.h"'),
             (2, '#include "b.h"'),
             (3, '#include "d.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [2])
    self.assertEqual(1, len(warnings))
    self.assertTrue('2' in warnings[0])

  def testIfElifElseEndif(self):
    mock_input_api = MockInputApi()
    contents = ['#include "e.h"',
                '#define foo',
                '#include "f.h"',
                '#undef foo',
                '#include "e.h"',
                '#if foo',
                '#include "d.h"',
                '#elif bar',
                '#include "c.h"',
                '#else',
                '#include "b.h"',
                '#endif',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testExcludedIncludes(self):
    # #include <sys/...>'s can appear in any order.
    mock_input_api = MockInputApi()
    contents = ['#include <sys/b.h>',
                '#include <sys/a.h>']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include <atlbase.h>',
                '#include <aaa.h>']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include "build/build_config.h"',
                '#include "aaa.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testCheckOnlyCFiles(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    contents = ['#include <b.h>',
                '#include <a.h>']
    mock_file_cc = MockFile('something.cc', contents)
    mock_file_h = MockFile('something.h', contents)
    mock_file_other = MockFile('something.py', contents)
    mock_input_api.files = [mock_file_cc, mock_file_h, mock_file_other]
    warnings = PRESUBMIT._CheckIncludeOrder(mock_input_api, mock_output_api)
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))
    self.assertEqual('promptOrNotify', warnings[0].type)

  def testUncheckableIncludes(self):
    mock_input_api = MockInputApi()
    contents = ['#include <windows.h>',
                '#include "b.h"',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))

    contents = ['#include "gpu/command_buffer/gles_autogen.h"',
                '#include "b.h"',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))

    contents = ['#include "gl_mock_autogen.h"',
                '#include "b.h"',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))

    contents = ['#include "ipc/some_macros.h"',
                '#include "b.h"',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))


class VersionControlConflictsTest(unittest.TestCase):
  def testTypicalConflict(self):
    lines = ['<<<<<<< HEAD',
             '  base::ScopedTempDir temp_dir_;',
             '=======',
             '  ScopedTempDir temp_dir_;',
             '>>>>>>> master']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(3, len(errors))
    self.assertTrue('1' in errors[0])
    self.assertTrue('3' in errors[1])
    self.assertTrue('5' in errors[2])

  def testIgnoresReadmes(self):
    lines = ['A First Level Header',
             '====================',
             '',
             'A Second Level Header',
             '---------------------']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/polymer/README.md', lines))
    self.assertEqual(0, len(errors))

class UmaHistogramChangeMatchedOrNotTest(unittest.TestCase):
  def testTypicalCorrectlyMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram name="Bla.Foo.Dummy"> </histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalNotMatchedChange(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('some/path/foo.cc', diff_cc)]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)

  def testTypicalNotMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu.Dummy"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('warning', warnings[0].type)

  def testTypicalCorrectlyMatchedChangeViaSuffixes(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Bla.Foo.Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Bla.Foo"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

  def testTypicalCorrectlyMatchedChangeViaSuffixesWithSeparator(self):
    diff_cc = ['UMA_HISTOGRAM_BOOL("Snafu_Dummy", true)']
    diff_xml = ['<histogram_suffixes name="SuperHistogram" separator="_">',
                '  <suffix name="Dummy"/>',
                '  <affected-histogram name="Snafu"/>',
                '</histogram>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', diff_cc),
      MockFile('tools/metrics/histograms/histograms.xml', diff_xml),
    ]
    warnings = PRESUBMIT._CheckUmaHistogramChanges(mock_input_api,
                                                   MockOutputApi())
    self.assertEqual(0, len(warnings))

class BadExtensionsTest(unittest.TestCase):
  def testBadRejFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', ''),
      MockFile('some/path/foo.cc.rej', ''),
      MockFile('some/path2/bar.h.rej', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(2, len(results[0].items))
    self.assertTrue('foo.cc.rej' in results[0].items[0])
    self.assertTrue('bar.h.rej' in results[0].items[1])

  def testBadOrigFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h.orig', ''),
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(1, len(results[0].items))
    self.assertTrue('qux.h.orig' in results[0].items[0])

  def testGoodFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]
    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))


class CheckSingletonInHeadersTest(unittest.TestCase):
  def testSingletonInArbitraryHeader(self):
    diff_singleton_h = ['base::subtle::AtomicWord '
                        'Singleton<Type, Traits, DifferentiatingType>::']
    diff_foo_h = ['// Singleton<Foo> in comment.',
                  'friend class Singleton<Foo>']
    diff_bad_h = ['Foo* foo = Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('base/memory/singleton.h',
                                     diff_singleton_h),
                            MockAffectedFile('foo.h', diff_foo_h),
                            MockAffectedFile('bad.h', diff_bad_h)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual('error', warnings[0].type)
    self.assertTrue('Found Singleton<T>' in warnings[0].message)

  def testSingletonInCC(self):
    diff_cc = ['Foo* foo = Singleton<Foo>::get();']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockAffectedFile('some/path/foo.cc', diff_cc)]
    warnings = PRESUBMIT._CheckSingletonInHeaders(mock_input_api,
                                                  MockOutputApi())
    self.assertEqual(0, len(warnings))


class InvalidOSMacroNamesTest(unittest.TestCase):
  def testInvalidOSMacroNames(self):
    lines = ['#if defined(OS_WINDOWS)',
             ' #elif defined(OS_WINDOW)',
             ' # if defined(OS_MACOSX) || defined(OS_CHROME)',
             '# else  // defined(OS_MAC)',
             '#endif  // defined(OS_MACOS)']
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(len(lines), len(errors))
    self.assertTrue(':1 OS_WINDOWS' in errors[0])
    self.assertTrue('(did you mean OS_WIN?)' in errors[0])

  def testValidOSMacroNames(self):
    lines = ['#if defined(%s)' % m for m in PRESUBMIT._VALID_OS_MACROS]
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(0, len(errors))


class InvalidIfDefinedMacroNamesTest(unittest.TestCase):
  def testInvalidIfDefinedMacroNames(self):
    lines = ['#if defined(TARGET_IPHONE_SIMULATOR)',
             '#if !defined(TARGET_IPHONE_SIMULATOR)',
             '#elif defined(TARGET_IPHONE_SIMULATOR)',
             '#ifdef TARGET_IPHONE_SIMULATOR',
             ' # ifdef TARGET_IPHONE_SIMULATOR',
             '# if defined(VALID) || defined(TARGET_IPHONE_SIMULATOR)',
             '# else  // defined(TARGET_IPHONE_SIMULATOR)',
             '#endif  // defined(TARGET_IPHONE_SIMULATOR)',]
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.mm', lines))
    self.assertEqual(len(lines), len(errors))

  def testValidIfDefinedMacroNames(self):
    lines = ['#if defined(FOO)',
             '#ifdef BAR',]
    errors = PRESUBMIT._CheckForInvalidIfDefinedMacrosInFile(
        MockInputApi(), MockFile('some/path/source.cc', lines))
    self.assertEqual(0, len(errors))


class CheckAddedDepsHaveTetsApprovalsTest(unittest.TestCase):
  def testFilesToCheckForIncomingDeps(self):
    changed_lines = [
      '"+breakpad",',
      '"+chrome/installer",',
      '"+chrome/plugin/chrome_content_plugin_client.h",',
      '"+chrome/utility/chrome_content_utility_client.h",',
      '"+chromeos/chromeos_paths.h",',
      '"+components/crash",',
      '"+components/nacl/common",',
      '"+content/public/browser/render_process_host.h",',
      '"+jni/fooblat.h",',
      '"+grit",  # For generated headers',
      '"+grit/generated_resources.h",',
      '"+grit/",',
      '"+policy",  # For generated headers and source',
      '"+sandbox",',
      '"+tools/memory_watcher",',
      '"+third_party/lss/linux_syscall_support.h",',
    ]
    files_to_check = PRESUBMIT._FilesToCheckForIncomingDeps(re, changed_lines)
    expected = set([
      'breakpad/DEPS',
      'chrome/installer/DEPS',
      'chrome/plugin/chrome_content_plugin_client.h',
      'chrome/utility/chrome_content_utility_client.h',
      'chromeos/chromeos_paths.h',
      'components/crash/DEPS',
      'components/nacl/common/DEPS',
      'content/public/browser/render_process_host.h',
      'policy/DEPS',
      'sandbox/DEPS',
      'tools/memory_watcher/DEPS',
      'third_party/lss/linux_syscall_support.h',
    ])
    self.assertEqual(expected, files_to_check);


class JSONParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_json.json'
    contents = ['// This is a comment.',
                '{',
                '  "key1": ["value1", "value2"],',
                '  "key2": 3  // This is an inline comment.',
                '}'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_json_1.json',
       ['{ x }'],
       'Expecting property name:'),
      ('invalid_json_2.json',
       ['// Hello world!',
        '{ "hello": "world }'],
       'Unterminated string starting at:'),
      ('invalid_json_3.json',
       ['{ "a": "b", "c": "d", }'],
       'Expecting property name:'),
      ('invalid_json_4.json',
       ['{ "a": "b" "c": "d" }'],
       'Expecting , delimiter:'),
      ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetJSONParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))

  def testNoEatComments(self):
    input_api = MockInputApi()
    file_with_comments = 'file_with_comments.json'
    contents_with_comments = ['// This is a comment.',
                              '{',
                              '  "key1": ["value1", "value2"],',
                              '  "key2": 3  // This is an inline comment.',
                              '}'
                              ]
    file_without_comments = 'file_without_comments.json'
    contents_without_comments = ['{',
                                 '  "key1": ["value1", "value2"],',
                                 '  "key2": 3',
                                 '}'
                                 ]
    input_api.files = [MockFile(file_with_comments, contents_with_comments),
                       MockFile(file_without_comments,
                                contents_without_comments)]

    self.assertEqual('No JSON object could be decoded',
                     str(PRESUBMIT._GetJSONParseError(input_api,
                                                      file_with_comments,
                                                      eat_comments=False)))
    self.assertEqual(None,
                     PRESUBMIT._GetJSONParseError(input_api,
                                                  file_without_comments,
                                                  eat_comments=False))


class IDLParsingTest(unittest.TestCase):
  def testSuccess(self):
    input_api = MockInputApi()
    filename = 'valid_idl_basics.idl'
    contents = ['// Tests a valid IDL file.',
                'namespace idl_basics {',
                '  enum EnumType {',
                '    name1,',
                '    name2',
                '  };',
                '',
                '  dictionary MyType1 {',
                '    DOMString a;',
                '  };',
                '',
                '  callback Callback1 = void();',
                '  callback Callback2 = void(long x);',
                '  callback Callback3 = void(MyType1 arg);',
                '  callback Callback4 = void(EnumType type);',
                '',
                '  interface Functions {',
                '    static void function1();',
                '    static void function2(long x);',
                '    static void function3(MyType1 arg);',
                '    static void function4(Callback1 cb);',
                '    static void function5(Callback2 cb);',
                '    static void function6(Callback3 cb);',
                '    static void function7(Callback4 cb);',
                '  };',
                '',
                '  interface Events {',
                '    static void onFoo1();',
                '    static void onFoo2(long x);',
                '    static void onFoo2(MyType1 arg);',
                '    static void onFoo3(EnumType type);',
                '  };',
                '};'
                ]
    input_api.files = [MockFile(filename, contents)]
    self.assertEqual(None,
                     PRESUBMIT._GetIDLParseError(input_api, filename))

  def testFailure(self):
    input_api = MockInputApi()
    test_data = [
      ('invalid_idl_1.idl',
       ['//',
        'namespace test {',
        '  dictionary {',
        '    DOMString s;',
        '  };',
        '};'],
       'Unexpected "{" after keyword "dictionary".\n'),
      # TODO(yoz): Disabled because it causes the IDL parser to hang.
      # See crbug.com/363830.
      # ('invalid_idl_2.idl',
      #  (['namespace test {',
      #    '  dictionary MissingSemicolon {',
      #    '    DOMString a',
      #    '    DOMString b;',
      #    '  };',
      #    '};'],
      #   'Unexpected symbol DOMString after symbol a.'),
      ('invalid_idl_3.idl',
       ['//',
        'namespace test {',
        '  enum MissingComma {',
        '    name1',
        '    name2',
        '  };',
        '};'],
       'Unexpected symbol name2 after symbol name1.'),
      ('invalid_idl_4.idl',
       ['//',
        'namespace test {',
        '  enum TrailingComma {',
        '    name1,',
        '    name2,',
        '  };',
        '};'],
       'Trailing comma in block.'),
      ('invalid_idl_5.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(;',
        '};'],
       'Unexpected ";" after "(".'),
      ('invalid_idl_6.idl',
       ['//',
        'namespace test {',
        '  callback Callback1 = void(long );',
        '};'],
       'Unexpected ")" after symbol long.'),
      ('invalid_idl_7.idl',
       ['//',
        'namespace test {',
        '  interace Events {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Unexpected symbol Events after symbol interace.'),
      ('invalid_idl_8.idl',
       ['//',
        'namespace test {',
        '  interface NotEvent {',
        '    static void onFoo1();',
        '  };',
        '};'],
       'Did not process Interface Interface(NotEvent)'),
      ('invalid_idl_9.idl',
       ['//',
        'namespace test {',
        '  interface {',
        '    static void function1();',
        '  };',
        '};'],
       'Interface missing name.'),
      ]

    input_api.files = [MockFile(filename, contents)
                       for (filename, contents, _) in test_data]

    for (filename, _, expected_error) in test_data:
      actual_error = PRESUBMIT._GetIDLParseError(input_api, filename)
      self.assertTrue(expected_error in str(actual_error),
                      "'%s' not found in '%s'" % (expected_error, actual_error))


class TryServerMasterTest(unittest.TestCase):
  def testTryServerMasters(self):
    bots = {
        'tryserver.chromium.mac': [
            'ios_dbg_simulator',
            'ios_rel_device',
            'ios_rel_device_ninja',
            'mac_asan',
            'mac_asan_64',
            'mac_chromium_compile_dbg',
            'mac_chromium_compile_rel',
            'mac_chromium_dbg',
            'mac_chromium_rel',
            'mac_nacl_sdk',
            'mac_nacl_sdk_build',
            'mac_rel_naclmore',
            'mac_valgrind',
            'mac_x64_rel',
            'mac_xcodebuild',
        ],
        'tryserver.chromium.linux': [
            'android_aosp',
            'android_chromium_gn_compile_dbg',
            'android_chromium_gn_compile_rel',
            'android_clang_dbg',
            'android_dbg',
            'android_dbg_recipe',
            'android_dbg_triggered_tests',
            'android_dbg_triggered_tests_recipe',
            'android_fyi_dbg',
            'android_fyi_dbg_triggered_tests',
            'android_rel',
            'android_rel_triggered_tests',
            'android_x86_dbg',
            'blink_android_compile_dbg',
            'blink_android_compile_rel',
            'blink_presubmit',
            'chromium_presubmit',
            'linux_arm_cross_compile',
            'linux_arm_tester',
            'linux_chromeos_asan',
            'linux_chromeos_browser_asan',
            'linux_chromeos_valgrind',
            'linux_chromium_chromeos_dbg',
            'linux_chromium_chromeos_rel',
            'linux_chromium_compile_dbg',
            'linux_chromium_compile_rel',
            'linux_chromium_dbg',
            'linux_chromium_gn_dbg',
            'linux_chromium_gn_rel',
            'linux_chromium_rel',
            'linux_chromium_trusty32_dbg',
            'linux_chromium_trusty32_rel',
            'linux_chromium_trusty_dbg',
            'linux_chromium_trusty_rel',
            'linux_clang_tsan',
            'linux_ecs_ozone',
            'linux_layout',
            'linux_layout_asan',
            'linux_layout_rel',
            'linux_layout_rel_32',
            'linux_nacl_sdk',
            'linux_nacl_sdk_bionic',
            'linux_nacl_sdk_bionic_build',
            'linux_nacl_sdk_build',
            'linux_redux',
            'linux_rel_naclmore',
            'linux_rel_precise32',
            'linux_valgrind',
            'tools_build_presubmit',
        ],
        'tryserver.chromium.win': [
            'win8_aura',
            'win8_chromium_dbg',
            'win8_chromium_rel',
            'win_chromium_compile_dbg',
            'win_chromium_compile_rel',
            'win_chromium_dbg',
            'win_chromium_rel',
            'win_chromium_rel',
            'win_chromium_x64_dbg',
            'win_chromium_x64_rel',
            'win_drmemory',
            'win_nacl_sdk',
            'win_nacl_sdk_build',
            'win_rel_naclmore',
         ],
    }
    for master, bots in bots.iteritems():
      for bot in bots:
        self.assertEqual(master, PRESUBMIT.GetTryServerMasterForBot(bot),
                         'bot=%s: expected %s, computed %s' % (
            bot, master, PRESUBMIT.GetTryServerMasterForBot(bot)))


class UserMetricsActionTest(unittest.TestCase):
  def testUserMetricsActionInActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("AboutChrome")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    self.assertEqual(
      [], PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi()))


  def testUserMetricsActionNotAddedToActions(self):
    input_api = MockInputApi()
    file_with_user_action = 'file_with_user_action.cc'
    contents_with_user_action = [
      'base::UserMetricsAction("NotInActionsXml")'
    ]

    input_api.files = [MockFile(file_with_user_action,
                                contents_with_user_action)]

    output = PRESUBMIT._CheckUserActionUpdate(input_api, MockOutputApi())
    self.assertEqual(
      ('File %s line %d: %s is missing in '
       'tools/metrics/actions/actions.xml. Please run '
       'tools/metrics/actions/extract_actions.py to update.'
       % (file_with_user_action, 1, 'NotInActionsXml')),
      output[0].message)


class LogUsageTest(unittest.TestCase):

  def testCheckAndroidCrLogUsage(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()

    mock_input_api.files = [
      MockAffectedFile('RandomStuff.java', [
        'random stuff'
      ]),
      MockAffectedFile('HasAndroidLog.java', [
        'import android.util.Log;',
        'some random stuff',
        'Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasExplicitUtilLog.java', [
        'some random stuff',
        'android.util.Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('IsInBasePackage.java', [
        'package org.chromium.base;',
        'private static final String TAG = "cr.Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('IsInBasePackageButImportsLog.java', [
        'package org.chromium.base;',
        'import android.util.Log;',
        'private static final String TAG = "cr.Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasBothLog.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr.Foo";',
        'Log.d(TAG, "foo");',
        'android.util.Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasCorrectTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr.Foo";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasShortCorrectTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasNoTagDecl.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasIncorrectTagDecl.java', [
        'import org.chromium.base.Log;',
        'private static final String TAHG = "cr.Foo";',
        'some random stuff',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasInlineTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr.Foo";',
        'Log.d("TAG", "foo");',
      ]),
      MockAffectedFile('HasIncorrectTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "rubbish";',
        'Log.d(TAG, "foo");',
      ]),
      MockAffectedFile('HasTooLongTag.java', [
        'import org.chromium.base.Log;',
        'some random stuff',
        'private static final String TAG = "cr.24_charachers_long___";',
        'Log.d(TAG, "foo");',
      ]),
    ]

    msgs = PRESUBMIT._CheckAndroidCrLogUsage(
        mock_input_api, mock_output_api)

    self.assertEqual(4, len(msgs))

    # Declaration format
    self.assertEqual(3, len(msgs[0].items))
    self.assertTrue('HasNoTagDecl.java' in msgs[0].items)
    self.assertTrue('HasIncorrectTagDecl.java' in msgs[0].items)
    self.assertTrue('HasIncorrectTag.java' in msgs[0].items)

    # Tag length
    self.assertEqual(1, len(msgs[1].items))
    self.assertTrue('HasTooLongTag.java' in msgs[1].items)

    # Tag must be a variable named TAG
    self.assertEqual(1, len(msgs[2].items))
    self.assertTrue('HasInlineTag.java:4' in msgs[2].items)

    # Util Log usage
    self.assertEqual(2, len(msgs[3].items))
    self.assertTrue('HasAndroidLog.java:3' in msgs[3].items)
    self.assertTrue('IsInBasePackageButImportsLog.java:4' in msgs[3].items)


if __name__ == '__main__':
  unittest.main()

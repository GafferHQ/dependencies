# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import Queue
import re
import subprocess
import sys
import threading
import time

from mopy.config import Config
from mopy.paths import Paths


def set_color():
  """Run gtests with color on TTY, unless its environment variable is set."""
  if sys.stdout.isatty() and "GTEST_COLOR" not in os.environ:
    logging.getLogger().debug("Setting GTEST_COLOR=yes")
    os.environ["GTEST_COLOR"] = "yes"


def run_apptest(config, shell, args, apptest, isolate):
  """Run the apptest; optionally isolating fixtures across shell invocations.

  Returns the list of tests run and the list of failures.

  Args:
    config: The mopy.config.Config for the build.
    shell: The mopy.android.AndroidShell, if Android is the target platform.
    args: The arguments for the shell or apptest.
    apptest: The application test URL.
    isolate: True if the test fixtures should be run in isolation.
  """
  tests = [apptest]
  failed = []
  if not isolate:
    # TODO(msw): Parse fixture-granular successes and failures in this case.
    # TODO(msw): Retry fixtures that failed, not the entire apptest suite.
    if not _run_apptest_with_retry(config, shell, args, apptest):
      failed.append(apptest)
  else:
    tests = _get_fixtures(config, shell, args, apptest)
    for fixture in tests:
      arguments = args + ["--gtest_filter=%s" % fixture]
      if not _run_apptest_with_retry(config, shell, arguments, apptest):
        failed.append(fixture)
      # Abort when 20 fixtures, or a tenth of the apptest fixtures, have failed.
      # base::TestLauncher does this for timeouts and unknown results.
      if len(failed) >= max(20, len(tests) / 10):
        print "Too many failing fixtures (%d), exiting now." % len(failed)
        return (tests, failed + [apptest + " aborted for excessive failures."])
  return (tests, failed)


# TODO(msw): Determine proper test retry counts; allow configuration.
def _run_apptest_with_retry(config, shell, args, apptest, try_count=3):
  """Runs an apptest, retrying on failure; returns True if any run passed."""
  for try_number in range(try_count):
    if _run_apptest(config, shell, args, apptest):
      return True
    print "Failed %s/%s test run attempts." % (try_number + 1, try_count)
  return False


def _run_apptest(config, shell, args, apptest):
  """Runs an apptest and checks the output for signs of gtest failure."""
  command = _build_command_line(config, args, apptest)
  logging.getLogger().debug("Command: %s" % " ".join(command))
  start_time = time.time()

  try:
    output = _run_test_with_timeout(config, shell, args, apptest)
  except Exception as e:
    _print_exception(command, e)
    return False

  # Fail on output with gtest's "[  FAILED  ]" or a lack of "[  PASSED  ]".
  # The latter condition ensures failure on broken command lines or output.
  # Check output instead of exit codes because mojo shell always exits with 0.
  if output.find("[  FAILED  ]") != -1 or output.find("[  PASSED  ]") == -1:
    _print_exception(command, output)
    return False

  ms = int(round(1000 * (time.time() - start_time)))
  logging.getLogger().debug("Passed with output (%d ms):\n%s" % (ms, output))
  return True


def _get_fixtures(config, shell, args, apptest):
  """Returns an apptest's "Suite.Fixture" list via --gtest_list_tests output."""
  arguments = args + ["--gtest_list_tests"]
  command = _build_command_line(config, arguments, apptest)
  logging.getLogger().debug("Command: %s" % " ".join(command))
  try:
    tests = _run_test_with_timeout(config, shell, arguments, apptest)
    logging.getLogger().debug("Tests for %s:\n%s" % (apptest, tests))
    # Remove log lines from the output and ensure it matches known formatting.
    tests = re.sub("^(\[|WARNING: linker:).*\n", "", tests, flags=re.MULTILINE)
    if not re.match("^(\w*\.\r?\n(  \w*\r?\n)+)+", tests):
      raise Exception("Unrecognized --gtest_list_tests output:\n%s" % tests)
    tests = tests.split("\n")
    test_list = []
    for line in tests:
      if not line:
        continue
      if line[0] != " ":
        suite = line.strip()
        continue
      test_list.append(suite + line.strip())
    return test_list
  except Exception as e:
    _print_exception(command, e)
  return []


def _print_exception(command_line, exception):
  """Print a formatted exception raised from a failed command execution."""
  exit_code = ""
  if hasattr(exception, 'returncode'):
    exit_code = " (exit code %d)" % exception.returncode
  print "\n[  FAILED  ] Command%s: %s" % (exit_code, " ".join(command_line))
  print 72 * "-"
  if hasattr(exception, 'output'):
    print exception.output
  print str(exception)
  print 72 * "-"


def _build_command_line(config, args, apptest):
  """Build the apptest command line. This value isn't executed on Android."""
  paths = Paths(config)
  # On Linux, always run tests with xvfb, but not for --gtest_list_tests.
  use_xvfb = (config.target_os == Config.OS_LINUX and
              not "--gtest_list_tests" in args)
  prefix = [paths.xvfb, paths.build_dir] if use_xvfb else []
  return prefix + [paths.mojo_runner] + args + [apptest]


# TODO(msw): Determine proper test timeout durations (starting small).
def _run_test_with_timeout(config, shell, args, apptest, timeout_in_seconds=10):
  """Run the test with a timeout; return the output or raise an exception."""
  result = Queue.Queue()
  thread = threading.Thread(target=_run_test,
                            args=(config, shell, args, apptest, result))
  thread.start()
  process_or_shell = result.get()
  thread.join(timeout_in_seconds)

  if thread.is_alive():
    try:
      process_or_shell.kill()
    except OSError:
      pass  # The process may have ended after checking |is_alive|.
    return "Error: Test timeout after %s seconds" % timeout_in_seconds

  if not result.empty():
    (output, exception) = result.get()
    if exception:
      raise Exception("%s%s%s" % (output, "\n" if output else "", exception))
    return output

  return "Error: Test exited with no output."


def _run_test(config, shell, args, apptest, result):
  """Run the test and put the output and any exception in |result|."""
  output = ""
  exception = ""
  try:
    if (config.target_os != Config.OS_ANDROID):
      command = _build_command_line(config, args, apptest)
      process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
      result.put(process)
      process.wait()
      if not process.poll():
        output = str(process.stdout.read())
      else:
        exception = "Error: Test exited with code: %d" % process.returncode
    else:
      assert shell
      result.put(shell)
      (r, w) = os.pipe()
      with os.fdopen(r, "r") as rf:
        with os.fdopen(w, "w") as wf:
          arguments = args + [apptest]
          shell.StartActivity('MojoShellActivity', arguments, wf, wf.close)
          output = rf.read()
  except Exception as e:
    output = e.output if hasattr(e, 'output') else ""
    exception = str(e)
  result.put((output, exception))

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fileinput
import glob
import optparse
import os
import textwrap


def AggregateVectorIcons(working_directory, output_cc, output_h):
  """Compiles all .icon files in a directory into two C++ files.

  Args:
      working_directory: The path to the directory that holds the .icon files
          and C++ templates.
      output_cc: The path that should be used to write the .cc file.
      output_h: The path that should be used to write the .h file.
  """

  icon_list = glob.glob(working_directory + "*.icon")

  input_header_template = open(os.path.join(working_directory,
                                            "vector_icons_public.h.template"))
  header_template_contents = input_header_template.readlines()
  input_header_template.close()
  output_header = open(output_h, "w")
  for line in header_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_header.write(line)
    else:
      for icon_path in icon_list:
        (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
        output_header.write("  {},\n".format(icon_name.upper()))
  output_header.close()

  input_cc_template = open(
      os.path.join(working_directory, "vector_icons.cc.template"))
  cc_template_contents = input_cc_template.readlines()
  input_cc_template.close()
  output_cc = open(output_cc, "w")
  for line in cc_template_contents:
    if not "TEMPLATE_PLACEHOLDER" in line:
      output_cc.write(line)
    else:
      for icon_path in icon_list:
        (icon_name, extension) = os.path.splitext(os.path.basename(icon_path))
        icon_file = open(icon_path)
        vector_commands = "".join(icon_file.readlines())
        icon_file.close()
        output_cc.write("ICON_TEMPLATE({}, {})\n".format(icon_name.upper(),
                                                         vector_commands))
  output_cc.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--working_directory",
                    help="The directory to look for template C++ as well as "
                         "icon files.")
  parser.add_option("--output_cc",
                    help="The path to output the CC file to.")
  parser.add_option("--output_h",
                    help="The path to output the header file to.")

  (options, args) = parser.parse_args()

  AggregateVectorIcons(options.working_directory,
                       options.output_cc,
                       options.output_h)


if __name__ == "__main__":
  main()

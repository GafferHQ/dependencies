// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

VISIT_GL_CALL(MapBufferCHROMIUM,
              void*,
              (GLuint target, GLenum access),
              (target, access))
VISIT_GL_CALL(UnmapBufferCHROMIUM, GLboolean, (GLuint target), (target))

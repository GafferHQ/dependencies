// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

library application;

import 'dart:async';

import 'package:mojo/bindings.dart' as bindings;
import 'package:mojo/core.dart' as core;
import 'package:mojom/mojo/application.mojom.dart' as application_mojom;
import 'package:mojom/mojo/service_provider.mojom.dart';
import 'package:mojom/mojo/shell.mojom.dart' as shell_mojom;

part 'src/application.dart';
part 'src/application_connection.dart';

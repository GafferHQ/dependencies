// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A binary wrapper for QuicServer.  It listens forever on --port
// (default 6121) until it's killed or ctrl-cd to death.

#include <iostream>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_server.h"

// The port the quic server will listen on.
int32 FLAGS_port = 6121;

int main(int argc, char *argv[]) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  base::CommandLine::Init(argc, argv);
  base::CommandLine* line = base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  if (line->HasSwitch("h") || line->HasSwitch("help")) {
    const char* help_str =
        "Usage: quic_server [options]\n"
        "\n"
        "Options:\n"
        "-h, --help                  show this help message and exit\n"
        "--port=<port>               specify the port to listen on\n"
        "--quic_in_memory_cache_dir  directory containing response data\n"
        "                            to load\n";
    std::cout << help_str;
    exit(0);
  }

  if (line->HasSwitch("quic_in_memory_cache_dir")) {
    net::tools::QuicInMemoryCache::GetInstance()->InitializeFromDirectory(
        line->GetSwitchValueASCII("quic_in_memory_cache_dir"));
  }

  if (line->HasSwitch("port")) {
    if (!base::StringToInt(line->GetSwitchValueASCII("port"), &FLAGS_port)) {
      LOG(ERROR) << "--port must be an integer\n";
      return 1;
    }
  }

  net::IPAddressNumber ip;
  CHECK(net::ParseIPLiteralToNumber("::", &ip));

  net::QuicConfig config;
  net::tools::QuicServer server(config, net::QuicSupportedVersions());
  server.SetStrikeRegisterNoStartupPeriod();

  int rc = server.Listen(net::IPEndPoint(ip, FLAGS_port));
  if (rc < 0) {
    return 1;
  }

  while (1) {
    server.WaitForEvents();
  }
}

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_net',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
      ],
      'sources': [
        'clients/crn_forwarding_network_client.h',
        'clients/crn_forwarding_network_client.mm',
        'clients/crn_forwarding_network_client_factory.h',
        'clients/crn_forwarding_network_client_factory.mm',
        'clients/crn_network_client_protocol.h',
        'clients/crn_simple_network_client_factory.h',
        'clients/crn_simple_network_client_factory.mm',
        'cookies/cookie_cache.cc',
        'cookies/cookie_cache.h',
        'cookies/cookie_creation_time_manager.h',
        'cookies/cookie_creation_time_manager.mm',
        'cookies/cookie_store_ios.h',
        'cookies/cookie_store_ios.mm',
        'cookies/cookie_store_ios_client.h',
        'cookies/cookie_store_ios_client.mm',
        'cookies/system_cookie_util.h',
        'cookies/system_cookie_util.mm',
        'crn_http_protocol_handler.h',
        'crn_http_protocol_handler.mm',
        'crn_http_protocol_handler_proxy.h',
        'crn_http_protocol_handler_proxy_with_client_thread.h',
        'crn_http_protocol_handler_proxy_with_client_thread.mm',
        'crn_http_url_response.h',
        'crn_http_url_response.mm',
        'empty_nsurlcache.h',
        'empty_nsurlcache.mm',
        'http_protocol_logging.h',
        'http_protocol_logging.mm',
        'http_response_headers_util.h',
        'http_response_headers_util.mm',
        'nsurlrequest_util.h',
        'nsurlrequest_util.mm',
        'protocol_handler_util.h',
        'protocol_handler_util.mm',
        'request_tracker.h',
        'request_tracker.mm',
        'url_scheme_util.h',
        'url_scheme_util.mm',
      ],
    },
  ],
}

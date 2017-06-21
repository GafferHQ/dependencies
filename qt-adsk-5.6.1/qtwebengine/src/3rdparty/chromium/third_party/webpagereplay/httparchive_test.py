#!/usr/bin/env python
# Copyright 2011 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import calendar
import email.utils
import httparchive
import unittest


def create_request(headers):
  return httparchive.ArchivedHttpRequest(
      'GET', 'www.test.com', '/', None, headers)

def create_response(headers):
  return httparchive.ArchivedHttpResponse(
      11, 200, 'OK', headers, '')


class HttpArchiveTest(unittest.TestCase):

  REQUEST_HEADERS = {}
  REQUEST = create_request(REQUEST_HEADERS)

  # Used for if-(un)modified-since checks
  DATE_PAST = 'Wed, 13 Jul 2011 03:58:08 GMT'
  DATE_PRESENT = 'Wed, 20 Jul 2011 04:58:08 GMT'
  DATE_FUTURE = 'Wed, 27 Jul 2011 05:58:08 GMT'
  DATE_INVALID = 'This is an invalid date!!'

  # etag values
  ETAG_VALID = 'etag'
  ETAG_INVALID = 'This is an invalid etag value!!'

  RESPONSE_HEADERS = [('last-modified', DATE_PRESENT), ('etag', ETAG_VALID)]
  RESPONSE = create_response(RESPONSE_HEADERS)

  def setUp(self):
    self.archive = httparchive.HttpArchive()
    self.archive[self.REQUEST] = self.RESPONSE

    # Also add an identical POST request for testing
    request = httparchive.ArchivedHttpRequest(
        'POST', 'www.test.com', '/', None, self.REQUEST_HEADERS)
    self.archive[request] = self.RESPONSE

  def tearDown(self):
    pass

  def test_init(self):
    archive = httparchive.HttpArchive()
    self.assertEqual(len(archive), 0)

  def test__TrimHeaders(self):
    request = httparchive.ArchivedHttpRequest
    header1 = {'accept-encoding': 'gzip,deflate'}
    self.assertEqual(request._TrimHeaders(header1),
                     [(k, v) for k, v in header1.items()])

    header2 = {'referer': 'www.google.com'}
    self.assertEqual(request._TrimHeaders(header2), [])

    header3 = {'referer': 'www.google.com', 'cookie': 'cookie_monster!',
               'hello': 'world'}
    self.assertEqual(request._TrimHeaders(header3), [('hello', 'world')])

    # Tests that spaces and trailing comma get stripped.
    header4 = {'accept-encoding': 'gzip, deflate,, '}
    self.assertEqual(request._TrimHeaders(header4),
                     [('accept-encoding', 'gzip,deflate')])

  def test_matches(self):
    headers = {}
    request1 = httparchive.ArchivedHttpRequest(
        'GET', 'www.test.com', '/index.html?hello=world', None, headers)
    request2 = httparchive.ArchivedHttpRequest(
        'GET', 'www.test.com', '/index.html?foo=bar', None, headers)

    self.assert_(not request1.matches(
        request2.command, request2.host, request2.full_path, use_query=True))
    self.assert_(request1.matches(
        request2.command, request2.host, request2.full_path, use_query=False))

    self.assert_(request1.matches(
        request2.command, request2.host, None, use_query=True))
    self.assert_(request1.matches(
        request2.command, None, request2.full_path, use_query=False))

    empty_request = httparchive.ArchivedHttpRequest(
        None, None, None, None, headers)
    self.assert_(not empty_request.matches(
        request2.command, request2.host, None, use_query=True))
    self.assert_(not empty_request.matches(
        request2.command, None, request2.full_path, use_query=False))

  def setup_find_closest_request(self):
    headers = {}
    request1 = httparchive.ArchivedHttpRequest(
        'GET', 'www.test.com', '/a?hello=world', None, headers)
    request2 = httparchive.ArchivedHttpRequest(
        'GET', 'www.test.com', '/a?foo=bar', None, headers)
    request3 = httparchive.ArchivedHttpRequest(
        'GET', 'www.test.com', '/b?hello=world', None, headers)

    archive = httparchive.HttpArchive()
    # Add requests 2 and 3 and find closest match with request1
    archive[request2] = self.RESPONSE
    archive[request3] = self.RESPONSE

    return archive, request1, request2, request3

  def test_find_closest_request(self):
    archive, request1, request2, request3 = self.setup_find_closest_request()

    # Request 3 is the closest match to request 1
    self.assertEqual(
        request3, archive.find_closest_request(request1, use_path=False))
    # However, if we match strictly on path, request2 is the only match
    self.assertEqual(
        request2, archive.find_closest_request(request1, use_path=True))

  def test_find_closest_request_delete_simple(self):
    archive, request1, request2, request3 = self.setup_find_closest_request()

    del archive[request3]
    self.assertEqual(
        request2, archive.find_closest_request(request1, use_path=False))
    self.assertEqual(
        request2, archive.find_closest_request(request1, use_path=True))

  def test_find_closest_request_delete_complex(self):
    archive, request1, request2, request3 = self.setup_find_closest_request()

    del archive[request2]
    self.assertEqual(
        request3, archive.find_closest_request(request1, use_path=False))
    self.assertEqual(
        None, archive.find_closest_request(request1, use_path=True))

  def test_get_simple(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive

    self.assertEqual(archive.get(request), response)

    false_request_headers = {'foo': 'bar'}
    false_request = create_request(false_request_headers)
    self.assertEqual(archive.get(false_request, default=None), None)

  def test_get_modified_headers(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive
    not_modified_response = httparchive.create_response(304)

    # Fail check and return response again
    request_headers = {'if-modified-since': self.DATE_PAST}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # Succeed check and return 304 Not Modified
    request_headers = {'if-modified-since': self.DATE_FUTURE}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # Succeed check and return 304 Not Modified
    request_headers = {'if-modified-since': self.DATE_PRESENT}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # Invalid date, fail check and return response again
    request_headers = {'if-modified-since': self.DATE_INVALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # fail check since the request is not a GET or HEAD request (as per RFC)
    request_headers = {'if-modified-since': self.DATE_FUTURE}
    request = httparchive.ArchivedHttpRequest(
        'POST', 'www.test.com', '/', None, request_headers)
    self.assertEqual(archive.get(request), response)

  def test_get_unmodified_headers(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive
    not_modified_response = httparchive.create_response(304)

    # Succeed check
    request_headers = {'if-unmodified-since': self.DATE_PAST}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # Fail check
    request_headers = {'if-unmodified-since': self.DATE_FUTURE}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # Succeed check
    request_headers = {'if-unmodified-since': self.DATE_PRESENT}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # Fail check
    request_headers = {'if-unmodified-since': self.DATE_INVALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # Fail check since the request is not a GET or HEAD request (as per RFC)
    request_headers = {'if-modified-since': self.DATE_PAST}
    request = httparchive.ArchivedHttpRequest(
        'POST', 'www.test.com', '/', None, request_headers)
    self.assertEqual(archive.get(request), response)

  def test_get_etags(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive
    not_modified_response = httparchive.create_response(304)
    precondition_failed_response = httparchive.create_response(412)

    # if-match headers
    request_headers = {'if-match': self.ETAG_VALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    request_headers = {'if-match': self.ETAG_INVALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), precondition_failed_response)

    # if-none-match headers
    request_headers = {'if-none-match': self.ETAG_VALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    request_headers = {'if-none-match': self.ETAG_INVALID}
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

  def test_get_multiple_match_headers(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive
    not_modified_response = httparchive.create_response(304)
    precondition_failed_response = httparchive.create_response(412)

    # if-match headers
    # If the request would, without the If-Match header field,
    # result in anything other than a 2xx or 412 status,
    # then the If-Match header MUST be ignored.

    request_headers = {
        'if-match': self.ETAG_VALID,
        'if-modified-since': self.DATE_PAST,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # Invalid etag, precondition failed
    request_headers = {
        'if-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_PAST,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), precondition_failed_response)

    # 304 response; ignore if-match header
    request_headers = {
        'if-match': self.ETAG_VALID,
        'if-modified-since': self.DATE_FUTURE,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # 304 response; ignore if-match header
    request_headers = {
        'if-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_PRESENT,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    # Invalid etag, precondition failed
    request_headers = {
        'if-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_INVALID,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), precondition_failed_response)

  def test_get_multiple_none_match_headers(self):
    request = self.REQUEST
    response = self.RESPONSE
    archive = self.archive
    not_modified_response = httparchive.create_response(304)
    precondition_failed_response = httparchive.create_response(412)

    # if-none-match headers
    # If the request would, without the If-None-Match header field,
    # result in anything other than a 2xx or 304 status,
    # then the If-None-Match header MUST be ignored.

    request_headers = {
        'if-none-match': self.ETAG_VALID,
        'if-modified-since': self.DATE_PAST,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    request_headers = {
        'if-none-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_PAST,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)

    # etag match, precondition failed
    request_headers = {
        'if-none-match': self.ETAG_VALID,
        'if-modified-since': self.DATE_FUTURE,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    request_headers = {
        'if-none-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_PRESENT,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), not_modified_response)

    request_headers = {
        'if-none-match': self.ETAG_INVALID,
        'if-modified-since': self.DATE_INVALID,
    }
    request = create_request(request_headers)
    self.assertEqual(archive.get(request), response)


class ArchivedHttpResponse(unittest.TestCase):
  PAST_DATE_A = 'Tue, 13 Jul 2010 03:47:07 GMT'
  PAST_DATE_B = 'Tue, 13 Jul 2010 02:47:07 GMT'  # PAST_DATE_A -1 hour
  PAST_DATE_C = 'Tue, 13 Jul 2010 04:47:07 GMT'  # PAST_DATE_A +1 hour
  NOW_DATE_A = 'Wed, 20 Jul 2011 04:58:08 GMT'
  NOW_DATE_B = 'Wed, 20 Jul 2011 03:58:08 GMT'  # NOW_DATE_A -1 hour
  NOW_DATE_C = 'Wed, 20 Jul 2011 05:58:08 GMT'  # NOW_DATE_A +1 hour
  NOW_SECONDS = calendar.timegm(email.utils.parsedate(NOW_DATE_A))

  def setUp(self):
    self.response = create_response([('date', self.PAST_DATE_A)])

  def test_update_date_same_date(self):
    self.assertEqual(
        self.response.update_date(self.PAST_DATE_A, now=self.NOW_SECONDS),
        self.NOW_DATE_A)

  def test_update_date_before_date(self):
    self.assertEqual(
        self.response.update_date(self.PAST_DATE_B, now=self.NOW_SECONDS),
        self.NOW_DATE_B)

  def test_update_date_after_date(self):
    self.assertEqual(
        self.response.update_date(self.PAST_DATE_C, now=self.NOW_SECONDS),
        self.NOW_DATE_C)

  def test_update_date_bad_date_param(self):
    self.assertEqual(
        self.response.update_date('garbage date', now=self.NOW_SECONDS),
        'garbage date')

  def test_update_date_bad_date_header(self):
    self.response.set_header('date', 'garbage date')
    self.assertEqual(
        self.response.update_date(self.PAST_DATE_B, now=self.NOW_SECONDS),
        self.PAST_DATE_B)


if __name__ == '__main__':
  unittest.main()

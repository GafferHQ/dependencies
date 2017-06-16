// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/sdch_dictionary_fetcher.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/load_flags.h"
#include "net/base/sdch_manager.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_data_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kSampleBufferContext[] = "This is a sample buffer.";
const char kTestDomain[] = "top.domain.test";

class URLRequestSpecifiedResponseJob : public URLRequestSimpleJob {
 public:
  // Called on destruction with load flags used for this request.
  typedef base::Callback<void(int)> DestructionCallback;

  URLRequestSpecifiedResponseJob(
      URLRequest* request,
      NetworkDelegate* network_delegate,
      const HttpResponseInfo& response_info_to_return,
      const DestructionCallback& destruction_callback)
      : URLRequestSimpleJob(request, network_delegate),
        response_info_to_return_(response_info_to_return),
        last_load_flags_seen_(request->load_flags()),
        destruction_callback_(destruction_callback) {
    DCHECK(!destruction_callback.is_null());
  }

  static std::string ExpectedResponseForURL(const GURL& url) {
    return base::StringPrintf("Response for %s\n%s\nEnd Response for %s\n",
                              url.spec().c_str(),
                              kSampleBufferContext,
                              url.spec().c_str());
  }

  // URLRequestJob
  void GetResponseInfo(HttpResponseInfo* info) override {
    *info = response_info_to_return_;
  }

 private:
  ~URLRequestSpecifiedResponseJob() override {
    destruction_callback_.Run(last_load_flags_seen_);
  }

  // URLRequestSimpleJob implementation:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const CompletionCallback& callback) const override {
    GURL url(request_->url());
    *data = ExpectedResponseForURL(url);
    return OK;
  }

  const HttpResponseInfo response_info_to_return_;
  int last_load_flags_seen_;
  const DestructionCallback destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestSpecifiedResponseJob);
};

class SpecifiedResponseJobInterceptor : public URLRequestInterceptor {
 public:
  // A callback to be called whenever a URLRequestSpecifiedResponseJob is
  // created or destroyed.  The first argument will be the change in number of
  // jobs (i.e. +1 for created, -1 for destroyed).
  // The second argument will be undefined if the job is being created,
  // and will contain the load flags passed to the request the
  // job was created for if the job is being destroyed.
  typedef base::Callback<void(int outstanding_job_delta,
                              int destruction_load_flags)> LifecycleCallback;

  // |*info| will be returned from all child URLRequestSpecifiedResponseJobs.
  // Note that: a) this pointer is shared with the caller, and the caller must
  // guarantee that |*info| outlives the SpecifiedResponseJobInterceptor, and
  // b) |*info| is mutable, and changes to should propagate to
  // URLRequestSpecifiedResponseJobs created after any change.
  SpecifiedResponseJobInterceptor(HttpResponseInfo* http_response_info,
                                  const LifecycleCallback& lifecycle_callback)
      : http_response_info_(http_response_info),
        lifecycle_callback_(lifecycle_callback) {
    DCHECK(!lifecycle_callback_.is_null());
  }
  ~SpecifiedResponseJobInterceptor() override {}

  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    lifecycle_callback_.Run(1, 0);

    return new URLRequestSpecifiedResponseJob(
        request, network_delegate, *http_response_info_,
        base::Bind(lifecycle_callback_, -1));
  }

  // The caller must ensure that both |*http_response_info| and the
  // callback remain valid for the lifetime of the
  // SpecifiedResponseJobInterceptor (i.e. until Unregister() is called).
  static void RegisterWithFilter(HttpResponseInfo* http_response_info,
                                 const LifecycleCallback& lifecycle_callback) {
    scoped_ptr<SpecifiedResponseJobInterceptor> interceptor(
        new SpecifiedResponseJobInterceptor(http_response_info,
                                            lifecycle_callback));

    URLRequestFilter::GetInstance()->AddHostnameInterceptor("http", kTestDomain,
                                                            interceptor.Pass());
  }

  static void Unregister() {
    URLRequestFilter::GetInstance()->RemoveHostnameHandler("http", kTestDomain);
  }

 private:
  HttpResponseInfo* http_response_info_;
  LifecycleCallback lifecycle_callback_;
  DISALLOW_COPY_AND_ASSIGN(SpecifiedResponseJobInterceptor);
};

// Local test infrastructure
// * URLRequestSpecifiedResponseJob: A URLRequestJob that returns
//   a different but derivable response for each URL (used for all
//   url requests in this file).  This class is initialized with
//   the HttpResponseInfo to return (if any), as well as a callback
//   that is called when the class is destroyed.  That callback
//   takes as arguemnt the load flags used for the request the
//   job was created for.
// * SpecifiedResponseJobInterceptor: This class is a
//   URLRequestInterceptor that generates the class above.  It is constructed
//   with a pointer to the (mutable) resposne info that should be
//   returned from the URLRequestSpecifiedResponseJob children, as well as
//   a callback that is run when URLRequestSpecifiedResponseJobs are
//   created or destroyed.
// * SdchDictionaryFetcherTest: This class registers the above interceptor,
//   tracks the number of jobs requested and the subset of those
//   that are still outstanding.  It exports an interface to wait until there
//   are no jobs outstanding.  It shares an HttpResponseInfo structure
//   with the SpecifiedResponseJobInterceptor to control the response
//   information returned by the jbos.
// The standard pattern for tests is to schedule a dictionary fetch, wait
// for no jobs outstanding, then test that the fetch results are as expected.

class SdchDictionaryFetcherTest : public ::testing::Test {
 public:
  struct DictionaryAdditions {
    DictionaryAdditions(const std::string& dictionary_text,
                        const GURL& dictionary_url)
        : dictionary_text(dictionary_text), dictionary_url(dictionary_url) {}

    std::string dictionary_text;
    GURL dictionary_url;
  };

  SdchDictionaryFetcherTest()
      : jobs_requested_(0),
        jobs_outstanding_(0),
        last_load_flags_seen_(LOAD_NORMAL),
        context_(new TestURLRequestContext),
        fetcher_(new SdchDictionaryFetcher(context_.get())),
        factory_(this) {
    response_info_to_return_.request_time = base::Time::Now();
    response_info_to_return_.response_time = base::Time::Now();
    SpecifiedResponseJobInterceptor::RegisterWithFilter(
        &response_info_to_return_,
        base::Bind(&SdchDictionaryFetcherTest::OnNumberJobsChanged,
                   factory_.GetWeakPtr()));
  }

  ~SdchDictionaryFetcherTest() override {
    SpecifiedResponseJobInterceptor::Unregister();
  }

  void OnDictionaryFetched(const std::string& dictionary_text,
                           const GURL& dictionary_url,
                           const BoundNetLog& net_log,
                           bool was_from_cache) {
    dictionary_additions_.push_back(
        DictionaryAdditions(dictionary_text, dictionary_url));
  }

  // Return (in |*out|) all dictionary additions since the last time
  // this function was called.
  void GetDictionaryAdditions(std::vector<DictionaryAdditions>* out) {
    out->swap(dictionary_additions_);
    dictionary_additions_.clear();
  }

  SdchDictionaryFetcher* fetcher() { return fetcher_.get(); }

  // May not be called outside the SetUp()/TearDown() interval.
  int jobs_requested() const { return jobs_requested_; }

  GURL PathToGurl(const char* path) const {
    std::string gurl_string("http://");
    gurl_string += kTestDomain;
    gurl_string += "/";
    gurl_string += path;
    return GURL(gurl_string);
  }

  // Block until there are no outstanding URLRequestSpecifiedResponseJobs.
  void WaitForNoJobs() {
    if (jobs_outstanding_ == 0)
      return;

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

  HttpResponseInfo* response_info_to_return() {
    return &response_info_to_return_;
  }

  int last_load_flags_seen() const { return last_load_flags_seen_; }

  const SdchDictionaryFetcher::OnDictionaryFetchedCallback
      GetDefaultCallback() {
    return base::Bind(&SdchDictionaryFetcherTest::OnDictionaryFetched,
                      base::Unretained(this));
  }

 private:
  void OnNumberJobsChanged(int outstanding_jobs_delta, int load_flags) {
    DCHECK_NE(0, outstanding_jobs_delta);
    if (outstanding_jobs_delta > 0)
      jobs_requested_ += outstanding_jobs_delta;
    else
      last_load_flags_seen_ = load_flags;
    jobs_outstanding_ += outstanding_jobs_delta;
    if (jobs_outstanding_ == 0 && run_loop_)
      run_loop_->Quit();
  }

  int jobs_requested_;
  int jobs_outstanding_;

  // Last load flags seen by the interceptor installed in
  // SdchDictionaryFetcherTest(). These are available to test bodies and
  // currently used for ensuring that certain loads are marked only-from-cache.
  int last_load_flags_seen_;

  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<TestURLRequestContext> context_;
  scoped_ptr<SdchDictionaryFetcher> fetcher_;
  std::vector<DictionaryAdditions> dictionary_additions_;

  // The request_time and response_time fields are filled in by the constructor
  // for SdchDictionaryFetcherTest. Tests can fill the other fields of this
  // member in to alter the HttpResponseInfo returned by the fetcher's
  // URLRequestJob.
  HttpResponseInfo response_info_to_return_;

  base::WeakPtrFactory<SdchDictionaryFetcherTest> factory_;

  DISALLOW_COPY_AND_ASSIGN(SdchDictionaryFetcherTest);
};

// Schedule a fetch and make sure it happens.
TEST_F(SdchDictionaryFetcherTest, Basic) {
  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->Schedule(dictionary_url, GetDefaultCallback());
  WaitForNoJobs();

  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
  EXPECT_FALSE(last_load_flags_seen() & LOAD_ONLY_FROM_CACHE);
}

// Multiple fetches of the same URL should result in only one request.
TEST_F(SdchDictionaryFetcherTest, Multiple) {
  GURL dictionary_url(PathToGurl("dictionary"));
  EXPECT_TRUE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));
  EXPECT_FALSE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));
  EXPECT_FALSE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));
  WaitForNoJobs();

  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
}

// A cancel should result in no actual requests being generated.
TEST_F(SdchDictionaryFetcherTest, Cancel) {
  GURL dictionary_url_1(PathToGurl("dictionary_1"));
  GURL dictionary_url_2(PathToGurl("dictionary_2"));
  GURL dictionary_url_3(PathToGurl("dictionary_3"));

  fetcher()->Schedule(dictionary_url_1, GetDefaultCallback());
  fetcher()->Schedule(dictionary_url_2, GetDefaultCallback());
  fetcher()->Schedule(dictionary_url_3, GetDefaultCallback());
  fetcher()->Cancel();
  WaitForNoJobs();

  // Synchronous execution may have resulted in a single job being scheduled.
  EXPECT_GE(1, jobs_requested());
}

// Attempt to confuse the fetcher loop processing by scheduling a
// dictionary addition while another fetch is in process.
TEST_F(SdchDictionaryFetcherTest, LoopRace) {
  GURL dictionary0_url(PathToGurl("dictionary0"));
  GURL dictionary1_url(PathToGurl("dictionary1"));
  fetcher()->Schedule(dictionary0_url, GetDefaultCallback());
  fetcher()->Schedule(dictionary1_url, GetDefaultCallback());
  WaitForNoJobs();

  ASSERT_EQ(2, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(2u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary0_url),
      additions[0].dictionary_text);
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary1_url),
      additions[1].dictionary_text);
}

TEST_F(SdchDictionaryFetcherTest, ScheduleReloadLoadFlags) {
  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->ScheduleReload(dictionary_url, GetDefaultCallback());

  WaitForNoJobs();
  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
  EXPECT_TRUE(last_load_flags_seen() & LOAD_ONLY_FROM_CACHE);
}

TEST_F(SdchDictionaryFetcherTest, ScheduleReloadFresh) {
  std::string raw_headers = "\0";
  response_info_to_return()->headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(raw_headers.data(), raw_headers.size()));
  response_info_to_return()->headers->AddHeader("Cache-Control: max-age=1000");

  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->ScheduleReload(dictionary_url, GetDefaultCallback());

  WaitForNoJobs();
  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
  EXPECT_TRUE(last_load_flags_seen() & LOAD_ONLY_FROM_CACHE);
}

TEST_F(SdchDictionaryFetcherTest, ScheduleReloadStale) {
  response_info_to_return()->headers = new HttpResponseHeaders("");
  response_info_to_return()->headers->AddHeader("Cache-Control: no-cache");

  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->ScheduleReload(dictionary_url, GetDefaultCallback());

  WaitForNoJobs();
  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  EXPECT_EQ(0u, additions.size());
  EXPECT_TRUE(last_load_flags_seen() & LOAD_ONLY_FROM_CACHE);
}

TEST_F(SdchDictionaryFetcherTest, ScheduleReloadThenLoad) {
  GURL dictionary_url(PathToGurl("dictionary"));
  EXPECT_TRUE(fetcher()->ScheduleReload(dictionary_url, GetDefaultCallback()));
  EXPECT_TRUE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));

  WaitForNoJobs();
  EXPECT_EQ(2, jobs_requested());
}

TEST_F(SdchDictionaryFetcherTest, ScheduleLoadThenReload) {
  GURL dictionary_url(PathToGurl("dictionary"));
  EXPECT_TRUE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));
  EXPECT_FALSE(fetcher()->ScheduleReload(dictionary_url, GetDefaultCallback()));

  WaitForNoJobs();
  EXPECT_EQ(1, jobs_requested());
}

TEST_F(SdchDictionaryFetcherTest, CancelAllowsFutureFetches) {
  GURL dictionary_url(PathToGurl("dictionary"));
  EXPECT_TRUE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));
  EXPECT_FALSE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));

  WaitForNoJobs();
  EXPECT_EQ(1, jobs_requested());

  fetcher()->Cancel();
  WaitForNoJobs();
  EXPECT_TRUE(fetcher()->Schedule(dictionary_url, GetDefaultCallback()));

  WaitForNoJobs();
  EXPECT_EQ(2, jobs_requested());
}

}  // namespace

}  // namespace net

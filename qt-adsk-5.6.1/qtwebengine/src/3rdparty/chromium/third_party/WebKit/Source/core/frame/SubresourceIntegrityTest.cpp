// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourcePtr.h"
#include "core/html/HTMLScriptElement.h"
#include "platform/Crypto.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>

namespace blink {

static const char kBasicScript[] = "alert('test');";
static const char kSha256Integrity[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityLenientSyntax[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityWithEmptyOption[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?";
static const char kSha256IntegrityWithOption[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar";
static const char kSha256IntegrityWithOptions[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar?baz=foz";
static const char kSha256IntegrityWithMimeOption[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?ct=application/javascript";
static const char kSha384Integrity[] = "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha512Integrity[] = "sha512-TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1_ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ--ItA==";
static const char kSha384IntegrityLabeledAs256[] = "sha256-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha256AndSha384Integrities[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kBadSha256AndGoodSha384Integrities[] = "sha256-deadbeef sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kGoodSha256AndBadSha384Integrities[] = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= sha384-deadbeef";
static const char kBadSha256AndBadSha384Integrities[] = "sha256-deadbeef sha384-deadbeef";
static const char kUnsupportedHashFunctionIntegrity[] = "sha1-JfLW308qMPKfb4DaHpUBEESwuPc=";

class SubresourceIntegrityTest : public ::testing::Test {
public:
    SubresourceIntegrityTest()
        : secureURL(ParsedURLString, "https://example.test:443")
        , insecureURL(ParsedURLString, "http://example.test:80")
        , secureOrigin(SecurityOrigin::create(secureURL))
        , insecureOrigin(SecurityOrigin::create(insecureURL))
    {
    }

protected:
    virtual void SetUp()
    {
        document = Document::create();
        scriptElement = HTMLScriptElement::create(*document, true);
    }

    void expectAlgorithm(const String& text, HashAlgorithm expectedAlgorithm)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        HashAlgorithm algorithm;

        EXPECT_EQ(SubresourceIntegrity::AlgorithmValid, SubresourceIntegrity::parseAlgorithm(position, end, algorithm));
        EXPECT_EQ(expectedAlgorithm, algorithm);
        EXPECT_EQ(end, position);
    }

    void expectAlgorithmFailure(const String& text, SubresourceIntegrity::AlgorithmParseResult expectedResult)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* begin = characters.data();
        const UChar* end = characters.end();
        HashAlgorithm algorithm;

        EXPECT_EQ(expectedResult, SubresourceIntegrity::parseAlgorithm(position, end, algorithm));
        EXPECT_EQ(begin, position);
    }

    void expectDigest(const String& text, const char* expectedDigest)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        String digest;

        EXPECT_TRUE(SubresourceIntegrity::parseDigest(position, end, digest));
        EXPECT_EQ(expectedDigest, digest);
    }

    void expectDigestFailure(const String& text)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        String digest;

        EXPECT_FALSE(SubresourceIntegrity::parseDigest(position, end, digest));
        EXPECT_TRUE(digest.isEmpty());
    }

    void expectParse(const char* integrityAttribute, const char* expectedDigest, HashAlgorithm expectedAlgorithm)
    {
        Vector<SubresourceIntegrity::IntegrityMetadata> metadataList;

        EXPECT_EQ(SubresourceIntegrity::IntegrityParseValidResult, SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, metadataList, *document));
        EXPECT_EQ(1u, metadataList.size());
        if (metadataList.size() > 0) {
            EXPECT_EQ(expectedDigest, metadataList[0].digest);
            EXPECT_EQ(expectedAlgorithm, metadataList[0].algorithm);
        }
    }

    void expectParseMultipleHashes(const char* integrityAttribute, const SubresourceIntegrity::IntegrityMetadata expectedMetadataArray[], size_t expectedMetadataArraySize)
    {
        Vector<SubresourceIntegrity::IntegrityMetadata> expectedMetadataList;
        expectedMetadataList.append(expectedMetadataArray, expectedMetadataArraySize);
        Vector<SubresourceIntegrity::IntegrityMetadata> metadataList;
        EXPECT_EQ(SubresourceIntegrity::IntegrityParseValidResult, SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, metadataList, *document));
        EXPECT_EQ(expectedMetadataList.size(), metadataList.size());
        if (expectedMetadataList.size() == metadataList.size()) {
            for (size_t i = 0; i < metadataList.size(); i++) {
                EXPECT_EQ(expectedMetadataList[i].digest, metadataList[i].digest);
                EXPECT_EQ(expectedMetadataList[i].algorithm, metadataList[i].algorithm);
            }
        }
    }

    void expectParseFailure(const char* integrityAttribute)
    {
        Vector<SubresourceIntegrity::IntegrityMetadata> metadataList;

        EXPECT_EQ(SubresourceIntegrity::IntegrityParseNoValidResult, SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, metadataList, *document));
    }

    void expectEmptyParseResult(const char* integrityAttribute)
    {
        Vector<SubresourceIntegrity::IntegrityMetadata> metadataList;

        EXPECT_EQ(SubresourceIntegrity::IntegrityParseValidResult, SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, metadataList, *document));
        EXPECT_EQ(0u, metadataList.size());
    }

    enum CorsStatus {
        WithCors,
        NoCors
    };

    void expectIntegrity(const char* integrity, const char* script, const KURL& url, const KURL& requestorUrl, CorsStatus corsStatus = WithCors)
    {
        scriptElement->setAttribute(HTMLNames::integrityAttr, integrity);
        EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, script, url, *createTestResource(url, requestorUrl, corsStatus).get()));
    }

    void expectIntegrityFailure(const char* integrity, const char* script, const KURL& url, const KURL& requestorUrl, CorsStatus corsStatus = WithCors)
    {
        scriptElement->setAttribute(HTMLNames::integrityAttr, integrity);
        EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, script, url, *createTestResource(url, requestorUrl, corsStatus).get()));
    }

    ResourcePtr<Resource> createTestResource(const KURL& url, const KURL& allowOriginUrl, CorsStatus corsStatus)
    {
        OwnPtr<ResourceResponse> response = adoptPtr(new ResourceResponse);
        response->setURL(url);
        response->setHTTPStatusCode(200);
        if (corsStatus == WithCors) {
            response->setHTTPHeaderField("access-control-allow-origin", SecurityOrigin::create(allowOriginUrl)->toAtomicString());
            response->setHTTPHeaderField("access-control-allow-credentials", "true");
        }
        ResourcePtr<Resource> resource = new Resource(ResourceRequest(response->url()), Resource::Raw);
        resource->setResponse(*response);
        return resource;
    }

    KURL secureURL;
    KURL insecureURL;
    RefPtr<SecurityOrigin> secureOrigin;
    RefPtr<SecurityOrigin> insecureOrigin;

    RefPtrWillBePersistent<Document> document;
    RefPtrWillBePersistent<HTMLScriptElement> scriptElement;
};

TEST_F(SubresourceIntegrityTest, Prioritization)
{
    EXPECT_EQ(HashAlgorithmSha256, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha256, HashAlgorithmSha256));
    EXPECT_EQ(HashAlgorithmSha384, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha384, HashAlgorithmSha384));
    EXPECT_EQ(HashAlgorithmSha512, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha512, HashAlgorithmSha512));

    EXPECT_EQ(HashAlgorithmSha384, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha384, HashAlgorithmSha256));
    EXPECT_EQ(HashAlgorithmSha512, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha512, HashAlgorithmSha256));
    EXPECT_EQ(HashAlgorithmSha512, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha512, HashAlgorithmSha384));

    EXPECT_EQ(HashAlgorithmSha384, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha256, HashAlgorithmSha384));
    EXPECT_EQ(HashAlgorithmSha512, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha256, HashAlgorithmSha512));
    EXPECT_EQ(HashAlgorithmSha512, SubresourceIntegrity::getPrioritizedHashFunction(HashAlgorithmSha384, HashAlgorithmSha512));
}

TEST_F(SubresourceIntegrityTest, ParseAlgorithm)
{
    expectAlgorithm("sha256-", HashAlgorithmSha256);
    expectAlgorithm("sha384-", HashAlgorithmSha384);
    expectAlgorithm("sha512-", HashAlgorithmSha512);
    expectAlgorithm("sha-256-", HashAlgorithmSha256);
    expectAlgorithm("sha-384-", HashAlgorithmSha384);
    expectAlgorithm("sha-512-", HashAlgorithmSha512);

    expectAlgorithmFailure("sha1-", SubresourceIntegrity::AlgorithmUnknown);
    expectAlgorithmFailure("sha-1-", SubresourceIntegrity::AlgorithmUnknown);
    expectAlgorithmFailure("foobarsha256-", SubresourceIntegrity::AlgorithmUnknown);
    expectAlgorithmFailure("foobar-", SubresourceIntegrity::AlgorithmUnknown);
    expectAlgorithmFailure("-", SubresourceIntegrity::AlgorithmUnknown);

    expectAlgorithmFailure("sha256", SubresourceIntegrity::AlgorithmUnparsable);
    expectAlgorithmFailure("", SubresourceIntegrity::AlgorithmUnparsable);
}

TEST_F(SubresourceIntegrityTest, ParseDigest)
{
    expectDigest("abcdefg", "abcdefg");
    expectDigest("abcdefg?", "abcdefg");
    expectDigest("ab+de/g", "ab+de/g");
    expectDigest("ab-de_g", "ab+de/g");

    expectDigestFailure("?");
    expectDigestFailure("&&&foobar&&&");
    expectDigestFailure("\x01\x02\x03\x04");
}

//
// End-to-end parsing tests.
//

TEST_F(SubresourceIntegrityTest, Parsing)
{
    expectParseFailure("not_really_a_valid_anything");
    expectParseFailure("sha256-&&&foobar&&&");
    expectParseFailure("sha256-\x01\x02\x03\x04");
    expectParseFailure("sha256-!!! sha256-!!!");

    expectEmptyParseResult("foobar:///sha256-abcdefg");
    expectEmptyParseResult("ni://sha256-abcdefg");
    expectEmptyParseResult("ni:///sha256-abcdefg");
    expectEmptyParseResult("notsha256atall-abcdefg");

    expectParse(
        "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "sha-256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "     sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=     ",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "sha384-XVVXBGoYw6AJOh9J-Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_tA1v5GPr",
        "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        HashAlgorithmSha384);

    expectParse(
        "sha-384-XVVXBGoYw6AJOh9J_Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_tA1v5GPr",
        "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        HashAlgorithmSha384);

    expectParse(
        "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/javascript",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?foo=bar?ct=application/xhtml+xml",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml?foo=bar",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParse(
        "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?baz=foz?ct=application/xhtml+xml?foo=bar",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);

    expectParseMultipleHashes("", 0, 0);
    expectParseMultipleHashes("    ", 0, 0);

    const SubresourceIntegrity::IntegrityMetadata kValidSha384AndSha512[] = {
        {"XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr", HashAlgorithmSha384},
        {"tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==", HashAlgorithmSha512}
    };
    expectParseMultipleHashes(
        "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        kValidSha384AndSha512,
        ARRAY_SIZE(kValidSha384AndSha512));

    const SubresourceIntegrity::IntegrityMetadata kValidSha256AndSha256[] = {
        {"BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", HashAlgorithmSha256},
        {"deadbeef", HashAlgorithmSha256}
    };
    expectParseMultipleHashes(
        "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-deadbeef",
        kValidSha256AndSha256,
        ARRAY_SIZE(kValidSha256AndSha256));

    const SubresourceIntegrity::IntegrityMetadata kValidSha256AndInvalidSha256[] = {
        {"BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", HashAlgorithmSha256}
    };
    expectParseMultipleHashes(
        "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-!!!!",
        kValidSha256AndInvalidSha256,
        ARRAY_SIZE(kValidSha256AndInvalidSha256));

    const SubresourceIntegrity::IntegrityMetadata kInvalidSha256AndValidSha256[] = {
        {"BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", HashAlgorithmSha256}
    };
    expectParseMultipleHashes(
        "sha256-!!! sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        kInvalidSha256AndValidSha256,
        ARRAY_SIZE(kInvalidSha256AndValidSha256));

    expectParse(
        "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
    expectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo:bar",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);
}

TEST_F(SubresourceIntegrityTest, ParsingBase64)
{
    expectParse(
        "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        HashAlgorithmSha384);
}

//
// End-to-end tests of ::CheckSubresourceIntegrity.
//

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInSecureOrigin)
{
    document->updateSecurityOrigin(secureOrigin->isolatedCopy());

    // Verify basic sha256, sha384, and sha512 integrity checks.
    expectIntegrity(kSha256Integrity, kBasicScript, secureURL, secureURL);
    expectIntegrity(kSha256IntegrityLenientSyntax, kBasicScript, secureURL, secureURL);
    expectIntegrity(kSha384Integrity, kBasicScript, secureURL, secureURL);
    expectIntegrity(kSha512Integrity, kBasicScript, secureURL, secureURL);

    // Verify multiple hashes in an attribute.
    expectIntegrity(kSha256AndSha384Integrities, kBasicScript, secureURL, secureURL);
    expectIntegrity(kBadSha256AndGoodSha384Integrities, kBasicScript, secureURL, secureURL);

    // The hash label must match the hash value.
    expectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript, secureURL, secureURL);

    // With multiple values, at least one must match, and it must be the
    // strongest hash algorithm.
    expectIntegrityFailure(kGoodSha256AndBadSha384Integrities, kBasicScript, secureURL, secureURL);
    expectIntegrityFailure(kBadSha256AndBadSha384Integrities, kBasicScript, secureURL, secureURL);

    // Unsupported hash functions should succeed.
    expectIntegrity(kUnsupportedHashFunctionIntegrity, kBasicScript, secureURL, secureURL);

    // All parameters are fine, and because this is not cross origin, CORS is
    // not needed.
    expectIntegrity(kSha256Integrity, kBasicScript, secureURL, secureURL, NoCors);

    // Options should be ignored
    expectIntegrity(kSha256IntegrityWithEmptyOption, kBasicScript, secureURL, secureURL, NoCors);
    expectIntegrity(kSha256IntegrityWithOption, kBasicScript, secureURL, secureURL, NoCors);
    expectIntegrity(kSha256IntegrityWithOptions, kBasicScript, secureURL, secureURL, NoCors);
    expectIntegrity(kSha256IntegrityWithMimeOption, kBasicScript, secureURL, secureURL, NoCors);
}

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInInsecureOrigin)
{
    // The same checks as CheckSubresourceIntegrityInSecureOrigin should pass
    // here, with the expection of the NoCors check at the end.
    document->updateSecurityOrigin(insecureOrigin->isolatedCopy());

    expectIntegrity(kSha256Integrity, kBasicScript, secureURL, insecureURL);
    expectIntegrity(kSha256IntegrityLenientSyntax, kBasicScript, secureURL, insecureURL);
    expectIntegrity(kSha384Integrity, kBasicScript, secureURL, insecureURL);
    expectIntegrity(kSha512Integrity, kBasicScript, secureURL, insecureURL);
    expectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript, secureURL, insecureURL);
    expectIntegrity(kUnsupportedHashFunctionIntegrity, kBasicScript, secureURL, insecureURL);

    expectIntegrity(kSha256AndSha384Integrities, kBasicScript, secureURL, insecureURL);
    expectIntegrity(kBadSha256AndGoodSha384Integrities, kBasicScript, secureURL, insecureURL);

    expectIntegrityFailure(kSha256Integrity, kBasicScript, secureURL, insecureURL, NoCors);
    expectIntegrityFailure(kGoodSha256AndBadSha384Integrities, kBasicScript, secureURL, insecureURL);
}

} // namespace blink

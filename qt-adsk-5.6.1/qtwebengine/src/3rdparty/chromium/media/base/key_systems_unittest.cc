// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sandersd): Refactor to remove recomputed codec arrays, and generally
// shorten and improve coverage.
//   - http://crbug.com/417444
//   - http://crbug.com/457438
// TODO(sandersd): Add tests to cover codec vectors with empty items.
// http://crbug.com/417461

#include <string>
#include <vector>

#include "base/logging.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems.h"
#include "media/base/media_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// These are the (fake) key systems that are registered for these tests.
// kUsesAes uses the AesDecryptor like Clear Key.
// kExternal uses an external CDM, such as Pepper-based or Android platform CDM.
const char kUsesAes[] = "x-org.example.clear";
const char kUsesAesParent[] = "x-org.example";  // Not registered.
const char kUseAesNameForUMA[] = "UseAes";
const char kExternal[] = "x-com.example.test";
const char kExternalParent[] = "x-com.example";
const char kExternalNameForUMA[] = "External";

const char kClearKey[] = "org.w3.clearkey";
const char kPrefixedClearKey[] = "webkit-org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kAudioFoo[] = "audio/foo";
const char kVideoFoo[] = "video/foo";

// Pick some arbitrary bit fields as long as they are not in conflict with the
// real ones.
enum TestCodec {
  TEST_CODEC_FOO_AUDIO = 1 << 10,  // An audio codec for foo container.
  TEST_CODEC_FOO_AUDIO_ALL = TEST_CODEC_FOO_AUDIO,
  TEST_CODEC_FOO_VIDEO = 1 << 11,  // A video codec for foo container.
  TEST_CODEC_FOO_VIDEO_ALL = TEST_CODEC_FOO_VIDEO,
  TEST_CODEC_FOO_ALL = TEST_CODEC_FOO_AUDIO_ALL | TEST_CODEC_FOO_VIDEO_ALL
};

static_assert((TEST_CODEC_FOO_ALL & EME_CODEC_ALL) == EME_CODEC_NONE,
              "test codec masks should only use invalid codec masks");

// Adapt IsSupportedKeySystemWithMediaMimeType() to the new API,
// IsSupportedCodecCombination().
static bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (KeySystems::GetInstance()->GetContentTypeConfigRule(
              key_system, EmeMediaType::VIDEO, mime_type, codecs) !=
          EmeConfigRule::NOT_SUPPORTED);
}

static bool IsSupportedKeySystemWithAudioMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return (KeySystems::GetInstance()->GetContentTypeConfigRule(
              key_system, EmeMediaType::AUDIO, mime_type, codecs) !=
          EmeConfigRule::NOT_SUPPORTED);
}

// Adds test container and codec masks.
// This function must be called after SetMediaClient() if a MediaClient will be
// provided.
// More details: AddXxxMask() will create KeySystems if it hasn't been created.
// During KeySystems's construction GetMediaClient() will be used to add key
// systems. In test code, the MediaClient is set by SetMediaClient().
// Therefore, SetMediaClient() must be called before this function to make sure
// MediaClient in effect when constructing KeySystems.
static void AddContainerAndCodecMasksForTest() {
  // Since KeySystems is a singleton. Make sure we only add test container and
  // codec masks once per process.
  static bool is_test_masks_added = false;

  if (is_test_masks_added)
    return;

  AddContainerMask("audio/foo", TEST_CODEC_FOO_AUDIO_ALL);
  AddContainerMask("video/foo", TEST_CODEC_FOO_ALL);
  AddCodecMask(EmeMediaType::AUDIO, "fooaudio", TEST_CODEC_FOO_AUDIO);
  AddCodecMask(EmeMediaType::VIDEO, "foovideo", TEST_CODEC_FOO_VIDEO);

  is_test_masks_added = true;
}

class TestMediaClient : public MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() override;

  // MediaClient implementation.
  void AddKeySystemsInfoForUMA(
      std::vector<KeySystemInfoForUMA>* key_systems_info_for_uma) final;
  bool IsKeySystemsUpdateNeeded() final;
  void AddSupportedKeySystems(
      std::vector<KeySystemInfo>* key_systems_info) override;
  void RecordRapporURL(const std::string& metric, const GURL& url) final;

  // Helper function to test the case where IsKeySystemsUpdateNeeded() is true
  // after AddSupportedKeySystems() is called.
  void SetKeySystemsUpdateNeeded();

  // Helper function to disable "kExternal" key system support so that we can
  // test the key system update case.
  void DisableExternalKeySystemSupport();

 protected:
  void AddUsesAesKeySystem(const std::string& name,
                           std::vector<KeySystemInfo>* key_systems_info);
  void AddExternalKeySystem(
      std::vector<KeySystemInfo>* key_systems_info);

 private:
  bool is_update_needed_;
  bool supports_external_key_system_;
};

TestMediaClient::TestMediaClient()
    : is_update_needed_(true), supports_external_key_system_(true) {
}

TestMediaClient::~TestMediaClient() {
}

void TestMediaClient::AddKeySystemsInfoForUMA(
    std::vector<KeySystemInfoForUMA>* key_systems_info_for_uma) {
  key_systems_info_for_uma->push_back(
      media::KeySystemInfoForUMA(kUsesAes, kUseAesNameForUMA, false));
  key_systems_info_for_uma->push_back(
      media::KeySystemInfoForUMA(kExternal, kExternalNameForUMA, true));
}

bool TestMediaClient::IsKeySystemsUpdateNeeded() {
  return is_update_needed_;
}

void TestMediaClient::AddSupportedKeySystems(
    std::vector<KeySystemInfo>* key_systems) {
  DCHECK(is_update_needed_);

  AddUsesAesKeySystem(kUsesAes, key_systems);

  if (supports_external_key_system_)
    AddExternalKeySystem(key_systems);

  is_update_needed_ = false;
}

void TestMediaClient::RecordRapporURL(const std::string& /* metric */,
                                      const GURL& /* url */) {
  NOTIMPLEMENTED();
}

void TestMediaClient::SetKeySystemsUpdateNeeded() {
  is_update_needed_ = true;
}

void TestMediaClient::DisableExternalKeySystemSupport() {
  supports_external_key_system_ = false;
}

void TestMediaClient::AddUsesAesKeySystem(
    const std::string& name,
    std::vector<KeySystemInfo>* key_systems) {
  KeySystemInfo system;
  system.key_system = name;
  system.supported_codecs = EME_CODEC_WEBM_ALL;
  system.supported_codecs |= TEST_CODEC_FOO_ALL;
  system.supported_init_data_types = kInitDataTypeMaskWebM;
  system.max_audio_robustness = EmeRobustness::EMPTY;
  system.max_video_robustness = EmeRobustness::EMPTY;
  system.persistent_license_support = EmeSessionTypeSupport::NOT_SUPPORTED;
  system.persistent_release_message_support =
      EmeSessionTypeSupport::NOT_SUPPORTED;
  system.persistent_state_support = EmeFeatureSupport::NOT_SUPPORTED;
  system.distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
  system.use_aes_decryptor = true;
  key_systems->push_back(system);
}

void TestMediaClient::AddExternalKeySystem(
    std::vector<KeySystemInfo>* key_systems) {
  KeySystemInfo ext;
  ext.key_system = kExternal;
  ext.supported_codecs = EME_CODEC_WEBM_ALL;
  ext.supported_codecs |= TEST_CODEC_FOO_ALL;
  ext.supported_init_data_types = kInitDataTypeMaskWebM;
  ext.max_audio_robustness = EmeRobustness::EMPTY;
  ext.max_video_robustness = EmeRobustness::EMPTY;
  ext.persistent_license_support = EmeSessionTypeSupport::SUPPORTED;
  ext.persistent_release_message_support = EmeSessionTypeSupport::NOT_SUPPORTED;
  ext.persistent_state_support = EmeFeatureSupport::ALWAYS_ENABLED;
  ext.distinctive_identifier_support = EmeFeatureSupport::ALWAYS_ENABLED;
  ext.parent_key_system = kExternalParent;
#if defined(ENABLE_PEPPER_CDMS)
  ext.pepper_type = "application/x-ppapi-external-cdm";
#endif  // defined(ENABLE_PEPPER_CDMS)
  key_systems->push_back(ext);
}

class PotentiallySupportedNamesTestMediaClient : public TestMediaClient {
  void AddSupportedKeySystems(
      std::vector<KeySystemInfo>* key_systems_info) final;
};

void PotentiallySupportedNamesTestMediaClient::AddSupportedKeySystems(
    std::vector<KeySystemInfo>* key_systems) {
  // org.w3.clearkey is automatically registered.
  AddUsesAesKeySystem("com.widevine.alpha", key_systems);
  AddUsesAesKeySystem("org.chromium.externalclearkey", key_systems);
  AddUsesAesKeySystem("org.chromium.externalclearkey.something", key_systems);
  AddUsesAesKeySystem("com.chromecast.something", key_systems);
  AddUsesAesKeySystem("x-something", key_systems);
}

class KeySystemsPotentiallySupportedNamesTest : public testing::Test {
 protected:
  KeySystemsPotentiallySupportedNamesTest() {
    SetMediaClient(&test_media_client_);
  }

  ~KeySystemsPotentiallySupportedNamesTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    SetMediaClient(nullptr);
  }

 private:
  PotentiallySupportedNamesTestMediaClient test_media_client_;
};

class KeySystemsTest : public testing::Test {
 protected:
  KeySystemsTest() {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vp9_codec_.push_back("vp9");

    vp90_codec_.push_back("vp9.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    vp9_and_vorbis_codecs_.push_back("vp9");
    vp9_and_vorbis_codecs_.push_back("vorbis");

    foovideo_codec_.push_back("foovideo");

    foovideo_extended_codec_.push_back("foovideo.4D400C");

    foovideo_dot_codec_.push_back("foovideo.");

    fooaudio_codec_.push_back("fooaudio");

    foovideo_and_fooaudio_codecs_.push_back("foovideo");
    foovideo_and_fooaudio_codecs_.push_back("fooaudio");

    unknown_codec_.push_back("unknown");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("foovideo");

    SetMediaClient(&test_media_client_);
  }

  void SetUp() override {
    AddContainerAndCodecMasksForTest();
  }

  ~KeySystemsTest() override {
    // Clear the use of |test_media_client_|, which was set in SetUp().
    SetMediaClient(nullptr);
  }

  void UpdateClientKeySystems() {
    test_media_client_.SetKeySystemsUpdateNeeded();
    test_media_client_.DisableExternalKeySystemSupport();
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }

  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vp9_codec() const { return vp9_codec_; }
  const CodecVector& vp90_codec() const { return vp90_codec_; }

  const CodecVector& vorbis_codec() const { return vorbis_codec_; }

  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }
  const CodecVector& vp9_and_vorbis_codecs() const {
    return vp9_and_vorbis_codecs_;
  }

  const CodecVector& foovideo_codec() const { return foovideo_codec_; }
  const CodecVector& foovideo_extended_codec() const {
    return foovideo_extended_codec_;
  }
  const CodecVector& foovideo_dot_codec() const { return foovideo_dot_codec_; }
  const CodecVector& fooaudio_codec() const { return fooaudio_codec_; }
  const CodecVector& foovideo_and_fooaudio_codecs() const {
    return foovideo_and_fooaudio_codecs_;
  }

  const CodecVector& unknown_codec() const { return unknown_codec_; }

  const CodecVector& mixed_codecs() const { return mixed_codecs_; }

 private:
  const CodecVector no_codecs_;
  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vp9_codec_;
  CodecVector vp90_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;
  CodecVector vp9_and_vorbis_codecs_;

  CodecVector foovideo_codec_;
  CodecVector foovideo_extended_codec_;
  CodecVector foovideo_dot_codec_;
  CodecVector fooaudio_codec_;
  CodecVector foovideo_and_fooaudio_codecs_;

  CodecVector unknown_codec_;

  CodecVector mixed_codecs_;

  TestMediaClient test_media_client_;
};

// TODO(ddorwin): Consider moving GetPepperType() calls out to their own test.

TEST_F(KeySystemsTest, EmptyKeySystem) {
  EXPECT_FALSE(IsSupportedKeySystem(std::string()));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(std::string()));
}

// Clear Key is the only key system registered in content.
TEST_F(KeySystemsTest, ClearKey) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kClearKey));

  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));

  // Prefixed Clear Key is not supported internally.
  EXPECT_FALSE(IsSupportedKeySystem(kPrefixedClearKey));
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kPrefixedClearKey));
}

TEST_F(KeySystemsTest, ClearKeyWithInitDataType) {
  EXPECT_TRUE(IsSupportedKeySystem(kClearKey));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::WEBM));
  EXPECT_TRUE(
      IsSupportedKeySystemWithInitDataType(kClearKey, EmeInitDataType::KEYIDS));

  // All other InitDataTypes are not supported.
  EXPECT_FALSE(IsSupportedKeySystemWithInitDataType(kClearKey,
                                                    EmeInitDataType::UNKNOWN));
}

// The key system is not registered and therefore is unrecognized.
TEST_F(KeySystemsTest, Basic_UnrecognizedKeySystem) {
  static const char* const kUnrecognized = "x-org.example.unrecognized";

  EXPECT_FALSE(IsSupportedKeySystem(kUnrecognized));

  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUnrecognized));
  EXPECT_FALSE(CanUseAesDecryptor(kUnrecognized));

#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(
      type = GetPepperType(kUnrecognized),
      "x-org.example.unrecognized is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest, Basic_UsesAesDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));

  // No UMA value for this test key system.
  EXPECT_EQ("UseAes", GetKeySystemNameForUMA(kUsesAes));

  EXPECT_TRUE(CanUseAesDecryptor(kUsesAes));
#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kUsesAes),
                     "x-org.example.clear is not Pepper-based");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kUsesAes));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, mixed_codecs(), kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vorbis_codec(), kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kUsesAes));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kUsesAes));
}

// No parent is registered for UsesAes.
TEST_F(KeySystemsTest, Parent_NoParentRegistered) {
  EXPECT_FALSE(IsSupportedKeySystem(kUsesAesParent));

  // The parent is not supported for most things.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kUsesAesParent));
  EXPECT_FALSE(CanUseAesDecryptor(kUsesAesParent));

#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kUsesAesParent),
                     "x-org.example is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest, IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.ClEaR"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystem("com"));

  // Extra period.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear."));
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example."));

  // Incomplete.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clea"));

  // Extra character.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clearz"));

  // There are no child key systems for UsesAes.
  EXPECT_FALSE(IsSupportedKeySystem("x-org.example.clear.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_NoType) {
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kUsesAesParent));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(std::string(), no_codecs(),
                                                     "x-org.example.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "x-org.example.clear.foo"));
}

// Tests the second registered container type.
// TODO(ddorwin): Combined with TypesContainer1 in a future CL.
TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kUsesAes));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kUsesAes));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kUsesAes));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, mixed_codecs(), kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, fooaudio_codec(), kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, vorbis_codec(), kUsesAes));
}

//
// Non-AesDecryptor-based key system.
//

TEST_F(KeySystemsTest, Basic_ExternalDecryptor) {
  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));

  EXPECT_FALSE(CanUseAesDecryptor(kExternal));
#if defined(ENABLE_PEPPER_CDMS)
  EXPECT_EQ("application/x-ppapi-external-cdm", GetPepperType(kExternal));
#endif  // defined(ENABLE_PEPPER_CDMS)
}

TEST_F(KeySystemsTest, Parent_ParentRegistered) {
  // Unprefixed has no parent key system support.
  EXPECT_FALSE(IsSupportedKeySystem(kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternalParent));

  // The parent is not supported for most things.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kExternalParent));
  EXPECT_FALSE(CanUseAesDecryptor(kExternalParent));

#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kExternalParent),
                     "x-com.example is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kExternal));

  // Valid video types - parent key system.
  // Prefixed has parent key system support.
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kExternalParent));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, mixed_codecs(), kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vorbis_codec(), kExternal));

  // Valid audio types - parent key system.
  // Prefixed has parent key system support.
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, no_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vorbis_codec(), kExternalParent));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp9_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kExternal));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioWebM, fooaudio_codec(), kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kExternal));

  // Audio in a video container.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternal));

  // Valid video types - parent key system.
  // Prefixed has parent key system support.
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternalParent));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kExternal));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kExternal));

  // Non-container2 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, mixed_codecs(), kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, fooaudio_codec(), kExternal));

  // Valid audio types - parent key system.
  // Prefixed has parent key system support.
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, no_codecs(), kExternalParent));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, fooaudio_codec(), kExternalParent));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kExternal));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithAudioMimeType(
      kAudioFoo, vorbis_codec(), kExternal));
}

TEST_F(KeySystemsTest, KeySystemNameForUMA) {
  EXPECT_EQ("ClearKey", GetKeySystemNameForUMA(kClearKey));
  // Prefixed is not supported internally.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kPrefixedClearKey));

  // External Clear Key never has a UMA name.
  EXPECT_EQ("Unknown", GetKeySystemNameForUMA(kExternalClearKey));
}

TEST_F(KeySystemsTest, KeySystemsUpdate) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));

  UpdateClientKeySystems();

  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystem(kExternal));
}

TEST_F(KeySystemsTest, PrefixedKeySystemsUpdate) {
  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystem(kExternal));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));

  UpdateClientKeySystems();

  EXPECT_TRUE(IsSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystem(kExternal));
  EXPECT_FALSE(PrefixedIsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));
}

TEST_F(KeySystemsPotentiallySupportedNamesTest, PotentiallySupportedNames) {
  EXPECT_FALSE(IsSupportedKeySystem("org.w3"));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3."));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3.clearke"));
  EXPECT_TRUE(IsSupportedKeySystem("org.w3.clearkey"));
  EXPECT_FALSE(IsSupportedKeySystem("org.w3.clearkeys"));

  EXPECT_FALSE(IsSupportedKeySystem("com.widevine"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine."));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alph"));
  EXPECT_TRUE(IsSupportedKeySystem("com.widevine.alpha"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.beta"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alphabeta"));
  EXPECT_FALSE(IsSupportedKeySystem("com.widevine.alpha.beta"));

  EXPECT_FALSE(IsSupportedKeySystem("org.chromium"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium."));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearke"));
  EXPECT_TRUE(IsSupportedKeySystem("org.chromium.externalclearkey"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkeys"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkey."));
  EXPECT_TRUE(IsSupportedKeySystem("org.chromium.externalclearkey.something"));
  EXPECT_FALSE(
      IsSupportedKeySystem("org.chromium.externalclearkey.something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.externalclearkey.other"));
  EXPECT_FALSE(IsSupportedKeySystem("org.chromium.other"));

  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast."));
  EXPECT_TRUE(IsSupportedKeySystem("com.chromecast.something"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast.something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("com.chromecast.other"));

  EXPECT_FALSE(IsSupportedKeySystem("x-"));
  EXPECT_TRUE(IsSupportedKeySystem("x-something"));
  EXPECT_FALSE(IsSupportedKeySystem("x-something.else"));
  EXPECT_FALSE(IsSupportedKeySystem("x-other"));
}

}  // namespace media

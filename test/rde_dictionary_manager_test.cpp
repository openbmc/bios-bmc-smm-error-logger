#include "rde/rde_dictionary_manager.hpp"

#include <cstring>
#include <memory>
#include <optional>
#include <span>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace biosBmcSmmErrorLogger
{
namespace rde
{

constexpr std::array<uint8_t, 132> dummyDictionary1{
    {0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x17, 0x01, 0x00, 0x00, 0x0,  0x0,
     0xc,  0x0,  0x0,  0xf0, 0xf0, 0xf1, 0x17, 0x1,  0x0,  0x0,  0x0,  0x0,
     0x0,  0x16, 0x0,  0x5,  0x0,  0xc,  0x84, 0x0,  0x14, 0x0,  0x0,  0x48,
     0x0,  0x1,  0x0,  0x13, 0x90, 0x0,  0x56, 0x1,  0x0,  0x0,  0x0,  0x0,
     0x0,  0x3,  0xa3, 0x0,  0x74, 0x2,  0x0,  0x0,  0x0,  0x0,  0x0,  0x16,
     0xa6, 0x0,  0x34, 0x3,  0x0,  0x0,  0x0,  0x0,  0x0,  0x16, 0xbc, 0x0,
     0x64, 0x4,  0x0,  0x0,  0x0,  0x0,  0x0,  0x13, 0xd2, 0x0,  0x0,  0x0,
     0x0,  0x52, 0x0,  0x2,  0x0,  0x0,  0x0,  0x0,  0x74, 0x0,  0x0,  0x0,
     0x0,  0x0,  0x0,  0xf,  0xe5, 0x0,  0x46, 0x1,  0x0,  0x66, 0x0,  0x3,
     0x0,  0xb,  0xf4, 0x0,  0x50, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x9,
     0xff, 0x0,  0x50, 0x1,  0x0,  0x0,  0x0,  0x0,  0x0,  0x7,  0x8,  0x1}};

constexpr std::array<uint8_t, 14> dummyDictionary2{
    {0x65, 0x0, 0x43, 0x68, 0x69, 0x6c, 0x64, 0x41, 0x72, 0x72, 0x61, 0x79,
     0x50, 0x72}};

class RdeDictionaryManagerTest : public ::testing::Test
{
  protected:
    uint32_t resourceId = 1;
    DictionaryManager dm;
};

TEST_F(RdeDictionaryManagerTest, DictionarySetTest)
{
    // Add a single dictionary.
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    EXPECT_THAT(dm.getDictionaryCount(), 0);

    // Mark the dictionary as a valid dictionary.
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    // Request the dictionary back and verify the data.
    auto dataOrErr = dm.getDictionary(resourceId);
    EXPECT_TRUE(dataOrErr);
    EXPECT_THAT((*dataOrErr).size_bytes(), dummyDictionary1.size());
    EXPECT_THAT(memcmp((*dataOrErr).data(), dummyDictionary1.data(),
                       dummyDictionary1.size()),
                0);
}

TEST_F(RdeDictionaryManagerTest, DictionaryNotSetTest)
{
    // Add a single dictionary.
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    EXPECT_THAT(dm.getDictionaryCount(), 0);
    // Request the dictionary back without marking it complete. Request should
    // fail.
    EXPECT_FALSE(dm.getDictionary(resourceId));
}

TEST_F(RdeDictionaryManagerTest, DictionaryMultiSetTest)
{
    // Creates a dictionary
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    EXPECT_THAT(dm.getDictionaryCount(), 0);
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    // Creates a second dictionary.
    dm.startDictionaryEntry(annotationResourceId, std::span(dummyDictionary2));
    dm.markDataComplete(annotationResourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 2);

    auto data1OrErr = dm.getDictionary(resourceId);
    EXPECT_TRUE(data1OrErr);
    EXPECT_THAT((*data1OrErr).size_bytes(), dummyDictionary1.size());
    EXPECT_THAT(memcmp((*data1OrErr).data(), dummyDictionary1.data(),
                       dummyDictionary1.size()),
                0);

    auto data2OrErr = dm.getDictionary(annotationResourceId);
    EXPECT_TRUE(data2OrErr);
    EXPECT_THAT((*data2OrErr).size_bytes(), dummyDictionary2.size());
    EXPECT_THAT(memcmp((*data2OrErr).data(), dummyDictionary2.data(),
                       dummyDictionary2.size()),
                0);
}

TEST_F(RdeDictionaryManagerTest, DictionaryOverwriteTest)
{
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary2));

    // Recreate another one on the same location.
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    EXPECT_THAT(dm.getDictionaryCount(), 0);
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    auto dataOrErr = dm.getDictionary(resourceId);
    EXPECT_TRUE(dataOrErr);
    EXPECT_THAT((*dataOrErr).size_bytes(), dummyDictionary1.size());
    EXPECT_THAT(memcmp((*dataOrErr).data(), dummyDictionary1.data(),
                       dummyDictionary1.size()),
                0);

    // Recreate another one on the same location.
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary2));
    EXPECT_THAT(dm.getDictionaryCount(), 0);
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    auto newDataOrErr = dm.getDictionary(resourceId);
    EXPECT_TRUE(newDataOrErr);
    EXPECT_THAT((*newDataOrErr).size_bytes(), dummyDictionary2.size());
    EXPECT_THAT(memcmp((*newDataOrErr).data(), dummyDictionary2.data(),
                       dummyDictionary2.size()),
                0);
}

TEST_F(RdeDictionaryManagerTest, DictionaryAppendDataTest)
{
    // Creates a dictionary
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    EXPECT_THAT(dm.getDictionaryCount(), 0);

    // Lets copy the dictionary in two sizes.
    const uint32_t copySize1 = dummyDictionary2.size() / 2;
    const uint32_t copySize2 = dummyDictionary2.size() - copySize1;

    // Overwrite on the same location as before.
    dm.startDictionaryEntry(resourceId,
                            std::span(dummyDictionary2.data(), copySize1));
    dm.addDictionaryData(
        resourceId, std::span(dummyDictionary2.data() + copySize1, copySize2));
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    auto dataOrErr = dm.getDictionary(resourceId);
    EXPECT_TRUE(dataOrErr);
    EXPECT_THAT((*dataOrErr).size_bytes(), dummyDictionary2.size());
    EXPECT_THAT(memcmp((*dataOrErr).data(), dummyDictionary2.data(),
                       dummyDictionary2.size()),
                0);
}

TEST_F(RdeDictionaryManagerTest, DictionaryOverrideWithAddDataTest)
{
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    dm.addDictionaryData(resourceId, std::span(dummyDictionary2));
    EXPECT_THAT(dm.getDictionaryCount(), 0);
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);
}

TEST_F(RdeDictionaryManagerTest, DictionaryInvalidateTest)
{
    dm.startDictionaryEntry(resourceId, std::span(dummyDictionary1));
    dm.markDataComplete(resourceId);
    EXPECT_THAT(dm.getDictionaryCount(), 1);

    dm.invalidateDictionaries();
    EXPECT_THAT(dm.getDictionaryCount(), 0);
}

} // namespace rde
} // namespace biosBmcSmmErrorLogger

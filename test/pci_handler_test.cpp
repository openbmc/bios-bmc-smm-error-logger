#include "internal_sys_mock.hpp"
#include "pci_handler.hpp"

#include <sys/mman.h>

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace biosBmcSmmErrorLogger
{
namespace
{

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::Return;

class PciHandlerTest : public ::testing::Test
{
  protected:
    PciHandlerTest() :
        testMapped({0, 11, 22, 33, 44, 55, 66, 77}),
        pciDataHandler(std::make_unique<PciDataHandler>(
            testRegionAddress, testRegionSize, &sys_mock))
    {
        testMappedPtr = testMapped.data();
    }

    static constexpr uint32_t testRegionAddress = 0xF0848000;
    // Smaller region size for easier boundary testing
    static constexpr size_t testRegionSize = 8;
    static constexpr int testFd = 22;
    std::vector<uint8_t> testMapped;
    void* testMappedPtr;

    internal::InternalSysMock sys_mock;
    std::unique_ptr<PciDataHandler> pciDataHandler;
};

TEST_F(PciHandlerTest, OpenFromOpenFails)
{
    EXPECT_CALL(sys_mock, open(_, _)).WillOnce(Return(-1));
    EXPECT_FALSE(pciDataHandler->open());
}

TEST_F(PciHandlerTest, MmapFromOpenFails)
{
    EXPECT_CALL(sys_mock, open(_, _)).WillOnce(Return(testFd));
    EXPECT_CALL(sys_mock, mmap(_, _, _, _, _, _)).WillOnce(Return(MAP_FAILED));
    EXPECT_CALL(sys_mock, close(testFd)).WillOnce(Return(true));
    EXPECT_FALSE(pciDataHandler->open());
}

TEST_F(PciHandlerTest, OpenPasses)
{
    InSequence s;
    EXPECT_CALL(sys_mock, open(_, _)).WillOnce(Return(testFd));
    EXPECT_CALL(sys_mock, mmap(_, _, _, _, _, _))
        .WillOnce(Return(testMappedPtr));
    EXPECT_CALL(sys_mock, close(testFd)).Times(0);
    EXPECT_TRUE(pciDataHandler->open());
}

TEST_F(PciHandlerTest, BoundaryChecksReadFail)
{
    std::vector<uint8_t> emptyVector;
    // Zero size
    EXPECT_THAT(pciDataHandler->read(0, 0), ElementsAreArray(emptyVector));

    const int lenghtTooBig = testRegionSize + 1;
    EXPECT_THAT(pciDataHandler->read(0, lenghtTooBig),
                ElementsAreArray(emptyVector));

    const int offsetTooBig = testRegionSize + 1;
    EXPECT_THAT(pciDataHandler->read(offsetTooBig, 0),
                ElementsAreArray(emptyVector));
}

TEST_F(PciHandlerTest, BoundaryChecksWriteFail)
{
    std::vector<uint8_t> emptyVector;
    // Zero size
    EXPECT_EQ(pciDataHandler->write(0, emptyVector), 0);

    std::vector<uint8_t> vectorTooBig(testRegionSize + 1);
    EXPECT_EQ(pciDataHandler->write(0, vectorTooBig), 0);

    const int offsetTooBig = testRegionSize + 1;
    EXPECT_EQ(pciDataHandler->write(offsetTooBig, vectorTooBig), 0);
}

class PciHandlerOpenedTest : public PciHandlerTest
{
  protected:
    PciHandlerOpenedTest()
    {
        InSequence s;
        EXPECT_CALL(sys_mock, open(_, _)).WillOnce(Return(testFd));
        EXPECT_CALL(sys_mock, mmap(_, _, _, _, _, _))
            .WillOnce(Return(testMappedPtr));
        EXPECT_CALL(sys_mock, close(testFd)).Times(0);
        EXPECT_TRUE(pciDataHandler->open());
    }
};

TEST_F(PciHandlerOpenedTest, ReadPasses)
{
    // Normal read from 0
    uint32_t testOffset = 0;
    uint32_t testSize = 2;
    auto testMappedItr = testMapped.begin() + testOffset;
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(testMappedItr, testMappedItr + testSize));

    // Read to buffer boundary from non 0 offset
    testOffset = 3;
    testSize = testRegionSize - testOffset;
    testMappedItr = testMapped.begin() + testOffset;
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(testMappedItr, testMappedItr + testSize));

    // Read over buffer boundary (which will read until the end)
    testOffset = 2;
    testSize = testRegionSize - testOffset + 1;
    testMappedItr = testMapped.begin() + testOffset;
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(testMappedItr,
                                 testMappedItr + testRegionSize - testOffset));
}

TEST_F(PciHandlerOpenedTest, WritePasses)
{
    std::vector<uint8_t> expectedMapped{0, 11, 22, 33, 44, 55, 66, 77};

    // Normal write from 0
    uint32_t testOffset = 0;
    std::vector<uint8_t> writeVector{99, 88};
    expectedMapped = {99, 88, 22, 33, 44, 55, 66, 77};

    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              writeVector.size());
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));

    // Write to buffer boundary from non 0 offset
    testOffset = 4;
    writeVector = {55, 44, 33, 22};
    expectedMapped = {99, 88, 22, 33, 55, 44, 33, 22};
    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              writeVector.size());
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));

    // Read over buffer boundary (which will read until the end)
    testOffset = 7;
    writeVector = {12, 23, 45};
    expectedMapped = {99, 88, 22, 33, 55, 44, 33, 12};
    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              testRegionSize - testOffset);
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));
}

TEST_F(PciHandlerOpenedTest, VerifyClose)
{
    EXPECT_CALL(sys_mock, munmap(testMappedPtr, testRegionSize))
        .WillOnce(Return(true));
    EXPECT_CALL(sys_mock, close(testFd)).WillOnce(Return(true));
    EXPECT_NO_THROW(pciDataHandler->close());
}

} // namespace
} // namespace biosBmcSmmErrorLogger

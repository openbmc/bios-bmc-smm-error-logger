#include "pci_handler.hpp"

#include <stdplus/fd/gmock.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/fd/mmap.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bios_bmc_smm_error_logger
{
namespace
{

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;

class PciHandlerTest : public ::testing::Test
{
  protected:
    PciHandlerTest() :
        testMapped({std::byte(0), std::byte(11), std::byte(22), std::byte(33),
                    std::byte(44), std::byte(55), std::byte(66),
                    std::byte(77)}),
        fdMock(std::make_unique<stdplus::fd::FdMock>()), fdMockPtr(fdMock.get())
    {
        // Verify that the constructor is called as expected
        EXPECT_CALL(*fdMockPtr, mmap(_, _, _, _))
            .WillOnce(Return(std::span<std::byte>(testMapped)));
        pciDataHandler = std::make_unique<PciDataHandler>(
            testRegionAddress, testRegionSize, std::move(fdMock));
    }
    ~PciHandlerTest() override
    {
        // Verify that the destructor is called as expected
        EXPECT_CALL(*fdMockPtr, munmap(_)).Times(1);
    }
    static constexpr uint32_t testRegionAddress = 0xF0848000;
    // Smaller region size for easier boundary testing
    static constexpr size_t testRegionSize = 8;
    std::vector<std::byte> testMapped;
    std::unique_ptr<stdplus::fd::FdMock> fdMock;
    stdplus::fd::FdMock* fdMockPtr;

    std::unique_ptr<PciDataHandler> pciDataHandler;
};

TEST_F(PciHandlerTest, GetMemoryRegionSizeSanity)
{
    EXPECT_EQ(pciDataHandler->getMemoryRegionSize(), testRegionSize);
}

TEST_F(PciHandlerTest, BoundaryChecksReadFail)
{
    std::vector<uint8_t> emptyVector;
    // Zero size
    EXPECT_THAT(pciDataHandler->read(0, 0), ElementsAreArray(emptyVector));

    const int offsetTooBig = testRegionSize + 1;
    EXPECT_THAT(pciDataHandler->read(offsetTooBig, 1),
                ElementsAreArray(emptyVector));
}

TEST_F(PciHandlerTest, BoundaryChecksWriteFail)
{
    std::vector<uint8_t> emptyVector;
    // Zero size
    EXPECT_EQ(pciDataHandler->write(0, emptyVector), 0);

    const int offsetTooBig = testRegionSize + 1;
    std::vector<uint8_t> testVector(testRegionSize - 1);
    EXPECT_EQ(pciDataHandler->write(offsetTooBig, testVector), 0);
}

TEST_F(PciHandlerTest, ReadPasses)
{
    // Normal read from 0
    uint32_t testOffset = 0;
    uint32_t testSize = 2;
    std::vector<uint8_t> expectedVector{0, 11};
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(expectedVector));

    // Read to buffer boundary from non 0 offset
    testOffset = 3;
    testSize = testRegionSize - testOffset;
    expectedVector.clear();
    expectedVector = {33, 44, 55, 66, 77};
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(expectedVector));

    // Read over buffer boundary (which will read until the end)
    testOffset = 4;
    testSize = testRegionSize - testOffset + 1;
    expectedVector.clear();
    expectedVector = {44, 55, 66, 77};
    EXPECT_THAT(pciDataHandler->read(testOffset, testSize),
                ElementsAreArray(expectedVector));
}

TEST_F(PciHandlerTest, WritePasses)
{
    std::vector<std::byte> expectedMapped{
        std::byte(0),  std::byte(11), std::byte(22), std::byte(33),
        std::byte(44), std::byte(55), std::byte(66), std::byte(77)};

    // Normal write from 0
    uint32_t testOffset = 0;
    std::vector<uint8_t> writeVector{99, 88};
    expectedMapped[0] = std::byte(99);
    expectedMapped[1] = std::byte(88);

    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              writeVector.size());
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));

    // Write to buffer boundary from non 0 offset
    testOffset = 4;
    writeVector = {55, 44, 33, 22};
    expectedMapped[4] = std::byte(55);
    expectedMapped[5] = std::byte(44);
    expectedMapped[6] = std::byte(33);
    expectedMapped[7] = std::byte(22);
    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              writeVector.size());
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));

    // Read over buffer boundary (which will read until the end)
    testOffset = 7;
    writeVector = {12, 23, 45};
    expectedMapped[7] = std::byte(12);
    EXPECT_EQ(pciDataHandler->write(testOffset, writeVector),
              testRegionSize - testOffset);
    EXPECT_THAT(testMapped, ElementsAreArray(expectedMapped));
}

} // namespace
} // namespace bios_bmc_smm_error_logger

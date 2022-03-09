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
using ::testing::Return;

class PciHandlerTest : public ::testing::Test
{
  protected:
    PciHandlerTest() :
        pciDataHandler(std::make_unique<PciDataHandler>(
            testRegionAddress, testRegionSize, &sys_mock))
    {}

    static constexpr uint32_t testRegionAddress = 0xF0848000;
    static constexpr size_t testRegionSize = 16 * 1024UL;
    static constexpr int testFd = 22;
    uint8_t testMapped = 0;
    void* testMappedPtr = &testMapped;

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
    EXPECT_CALL(sys_mock, open(_, _)).WillOnce(Return(testFd));
    EXPECT_CALL(sys_mock, mmap(_, _, _, _, _, _))
        .WillOnce(Return(testMappedPtr));
    EXPECT_CALL(sys_mock, close(testFd)).Times(0);
    EXPECT_TRUE(pciDataHandler->open());

    // Destructor cleans up
    EXPECT_CALL(sys_mock, munmap(testMappedPtr, testRegionSize))
        .WillOnce(Return(true));
    EXPECT_CALL(sys_mock, close(testFd)).WillOnce(Return(true));
}

TEST_F(PciHandlerTest, ReadOffsetTooBigFails)
{
    testRegionSize
}

} // namespace
} // namespace biosBmcSmmErrorLogger

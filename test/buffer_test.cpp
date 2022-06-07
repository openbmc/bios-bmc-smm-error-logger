#include "buffer.hpp"
#include "data_interface_mock.hpp"

#include <array>
#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bios_bmc_smm_error_logger
{
namespace
{

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::Return;

class BufferTest : public ::testing::Test
{
  protected:
    BufferTest() :
        dataInterfaceMock(std::make_unique<DataInterfaceMock>()),
        dataInterfaceMockPtr(dataInterfaceMock.get())
    {
        bufferImpl = std::make_unique<BufferImpl>(std::move(dataInterfaceMock));
        testInitializationHeader.bmcInterfaceVersion = testBmcInterfaceVersion;
        testInitializationHeader.queueSize = testQueueSize;
        testInitializationHeader.ueRegionSize = testUeRegionSize;
        testInitializationHeader.magicNumber = testMagicNumber;
    }
    ~BufferTest() override = default;

    // CircularBufferHeader size is 0x30, ensure the test region is bigger
    static constexpr size_t testRegionSize = 0x200;
    static constexpr uint32_t testBmcInterfaceVersion = 123;
    static constexpr uint16_t testQueueSize = 0x100;
    static constexpr uint16_t testUeRegionSize = 0x50;
    static constexpr std::array<uint32_t, 4> testMagicNumber = {
        0x12345678, 0x22345678, 0x32345678, 0x42345678};
    struct CircularBufferHeader testInitializationHeader
    {};

    std::unique_ptr<DataInterfaceMock> dataInterfaceMock;
    DataInterfaceMock* dataInterfaceMockPtr;

    std::unique_ptr<BufferImpl> bufferImpl;
};

TEST_F(BufferTest, BufferInitializeEraseFail)
{
    InSequence s;

    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    const std::vector<uint8_t> emptyArray(testRegionSize, 0);
    // Return a smaller write than the intended testRegionSize to test the error
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testRegionSize - 1));
    EXPECT_THROW(
        try {
            bufferImpl->initialize(testBmcInterfaceVersion, testQueueSize,
                                   testUeRegionSize, testMagicNumber);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Buffer initialization only erased '511'");
            throw;
        },
        std::runtime_error);

    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testRegionSize));
    // Return a smaller write than the intended initializationHeader to test the
    // error
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, _)).WillOnce(Return(0));
    EXPECT_THROW(
        try {
            bufferImpl->initialize(testBmcInterfaceVersion, testQueueSize,
                                   testUeRegionSize, testMagicNumber);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "Buffer initialization buffer header write only wrote '0'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferTest, BufferInitializePass)
{
    InSequence s;
    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    const std::vector<uint8_t> emptyArray(testRegionSize, 0);
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testRegionSize));

    uint8_t* testInitializationHeaderPtr =
        reinterpret_cast<uint8_t*>(&testInitializationHeader);
    size_t initializationHeaderSize = sizeof(testInitializationHeader);
    EXPECT_CALL(*dataInterfaceMockPtr,
                write(0, ElementsAreArray(testInitializationHeaderPtr,
                                          initializationHeaderSize)))
        .WillOnce(Return(initializationHeaderSize));
    EXPECT_NO_THROW(bufferImpl->initialize(testBmcInterfaceVersion,
                                           testQueueSize, testUeRegionSize,
                                           testMagicNumber));
}

} // namespace
} // namespace bios_bmc_smm_error_logger

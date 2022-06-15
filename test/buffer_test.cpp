#include "buffer.hpp"
#include "data_interface_mock.hpp"

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/conversion.hpp>

#include <algorithm>
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
        std::transform(testMagicNumber.begin(), testMagicNumber.end(),
                       testInitializationHeader.magicNumber.begin(),
                       [](uint32_t number) -> little_uint32_t {
                           return boost::endian::native_to_little(number);
                       });
    }
    ~BufferTest() override = default;

    // CircularBufferHeader size is 0x30, ensure the test region is bigger
    static constexpr size_t testRegionSize = 0x200;
    static constexpr uint32_t testBmcInterfaceVersion = 123;
    static constexpr uint16_t testQueueSize = 0x100;
    static constexpr uint16_t testUeRegionSize = 0x50;
    static constexpr std::array<uint32_t, 4> testMagicNumber = {
        0x12345678, 0x22345678, 0x32345678, 0x42345678};
    static constexpr size_t bufferHeaderSize =
        sizeof(struct CircularBufferHeader);

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
    EXPECT_NE(bufferImpl->getCachedBufferHeader(), testInitializationHeader);

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
    EXPECT_NE(bufferImpl->getCachedBufferHeader(), testInitializationHeader);
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
    EXPECT_CALL(*dataInterfaceMockPtr,
                write(0, ElementsAreArray(testInitializationHeaderPtr,
                                          bufferHeaderSize)))
        .WillOnce(Return(bufferHeaderSize));
    EXPECT_NO_THROW(bufferImpl->initialize(testBmcInterfaceVersion,
                                           testQueueSize, testUeRegionSize,
                                           testMagicNumber));
    EXPECT_EQ(bufferImpl->getCachedBufferHeader(), testInitializationHeader);
}

TEST_F(BufferTest, BufferHeaderReadFail)
{
    std::vector<std::uint8_t> testBytesRead{};
    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(testBytesRead));
    EXPECT_THROW(
        try {
            bufferImpl->readBufferHeader();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(),
                         "Buffer header read only read '0', expected '48'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferTest, BufferHeaderReadPass)
{
    uint8_t* testInitializationHeaderPtr =
        reinterpret_cast<uint8_t*>(&testInitializationHeader);
    std::vector<uint8_t> testInitializationHeaderVector(
        testInitializationHeaderPtr,
        testInitializationHeaderPtr + bufferHeaderSize);

    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(testInitializationHeaderVector));
    EXPECT_NO_THROW(bufferImpl->readBufferHeader());
    EXPECT_EQ(bufferImpl->getCachedBufferHeader(), testInitializationHeader);
}

TEST_F(BufferTest, BufferUpdateReadPtrFail)
{
    // Return write size that is not 2 which is sizeof(little_uint16_t)
    constexpr size_t wrongWriteSize = 1;
    EXPECT_CALL(*dataInterfaceMockPtr, write(_, _))
        .WillOnce(Return(wrongWriteSize));
    EXPECT_THROW(
        try {
            bufferImpl->updateReadPtr(0);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[updateReadPtr] Wrote '1' bytes, instead of expected '2'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferTest, BufferUpdateReadPtrPass)
{
    constexpr size_t expectedWriteSize = 2;
    constexpr uint8_t expectedBmcReadPtrOffset = 0x20;
    // Check that we truncate the highest 16bits
    const uint32_t testNewReadPtr = 0x99881234;
    const std::vector<uint8_t> expectedReadPtr{0x34, 0x12};

    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));
    EXPECT_NO_THROW(bufferImpl->updateReadPtr(testNewReadPtr));
}

} // namespace
} // namespace bios_bmc_smm_error_logger

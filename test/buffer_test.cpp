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

class BufferWraparoundReadTest : public BufferTest
{
  protected:
    BufferWraparoundReadTest()
    {
        // Initialize the memory and the cachedBufferHeader
        InSequence s;
        EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
            .WillOnce(Return(testRegionSize));
        const std::vector<uint8_t> emptyArray(testRegionSize, 0);
        EXPECT_CALL(*dataInterfaceMockPtr,
                    write(0, ElementsAreArray(emptyArray)))
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
    }
    static constexpr size_t expectedWriteSize = 2;
    static constexpr uint8_t expectedBmcReadPtrOffset = 0x20;
    static constexpr size_t expectedqueueOffset = 0x30 + testUeRegionSize;
};

TEST_F(BufferWraparoundReadTest, TooBigReadFail)
{
    InSequence s;
    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    size_t tooBigLength = testRegionSize - expectedqueueOffset + 1;
    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(/* offset */ 0, tooBigLength);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[wraparoundRead] queueOffset '128' + length '385' was "
                "bigger than memoryRegionSize '512'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferWraparoundReadTest, NoWrapAroundReadPass)
{
    InSequence s;
    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    size_t testLength = 0x10;
    size_t testOffset = 0x50;

    // Successfully read all the requested length without a wrap around
    std::vector<std::uint8_t> testBytesRead(testLength);
    EXPECT_CALL(*dataInterfaceMockPtr, read(testOffset, testLength))
        .WillOnce(Return(testBytesRead));

    // Call to updateReadPtr is triggered
    const std::vector<uint8_t> expectedReadPtr{
        static_cast<uint8_t>(testOffset + testLength), 0x0};
    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));

    EXPECT_THAT(bufferImpl->wraparoundRead(testOffset, testLength),
                ElementsAreArray(testBytesRead));
}

TEST_F(BufferWraparoundReadTest, WrapAroundReadFails)
{
    InSequence s;
    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    size_t testBytesLeft = 3;
    size_t testLength = 0x10;
    size_t testOffset = testRegionSize - (testLength - testBytesLeft);

    // Read 3 bytes short
    std::vector<std::uint8_t> testBytesReadShort(testLength - testBytesLeft);
    EXPECT_CALL(*dataInterfaceMockPtr, read(testOffset, testLength))
        .WillOnce(Return(testBytesReadShort));

    // Read 1 byte short after wraparound
    std::vector<std::uint8_t> testBytesLeftReadShort(testBytesLeft - 1);
    EXPECT_CALL(*dataInterfaceMockPtr, read(expectedqueueOffset, testBytesLeft))
        .WillOnce(Return(testBytesLeftReadShort));

    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(testOffset, testLength);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(),
                         "[wraparoundRead] Buffer wrapped around but was not "
                         "able to read all of the requested info. "
                         "Bytes remaining to read '1' of '16'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferWraparoundReadTest, WrapAroundReadPasses)
{
    InSequence s;
    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    size_t testBytesLeft = 3;
    size_t testLength = 0x10;
    size_t testOffset = testRegionSize - (testLength - testBytesLeft);

    // Read 3 bytes short
    std::vector<std::uint8_t> testBytesReadFirst{16, 15, 14, 13, 12, 11, 10,
                                                 9,  8,  7,  6,  5,  4};
    EXPECT_CALL(*dataInterfaceMockPtr, read(testOffset, testLength))
        .WillOnce(Return(testBytesReadFirst));

    std::vector<std::uint8_t> testBytesReadSecond{3, 2, 1};
    EXPECT_CALL(*dataInterfaceMockPtr, read(expectedqueueOffset, testBytesLeft))
        .WillOnce(Return(testBytesReadSecond));

    // Call to updateReadPtr is triggered
    const std::vector<uint8_t> expectedReadPtr{
        static_cast<uint8_t>(expectedqueueOffset + testBytesLeft), 0x0};
    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));

    std::vector<std::uint8_t> expectedBytes = {16, 15, 14, 13, 12, 11, 10, 9,
                                               8,  7,  6,  5,  4,  3,  2,  1};
    EXPECT_THAT(bufferImpl->wraparoundRead(testOffset, testLength),
                ElementsAreArray(expectedBytes));
}

class BufferEntryHeaderTest : public BufferWraparoundReadTest
{
  protected:
    BufferEntryHeaderTest()
    {
        testEntryHeader.sequenceId = testSequenceId;
        testEntryHeader.entrySize = testEntrySize;
        testEntryHeader.checksum = testChecksum;
        testEntryHeader.rdeCommandType = testRdeCommandType;
    }
    ~BufferEntryHeaderTest() override = default;

    void wraparoundReadMock(std::span<std::uint8_t> expetedBytesOutput)
    {
        EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
            .WillOnce(Return(testRegionSize));
        EXPECT_CALL(*dataInterfaceMockPtr, read(_, _))
            .WillOnce(Return(std::vector<std::uint8_t>(
                expetedBytesOutput.begin(), expetedBytesOutput.end())));

        EXPECT_CALL(*dataInterfaceMockPtr, write(_, _))
            .WillOnce(Return(expectedWriteSize));
    }

    static constexpr size_t entryHeaderSize = sizeof(struct QueueEntryHeader);
    static constexpr uint16_t testSequenceId = 0;
    static constexpr uint16_t testEntrySize = 0x20;
    static constexpr uint8_t testChecksum = 1;
    static constexpr uint8_t testRdeCommandType = 0x01;
    size_t testOffset = 0x50;

    struct QueueEntryHeader testEntryHeader
    {};
};

TEST_F(BufferEntryHeaderTest, ReadEntryHeaderPass)
{
    uint8_t* testEntryHeaderPtr = reinterpret_cast<uint8_t*>(&testEntryHeader);
    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    wraparoundReadMock(testEntryHeaderVector);
    EXPECT_EQ(bufferImpl->readEntryHeader(testOffset), testEntryHeader);
}

} // namespace
} // namespace bios_bmc_smm_error_logger

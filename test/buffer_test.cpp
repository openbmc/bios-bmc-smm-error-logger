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
    EXPECT_THROW(
        try {
            // Test too big of a proposed buffer compared to the memori size
            uint16_t bigQueueSize = 0x181;
            uint16_t bigUeRegionSize = 0x50;
            bufferImpl->initialize(testBmcInterfaceVersion, bigQueueSize,
                                   bigUeRegionSize, testMagicNumber);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[initialize] Proposed region size '513' "
                "is bigger than the BMC's allocated MMIO region of '512'");
            throw;
        },
        std::runtime_error);
    EXPECT_NE(bufferImpl->getCachedBufferHeader(), testInitializationHeader);

    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    size_t testProposedBufferSize =
        sizeof(struct CircularBufferHeader) + testUeRegionSize + testQueueSize;
    const std::vector<uint8_t> emptyArray(testProposedBufferSize, 0);
    // Return a smaller write than the intended testRegionSize to test the error
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testProposedBufferSize - 1));
    EXPECT_THROW(
        try {
            bufferImpl->initialize(testBmcInterfaceVersion, testQueueSize,
                                   testUeRegionSize, testMagicNumber);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "[initialize] Only erased '383'");
            throw;
        },
        std::runtime_error);
    EXPECT_NE(bufferImpl->getCachedBufferHeader(), testInitializationHeader);

    EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
        .WillOnce(Return(testRegionSize));
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testProposedBufferSize));
    // Return a smaller write than the intended initializationHeader to test the
    // error
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, _)).WillOnce(Return(0));
    EXPECT_THROW(
        try {
            bufferImpl->initialize(testBmcInterfaceVersion, testQueueSize,
                                   testUeRegionSize, testMagicNumber);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(),
                         "[initialize] Only wrote '0' bytes of the header");
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
    size_t testProposedBufferSize =
        sizeof(struct CircularBufferHeader) + testUeRegionSize + testQueueSize;
    const std::vector<uint8_t> emptyArray(testProposedBufferSize, 0);
    EXPECT_CALL(*dataInterfaceMockPtr, write(0, ElementsAreArray(emptyArray)))
        .WillOnce(Return(testProposedBufferSize));

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
        initializeFuncMock();
    }
    void initializeFuncMock()
    {
        // Initialize the memory and the cachedBufferHeader
        InSequence s;
        EXPECT_CALL(*dataInterfaceMockPtr, getMemoryRegionSize())
            .WillOnce(Return(testRegionSize));
        size_t testProposedBufferSize = sizeof(struct CircularBufferHeader) +
                                        testUeRegionSize + testQueueSize;
        const std::vector<uint8_t> emptyArray(testProposedBufferSize, 0);
        EXPECT_CALL(*dataInterfaceMockPtr,
                    write(0, ElementsAreArray(emptyArray)))
            .WillOnce(Return(testProposedBufferSize));

        EXPECT_CALL(*dataInterfaceMockPtr, write(0, _))
            .WillOnce(Return(bufferHeaderSize));
        EXPECT_NO_THROW(bufferImpl->initialize(testBmcInterfaceVersion,
                                               testQueueSize, testUeRegionSize,
                                               testMagicNumber));
    }
    static constexpr size_t expectedWriteSize = 2;
    static constexpr uint8_t expectedBmcReadPtrOffset = 0x20;
    static constexpr size_t expectedqueueOffset = 0x30 + testUeRegionSize;

    uint8_t* testInitializationHeaderPtr =
        reinterpret_cast<uint8_t*>(&testInitializationHeader);
};

TEST_F(BufferWraparoundReadTest, ParamsTooBigFail)
{
    InSequence s;

    size_t tooBigOffset = testQueueSize + 1;
    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(tooBigOffset, /* length */ 1);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[wraparoundRead] relativeOffset '257' was bigger than queueSize '256'");
            throw;
        },
        std::runtime_error);

    size_t tooBigLength = testQueueSize + 1;
    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(/* relativeOffset */ 0, tooBigLength);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "[wraparoundRead] length '257' was bigger "
                                   "than queueSize '256'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferWraparoundReadTest, NoWrapAroundReadFails)
{
    InSequence s;
    size_t testLength = 0x10;
    size_t testOffset = 0x20;

    // Fail the first read
    std::vector<std::uint8_t> shortTestBytesRead(testLength - 1);
    EXPECT_CALL(*dataInterfaceMockPtr,
                read(testOffset + expectedqueueOffset, testLength))
        .WillOnce(Return(shortTestBytesRead));

    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(testOffset, testLength);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(),
                         "[wraparoundRead] Read '15' which was not the "
                         "requested length of '16'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferWraparoundReadTest, NoWrapAroundReadPass)
{
    InSequence s;
    size_t testLength = 0x10;
    size_t testOffset = 0x20;

    // Successfully read all the requested length without a wrap around
    std::vector<std::uint8_t> testBytesRead(testLength);
    EXPECT_CALL(*dataInterfaceMockPtr,
                read(testOffset + expectedqueueOffset, testLength))
        .WillOnce(Return(testBytesRead));

    // Call to updateReadPtr is triggered
    const std::vector<uint8_t> expectedReadPtr{
        static_cast<uint8_t>(testOffset + testLength), 0x0};
    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));

    EXPECT_THAT(bufferImpl->wraparoundRead(testOffset, testLength),
                ElementsAreArray(testBytesRead));
    struct CircularBufferHeader cachedBufferHeader =
        bufferImpl->getCachedBufferHeader();
    // The bmcReadPtr should have been updated
    EXPECT_EQ(boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr),
              testOffset + testLength);
}

TEST_F(BufferWraparoundReadTest, WrapAroundReadFails)
{
    InSequence s;
    size_t testBytesLeft = 3;
    size_t testLength = 0x10;
    size_t testOffset = testQueueSize - (testLength - testBytesLeft);

    // Read until the end of the queue
    std::vector<std::uint8_t> testBytesReadShort(testLength - testBytesLeft);
    EXPECT_CALL(*dataInterfaceMockPtr, read(testOffset + expectedqueueOffset,
                                            testLength - testBytesLeft))
        .WillOnce(Return(testBytesReadShort));

    // Read 1 byte short after wraparound
    std::vector<std::uint8_t> testBytesLeftReadShort(testBytesLeft - 1);
    EXPECT_CALL(*dataInterfaceMockPtr, read(expectedqueueOffset, testBytesLeft))
        .WillOnce(Return(testBytesLeftReadShort));

    EXPECT_THROW(
        try {
            bufferImpl->wraparoundRead(testOffset, testLength);
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[wraparoundRead] Buffer wrapped around but read '2' which was "
                "not the requested lenght of '3'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferWraparoundReadTest, WrapAroundReadPasses)
{
    InSequence s;
    size_t testBytesLeft = 3;
    size_t testLength = 0x10;
    size_t testOffset = testQueueSize - (testLength - testBytesLeft);

    // Read to the end of the queue
    std::vector<std::uint8_t> testBytesReadFirst{16, 15, 14, 13, 12, 11, 10,
                                                 9,  8,  7,  6,  5,  4};
    EXPECT_CALL(*dataInterfaceMockPtr, read(testOffset + expectedqueueOffset,
                                            testLength - testBytesLeft))
        .WillOnce(Return(testBytesReadFirst));

    std::vector<std::uint8_t> testBytesReadSecond{3, 2, 1};
    EXPECT_CALL(*dataInterfaceMockPtr, read(expectedqueueOffset, testBytesLeft))
        .WillOnce(Return(testBytesReadSecond));

    // Call to updateReadPtr is triggered
    const std::vector<uint8_t> expectedReadPtr{
        static_cast<uint8_t>(testBytesLeft), 0x0};
    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));

    std::vector<std::uint8_t> expectedBytes = {16, 15, 14, 13, 12, 11, 10, 9,
                                               8,  7,  6,  5,  4,  3,  2,  1};
    EXPECT_THAT(bufferImpl->wraparoundRead(testOffset, testLength),
                ElementsAreArray(expectedBytes));
    struct CircularBufferHeader cachedBufferHeader =
        bufferImpl->getCachedBufferHeader();
    // The bmcReadPtr should have been updated to reflect the wraparound
    EXPECT_EQ(boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr),
              testBytesLeft);
}

TEST_F(BufferWraparoundReadTest, WrapAroundCornerCasePass)
{
    InSequence s;
    size_t testBytesLeft = 0;
    size_t testLength = 4;
    size_t testOffset = testQueueSize - (testLength - testBytesLeft);

    // Read to the very end of the queue
    std::vector<std::uint8_t> testBytes{4, 3, 2, 1};
    EXPECT_CALL(*dataInterfaceMockPtr,
                read(testOffset + expectedqueueOffset, testLength))
        .WillOnce(Return(testBytes));

    // Call to updateReadPtr is triggered, since we read to the very end of the
    // buffer, update the readPtr up around to 0
    const std::vector<uint8_t> expectedReadPtr{0x0, 0x0};
    EXPECT_CALL(*dataInterfaceMockPtr, write(expectedBmcReadPtrOffset,
                                             ElementsAreArray(expectedReadPtr)))
        .WillOnce(Return(expectedWriteSize));

    EXPECT_THAT(bufferImpl->wraparoundRead(testOffset, testLength),
                ElementsAreArray(testBytes));
    struct CircularBufferHeader cachedBufferHeader =
        bufferImpl->getCachedBufferHeader();
    // The bmcReadPtr should have been updated to reflect the wraparound
    EXPECT_EQ(boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr),
              0);
}

class BufferEntryTest : public BufferWraparoundReadTest
{
  protected:
    BufferEntryTest()
    {
        testEntryHeader.sequenceId = testSequenceId;
        testEntryHeader.entrySize = testEntrySize;
        testEntryHeader.checksum = testChecksum;
        testEntryHeader.rdeCommandType = testRdeCommandType;
    }
    ~BufferEntryTest() override = default;

    void wraparoundReadMock(const uint32_t relativeOffset,
                            std::span<std::uint8_t> expetedBytesOutput)
    {
        InSequence s;
        const uint32_t queueSizeToQueueEnd = testQueueSize - relativeOffset;

        // This will wrap, split the read mocks in 2
        if (expetedBytesOutput.size() > queueSizeToQueueEnd)
        {
            EXPECT_CALL(*dataInterfaceMockPtr, read(_, _))
                .WillOnce(Return(std::vector<std::uint8_t>(
                    expetedBytesOutput.begin(),
                    expetedBytesOutput.begin() + queueSizeToQueueEnd)));
            EXPECT_CALL(*dataInterfaceMockPtr, read(_, _))
                .WillOnce(Return(std::vector<std::uint8_t>(
                    expetedBytesOutput.begin() + queueSizeToQueueEnd,
                    expetedBytesOutput.end())));
        }
        else
        {
            EXPECT_CALL(*dataInterfaceMockPtr, read(_, _))
                .WillOnce(Return(std::vector<std::uint8_t>(
                    expetedBytesOutput.begin(), expetedBytesOutput.end())));
        }

        EXPECT_CALL(*dataInterfaceMockPtr, write(_, _))
            .WillOnce(Return(expectedWriteSize));
    }

    static constexpr size_t entryHeaderSize = sizeof(struct QueueEntryHeader);
    static constexpr uint16_t testSequenceId = 0;
    static constexpr uint16_t testEntrySize = 0x20;
    static constexpr uint8_t testRdeCommandType = 0x01;
    // Calculated checksum for the header
    static constexpr uint8_t testChecksum =
        (testSequenceId ^ testEntrySize ^ testRdeCommandType);
    size_t testOffset = 0x20;

    struct QueueEntryHeader testEntryHeader
    {};
};

TEST_F(BufferEntryTest, ReadEntryHeaderPass)
{
    uint8_t* testEntryHeaderPtr = reinterpret_cast<uint8_t*>(&testEntryHeader);
    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    wraparoundReadMock(testOffset, testEntryHeaderVector);
    EXPECT_EQ(bufferImpl->readEntryHeader(testOffset), testEntryHeader);
    // Check the bmcReadPtr
    struct CircularBufferHeader cachedBufferHeader =
        bufferImpl->getCachedBufferHeader();
    EXPECT_EQ(boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr),
              testOffset + testEntryHeaderVector.size());
}

TEST_F(BufferEntryTest, ReadEntryChecksumFail)
{
    InSequence s;
    std::vector<uint8_t> testEntryVector(testEntrySize);
    // Offset the checksum by 1
    testEntryHeader.checksum += 1;
    uint8_t* testEntryHeaderPtr = reinterpret_cast<uint8_t*>(&testEntryHeader);
    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    wraparoundReadMock(testOffset, testEntryHeaderVector);
    wraparoundReadMock(testOffset + entryHeaderSize, testEntryVector);
    EXPECT_THROW(
        try {
            bufferImpl->readEntry(testOffset);
        } catch (const std::runtime_error& e) {
            // Calculation: testChecksum (0x21) XOR (0x22) = 3
            EXPECT_STREQ(e.what(),
                         "[readEntry] Checksum was '3', expected '0'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferEntryTest, ReadEntryPass)
{
    InSequence s;
    // We expect this will bump checksum up by "testEntrySize" = 0xff ^ 0xff ...
    // (20 times) = 0 therefore leave the checksum as is
    std::vector<uint8_t> testEntryVector(testEntrySize, 0xff);
    uint8_t* testEntryHeaderPtr = reinterpret_cast<uint8_t*>(&testEntryHeader);
    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    // Set testOffset so that we can test the wraparound here as well on our
    // second read for the entry (by 1 byte)
    testOffset = testQueueSize - entryHeaderSize - 1;
    wraparoundReadMock(testOffset, testEntryHeaderVector);
    wraparoundReadMock(testOffset + entryHeaderSize, testEntryVector);

    EntryPair testedEntryPair;
    EXPECT_NO_THROW(testedEntryPair = bufferImpl->readEntry(testOffset));
    EXPECT_EQ(testedEntryPair.first, testEntryHeader);
    EXPECT_THAT(testedEntryPair.second, ElementsAreArray(testEntryVector));
    struct CircularBufferHeader cachedBufferHeader =
        bufferImpl->getCachedBufferHeader();
    // The bmcReadPtr should have been updated to reflect the wraparound
    EXPECT_EQ(boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr),
              testEntrySize - 1);
}

class BufferReadErrorLogsTest : public BufferEntryTest
{
  protected:
    BufferReadErrorLogsTest() = default;

    uint8_t* testEntryHeaderPtr = reinterpret_cast<uint8_t*>(&testEntryHeader);
    size_t entryAndHeaderSize = entryHeaderSize + testEntrySize;
};

TEST_F(BufferReadErrorLogsTest, PtrsTooBigFail)
{
    InSequence s;
    // Set the biosWritePtr too big
    testInitializationHeader.biosWritePtr =
        boost::endian::native_to_little((testQueueSize + 1));
    initializeFuncMock();

    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));
    EXPECT_THROW(
        try {
            bufferImpl->readErrorLogs();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[readErrorLogs] currentBiosWritePtr was '257' which was bigger than queueSize '256'");
            throw;
        },
        std::runtime_error);

    // Reset the biosWritePtr and set the bmcReadPtr too big
    testInitializationHeader.biosWritePtr = 0;
    initializeFuncMock();
    testInitializationHeader.bmcReadPtr =
        boost::endian::native_to_little((testQueueSize + 1));
    initializeFuncMock();

    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));
    EXPECT_THROW(
        try {
            bufferImpl->readErrorLogs();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[readErrorLogs] currentReadPtr was '257' which was bigger than queueSize '256'");
            throw;
        },
        std::runtime_error);
}

TEST_F(BufferReadErrorLogsTest, IdenticalPtrsPass)
{
    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));
    EXPECT_NO_THROW(bufferImpl->readErrorLogs());
}

TEST_F(BufferReadErrorLogsTest, NoWraparoundPass)
{
    InSequence s;
    // Set the biosWritePtr to 1 entryHeader + entry size
    testInitializationHeader.biosWritePtr =
        boost::endian::native_to_little((entryAndHeaderSize));
    initializeFuncMock();
    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));
    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    std::vector<uint8_t> testEntryVector(testEntrySize);
    wraparoundReadMock(/* offset */ 0, testEntryHeaderVector);
    wraparoundReadMock(/* offset */ 0 + entryHeaderSize, testEntryVector);

    std::vector<EntryPair> entryPairs;
    EXPECT_NO_THROW(entryPairs = bufferImpl->readErrorLogs());

    // Check that we only read one entryPair and that the content is correct
    EXPECT_EQ(entryPairs.size(), 1);
    EXPECT_EQ(entryPairs[0].first, testEntryHeader);
    EXPECT_THAT(entryPairs[0].second, ElementsAreArray(testEntryVector));
}

TEST_F(BufferReadErrorLogsTest, WraparoundMultiplEntryPass)
{
    InSequence s;
    // Set the bmcReadPtr to 1 entryHeader + entry size from the "end" exactly
    uint32_t entryAndHeaderSizeAwayFromEnd = testQueueSize - entryAndHeaderSize;
    testInitializationHeader.bmcReadPtr =
        boost::endian::native_to_little(entryAndHeaderSizeAwayFromEnd);
    // Set the biosWritePtr to 1 entryHeader + entry size from the "beginning"
    testInitializationHeader.biosWritePtr = entryAndHeaderSize;
    initializeFuncMock();
    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));

    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    std::vector<uint8_t> testEntryVector(testEntrySize);
    wraparoundReadMock(/* offset */ entryAndHeaderSizeAwayFromEnd,
                       testEntryHeaderVector);
    wraparoundReadMock(/* offset */ entryAndHeaderSizeAwayFromEnd +
                           entryHeaderSize,
                       testEntryVector);
    wraparoundReadMock(/* offset */ 0 + entryAndHeaderSize,
                       testEntryHeaderVector);
    wraparoundReadMock(/* offset */ 0 + entryAndHeaderSize + entryHeaderSize,
                       testEntryVector);

    std::vector<EntryPair> entryPairs;
    EXPECT_NO_THROW(entryPairs = bufferImpl->readErrorLogs());

    // Check that we only read one entryPair and that the content is correct
    EXPECT_EQ(entryPairs.size(), 2);
    EXPECT_EQ(entryPairs[0].first, testEntryHeader);
    EXPECT_EQ(entryPairs[1].first, testEntryHeader);
    EXPECT_THAT(entryPairs[0].second, ElementsAreArray(testEntryVector));
    EXPECT_THAT(entryPairs[1].second, ElementsAreArray(testEntryVector));
}

TEST_F(BufferReadErrorLogsTest, WraparoundMismatchingPtrsFail)
{
    InSequence s;
    testInitializationHeader.bmcReadPtr = boost::endian::native_to_little(0);
    // Make the biosWritePtr intentially 1 smaller than expected
    testInitializationHeader.biosWritePtr =
        boost::endian::native_to_little(entryAndHeaderSize - 1);
    initializeFuncMock();
    EXPECT_CALL(*dataInterfaceMockPtr, read(0, bufferHeaderSize))
        .WillOnce(Return(std::vector<uint8_t>(testInitializationHeaderPtr,
                                              testInitializationHeaderPtr +
                                                  bufferHeaderSize)));

    std::vector<uint8_t> testEntryHeaderVector(
        testEntryHeaderPtr, testEntryHeaderPtr + entryHeaderSize);
    std::vector<uint8_t> testEntryVector(testEntrySize);
    wraparoundReadMock(/* offset */ 0, testEntryHeaderVector);
    wraparoundReadMock(/* offset */ 0 + entryHeaderSize, testEntryVector);

    EXPECT_THROW(
        try {
            bufferImpl->readErrorLogs();
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(
                e.what(),
                "[readErrorLogs] biosWritePtr '37' and bmcReaddPtr '38' "
                "are not identical after reading through all the logs");
            throw;
        },
        std::runtime_error);
}

} // namespace
} // namespace bios_bmc_smm_error_logger

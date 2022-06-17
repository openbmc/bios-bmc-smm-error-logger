#include "buffer.hpp"

#include "pci_handler.hpp"

#include <fmt/format.h>

#include <boost/endian/arithmetic.hpp>
#include <boost/endian/conversion.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <span>
#include <vector>

namespace bios_bmc_smm_error_logger
{

BufferImpl::BufferImpl(std::unique_ptr<DataInterface> dataInterface) :
    dataInterface(std::move(dataInterface)){};

void BufferImpl::initialize(uint32_t bmcInterfaceVersion, uint16_t queueSize,
                            uint16_t ueRegionSize,
                            const std::array<uint32_t, 4>& magicNumber)
{
    const size_t memoryRegionSize = dataInterface->getMemoryRegionSize();
    const size_t proposedBufferSize =
        sizeof(struct CircularBufferHeader) + ueRegionSize + queueSize;
    if (proposedBufferSize > memoryRegionSize)
    {
        throw std::runtime_error(fmt::format(
            "[initialize] Proposed region size '{}' is bigger than the "
            "BMC's allocated MMIO region of '{}'",
            proposedBufferSize, memoryRegionSize));
    }

    // Initialize the whole buffer with 0x00
    const std::vector<uint8_t> emptyVector(proposedBufferSize, 0);
    size_t byteWritten = dataInterface->write(0, emptyVector);
    if (byteWritten != proposedBufferSize)
    {
        throw std::runtime_error(
            fmt::format("[initialize] Only erased '{}'", byteWritten));
    }

    // Create an initial buffer header and write to it
    struct CircularBufferHeader initializationHeader = {};
    initializationHeader.bmcInterfaceVersion =
        boost::endian::native_to_little(bmcInterfaceVersion);
    initializationHeader.queueSize = boost::endian::native_to_little(queueSize);
    initializationHeader.ueRegionSize =
        boost::endian::native_to_little(ueRegionSize);
    std::transform(magicNumber.begin(), magicNumber.end(),
                   initializationHeader.magicNumber.begin(),
                   [](uint32_t number) -> little_uint32_t {
                       return boost::endian::native_to_little(number);
                   });

    uint8_t* initializationHeaderPtr =
        reinterpret_cast<uint8_t*>(&initializationHeader);
    size_t initializationHeaderSize = sizeof(initializationHeader);
    byteWritten = dataInterface->write(
        0, std::span<const uint8_t>(initializationHeaderPtr,
                                    initializationHeaderPtr +
                                        initializationHeaderSize));
    if (byteWritten != initializationHeaderSize)
    {
        throw std::runtime_error(fmt::format(
            "[initialize] Only wrote '{}' bytes of the header", byteWritten));
    }
    cachedBufferHeader = initializationHeader;
}

void BufferImpl::readBufferHeader()
{
    size_t headerSize = sizeof(struct CircularBufferHeader);
    std::vector<uint8_t> bytesRead =
        dataInterface->read(/*offset=*/0, headerSize);

    if (bytesRead.size() != headerSize)
    {
        throw std::runtime_error(
            fmt::format("Buffer header read only read '{}', expected '{}'",
                        bytesRead.size(), headerSize));
    }

    cachedBufferHeader =
        *reinterpret_cast<struct CircularBufferHeader*>(bytesRead.data());
};

struct CircularBufferHeader BufferImpl::getCachedBufferHeader() const
{
    return cachedBufferHeader;
}

void BufferImpl::updateReadPtr(const uint32_t newReadPtr)
{
    constexpr uint8_t bmcReadPtrOffset =
        offsetof(struct CircularBufferHeader, bmcReadPtr);

    little_uint16_t truncatedReadPtr =
        boost::endian::native_to_little(newReadPtr & 0xffff);
    uint8_t* truncatedReadPtrPtr =
        reinterpret_cast<uint8_t*>(&truncatedReadPtr);

    size_t writtenSize = dataInterface->write(
        bmcReadPtrOffset, std::span<const uint8_t>{
                              truncatedReadPtrPtr,
                              truncatedReadPtrPtr + sizeof(little_uint16_t)});
    if (writtenSize != sizeof(little_uint16_t))
    {
        throw std::runtime_error(fmt::format(
            "[updateReadPtr] Wrote '{}' bytes, instead of expected '{}'",
            writtenSize, sizeof(little_uint16_t)));
    }
    cachedBufferHeader.bmcReadPtr = truncatedReadPtr;
}

void BufferImpl::updateBmcFlags(const enum BmcFlags newBmcFlag)
{
    constexpr uint8_t bmcFlagsPtrOffset =
        offsetof(struct CircularBufferHeader, bmcFlags);

    little_uint32_t littleNewBmcFlag =
        boost::endian::native_to_little(static_cast<uint32_t>(newBmcFlag));
    uint8_t* littleNewBmcFlagPtr =
        reinterpret_cast<uint8_t*>(&littleNewBmcFlag);

    size_t writtenSize = dataInterface->write(
        bmcFlagsPtrOffset, std::span<const uint8_t>{
                               littleNewBmcFlagPtr,
                               littleNewBmcFlagPtr + sizeof(little_uint32_t)});
    if (writtenSize != sizeof(little_uint32_t))
    {
        throw std::runtime_error(fmt::format(
            "[updateBmcFlags] Wrote '{}' bytes, instead of expected '{}'",
            writtenSize, sizeof(little_uint32_t)));
    }
    cachedBufferHeader.bmcFlags = littleNewBmcFlag;
}

size_t BufferImpl::getQueueOffset()
{
    return sizeof(struct CircularBufferHeader) +
           boost::endian::little_to_native(cachedBufferHeader.ueRegionSize);
}

std::vector<uint8_t> BufferImpl::wraparoundRead(const uint32_t relativeOffset,
                                                const uint32_t length)
{
    const size_t queueSize =
        boost::endian::little_to_native(cachedBufferHeader.queueSize);

    if (relativeOffset > queueSize)
    {
        throw std::runtime_error(
            fmt::format("[wraparoundRead] relativeOffset '{}' was bigger "
                        "than queueSize '{}'",
                        relativeOffset, queueSize));
    }
    if (length > queueSize)
    {
        throw std::runtime_error(fmt::format(
            "[wraparoundRead] length '{}' was bigger than queueSize '{}'",
            length, queueSize));
    }

    // Do a calculation to see if the read will wraparound
    const size_t queueOffset = getQueueOffset();
    const size_t queueSizeToQueueEnd = queueSize - relativeOffset;
    size_t numWraparoundBytesToRead = 0;
    if (length > queueSizeToQueueEnd)
    {
        // This means we will wrap, count the bytes that are left to read
        numWraparoundBytesToRead = length - queueSizeToQueueEnd;
    }
    const size_t numBytesToReadTillQueueEnd = length - numWraparoundBytesToRead;

    std::vector<uint8_t> bytesRead = dataInterface->read(
        queueOffset + relativeOffset, numBytesToReadTillQueueEnd);
    if (bytesRead.size() != numBytesToReadTillQueueEnd)
    {
        throw std::runtime_error(
            fmt::format("[wraparoundRead] Read '{}' which was not "
                        "the requested length of '{}'",
                        bytesRead.size(), numBytesToReadTillQueueEnd));
    }
    size_t updatedReadPtr = relativeOffset + numBytesToReadTillQueueEnd;
    if (updatedReadPtr == queueSize)
    {
        // If we read all the way up to the end of the queue, we need to
        // manually wrap the updateReadPtr around to 0
        updatedReadPtr = 0;
    }

    // If there are any more bytes to be read beyond the buffer, wrap around and
    // read from the beginning of the buffer (offset by the queueOffset)
    if (numWraparoundBytesToRead > 0)
    {
        std::vector<uint8_t> wrappedBytesRead =
            dataInterface->read(queueOffset, numWraparoundBytesToRead);
        if (numWraparoundBytesToRead != wrappedBytesRead.size())
        {
            throw std::runtime_error(fmt::format(
                "[wraparoundRead] Buffer wrapped around but read '{}' which "
                "was not the requested lenght of '{}'",
                wrappedBytesRead.size(), numWraparoundBytesToRead));
        }
        bytesRead.insert(bytesRead.end(), wrappedBytesRead.begin(),
                         wrappedBytesRead.end());
        updatedReadPtr = numWraparoundBytesToRead;
    }
    updateReadPtr(updatedReadPtr);

    return bytesRead;
}

struct QueueEntryHeader BufferImpl::readEntryHeader(size_t relativeOffset)
{
    size_t headerSize = sizeof(struct QueueEntryHeader);
    // wraparonudRead will throw if it did not read all the bytes, let it
    // propagate up the stack
    std::vector<uint8_t> bytesRead = wraparoundRead(relativeOffset, headerSize);

    return *reinterpret_cast<struct QueueEntryHeader*>(bytesRead.data());
}

EntryPair BufferImpl::readEntry(size_t relativeOffset)
{
    struct QueueEntryHeader entryHeader = readEntryHeader(relativeOffset);
    size_t entrySize = boost::endian::little_to_native(entryHeader.entrySize);

    // wraparonudRead may throw if entrySize was bigger than the buffer or if it
    // was not able to read all the bytes, let it propagate up the stack
    std::vector<uint8_t> entry = wraparoundRead(
        relativeOffset + sizeof(struct QueueEntryHeader), entrySize);

    // Calculate the checksum
    uint8_t* entryHeaderPtr = reinterpret_cast<uint8_t*>(&entryHeader);
    uint8_t checksum =
        std::accumulate(entryHeaderPtr,
                        entryHeaderPtr + sizeof(struct QueueEntryHeader), 0,
                        std::bit_xor<void>()) ^
        std::accumulate(entry.begin(), entry.end(), 0, std::bit_xor<void>());

    if (checksum != 0)
    {
        throw std::runtime_error(fmt::format(
            "[readEntry] Checksum was '{}', expected '0'", checksum));
    }

    return {entryHeader, entry};
}

std::vector<EntryPair> BufferImpl::readErrorLogs()
{
    // Reading the buffer header will update the cachedBufferHeader
    readBufferHeader();

    const size_t queueSize =
        boost::endian::little_to_native(cachedBufferHeader.queueSize);
    size_t currentBiosWritePtr =
        boost::endian::little_to_native(cachedBufferHeader.biosWritePtr);
    if (currentBiosWritePtr > queueSize)
    {
        throw std::runtime_error(fmt::format(
            "[readErrorLogs] currentBiosWritePtr was '{}' which was bigger "
            "than queueSize '{}'",
            currentBiosWritePtr, queueSize));
    }
    size_t currentReadPtr =
        boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr);
    if (currentReadPtr > queueSize)
    {
        throw std::runtime_error(fmt::format(
            "[readErrorLogs] currentReadPtr was '{}' which was bigger "
            "than queueSize '{}'",
            currentReadPtr, queueSize));
    }

    size_t bytesToRead;
    if (currentBiosWritePtr == currentReadPtr)
    {
        // No new payload was detected, return an empty vector gracefully
        return {};
    }

    if (currentBiosWritePtr > currentReadPtr)
    {
        // Simply subtract in this case
        bytesToRead = currentBiosWritePtr - currentReadPtr;
    }
    else
    {
        // Calculate the bytes to the "end" (QueueSize - ReadPtr) +
        // bytes to read from the "beginning" (0 +  WritePtr)
        bytesToRead = (queueSize - currentReadPtr) + currentBiosWritePtr;
    }

    size_t byteRead = 0;
    std::vector<EntryPair> entryPairs;
    while (byteRead < bytesToRead)
    {
        EntryPair entryPair = readEntry(currentReadPtr);
        byteRead += sizeof(struct QueueEntryHeader) + entryPair.second.size();
        entryPairs.push_back(entryPair);

        // Note: readEntry() will update cachedBufferHeader.bmcReadPtr
        currentReadPtr =
            boost::endian::little_to_native(cachedBufferHeader.bmcReadPtr);
    }
    if (currentBiosWritePtr != currentReadPtr)
    {
        throw std::runtime_error(fmt::format(
            "[readErrorLogs] biosWritePtr '{}' and bmcReaddPtr '{}' "
            "are not identical after reading through all the logs",
            currentBiosWritePtr, currentReadPtr));
    }
    return entryPairs;
}

} // namespace bios_bmc_smm_error_logger

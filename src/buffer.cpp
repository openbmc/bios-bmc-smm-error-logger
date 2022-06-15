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
    // Initialize the whole buffer with 0x00
    const size_t memoryRegionSize = dataInterface->getMemoryRegionSize();
    const std::vector<uint8_t> emptyVector(memoryRegionSize, 0);
    size_t byteWritten = dataInterface->write(0, emptyVector);
    if (byteWritten != memoryRegionSize)
    {
        throw std::runtime_error(
            fmt::format("Buffer initialization only erased '{}'", byteWritten));
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
            "Buffer initialization buffer header write only wrote '{}'",
            byteWritten));
    }
    cachedBufferHeader = initializationHeader;
}

void BufferImpl::readBufferHeader()
{
    size_t headerSize = sizeof(struct CircularBufferHeader);
    std::vector<uint8_t> bytesRead =
        dataInterface->read(/* offset */ 0, headerSize);

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

} // namespace bios_bmc_smm_error_logger

#include "buffer.hpp"

#include "pci_handler.hpp"

#include <fmt/format.h>

#include <array>
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
    initializationHeader.bmcInterfaceVersion = bmcInterfaceVersion;
    initializationHeader.queueSize = queueSize;
    initializationHeader.ueRegionSize = ueRegionSize;
    initializationHeader.magicNumber = magicNumber;

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
}

} // namespace bios_bmc_smm_error_logger

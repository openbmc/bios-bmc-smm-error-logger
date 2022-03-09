#include "pci_handler.hpp"

#include <fcntl.h>

#include <stdplus/fd/managed.hpp>
#include <stdplus/fd/mmap.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace biosBmcSmmErrorLogger
{

PciDataHandler::PciDataHandler(uint32_t regionAddress, size_t regionSize,
                               std::unique_ptr<stdplus::fd::Fd> fd) :
    regionAddress(regionAddress),
    regionSize(regionSize), fd(std::move(fd)),
    mmap(stdplus::fd::MMap(
        *this->fd, regionSize, stdplus::fd::ProtFlags{PROT_READ | PROT_WRITE},
        stdplus::fd::MMapFlags{stdplus::fd::MMapAccess::Shared}, regionAddress))
{}

std::vector<uint8_t> PciDataHandler::read(const uint32_t offset,
                                          const uint32_t length)
{
    if (length > regionSize || length == 0 || offset > regionSize)
    {
        return {};
    }

    // Read up to regionSize in case the offset + length overflowed
    uint32_t finalLength =
        (offset + length < regionSize) ? length : regionSize - offset;
    std::vector<uint8_t> results(finalLength);

    std::memcpy(results.data(), mmap.get().data() + offset, finalLength);
    return results;
}

uint32_t PciDataHandler::write(const uint32_t offset,
                               const std::span<const uint8_t> bytes)
{
    if (offset > regionSize)
    {
        return 0;
    }

    const size_t length = bytes.size();
    if (length > regionSize || length == 0)
    {
        return 0;
    }

    // Write up to regionSize in case the offset + length overflowed
    uint16_t finalLength =
        (offset + length < regionSize) ? length : regionSize - offset;
    std::memcpy(mmap.get().data() + offset, bytes.data(), finalLength);
    return finalLength;
}

uint32_t PciDataHandler::getMemoryRegionSize()
{
    return regionSize;
}

} // namespace biosBmcSmmErrorLogger

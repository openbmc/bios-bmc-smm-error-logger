#include "pci_handler.hpp"

#include "internal/sys.hpp"

#include <fcntl.h>
#include <sys/mman.h>

#include <stdplus/handle/managed.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace biosBmcSmmErrorLogger
{

PciDataHandler::PciDataHandler(uint32_t regionAddress, size_t regionSize,
                               const internal::Sys* sys) :
    regionAddress(regionAddress),
    memoryRegionSize(regionSize), sys(sys)
{}

bool PciDataHandler::open()
{
    static constexpr auto devmem = "/dev/mem";

    Fd fd(sys->open(devmem, O_RDWR | O_SYNC), sys);
    if (*fd < 0)
    {
        mappedFd = -1;

        std::fprintf(stderr,
                     "PciDataHandler::Unable to open /dev/mem with errno: %i",
                     *fd);
        // No need to close when it wasn't opened - release the RAII object
        (void)fd.release();
        return false;
    }

    mapped = reinterpret_cast<uint8_t*>(
        sys->mmap(nullptr, memoryRegionSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                  *fd, regionAddress));
    if (mapped == MAP_FAILED)
    {
        mappedFd = -1;
        mapped = nullptr;

        std::fprintf(stderr, "PciDataHandler::Unable to map region");
        // RAII object Fd will close the fd automatically
        return false;
    }

    // Release the Managed object so that the destructor isn't called
    mappedFd = fd.release();
    return true;
}

std::vector<uint8_t> PciDataHandler::read(const uint32_t offset,
                                          const uint32_t length)
{
    if (length > memoryRegionSize || length == 0)
    {
        return std::vector<uint8_t>{};
    }
    if (offset > memoryRegionSize)
    {
        return std::vector<uint8_t>{};
    }

    // Read up to memoryRegionSize in case the offset + length overflowed
    uint32_t finalLength = (offset + length < memoryRegionSize)
                               ? length
                               : memoryRegionSize - offset;
    std::vector<uint8_t> results(finalLength);

    std::memcpy(results.data(), mapped + offset, finalLength);
    return results;
}

uint32_t PciDataHandler::write(const uint32_t offset,
                               const std::span<const uint8_t> bytes)
{
    if (offset > memoryRegionSize)
    {
        return 0;
    }

    const size_t length = bytes.size();
    if (length > memoryRegionSize || length == 0)
    {
        return 0;
    }

    // Write up to memoryRegionSize in case the offset + length overflowed
    uint16_t finalLength = (offset + length < memoryRegionSize)
                               ? length
                               : memoryRegionSize - offset;
    std::memcpy(mapped + offset, bytes.data(), finalLength);
    return finalLength;
}

void PciDataHandler::close()
{
    if (mapped)
    {
        sys->munmap(mapped, memoryRegionSize);
        mapped = nullptr;
    }

    if (mappedFd != -1)
    {
        sys->close(mappedFd);
        mappedFd = -1;
    }
}

} // namespace biosBmcSmmErrorLogger

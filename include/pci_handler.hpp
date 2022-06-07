#pragma once

#include "data_interface.hpp"

#include <stdplus/fd/managed.hpp>
#include <stdplus/fd/mmap.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace bios_bmc_smm_error_logger
{

/**
 * Data handler for reading and writing data via the PCI bridge.
 *
 */
class PciDataHandler : public DataInterface
{
  public:
    explicit PciDataHandler(uint32_t regionAddress, size_t regionSize,
                            std::unique_ptr<stdplus::fd::Fd> fd);

    std::vector<uint8_t> read(uint32_t offset, uint32_t length) override;
    uint32_t write(const uint32_t offset,
                   const std::span<const uint8_t> bytes) override;
    uint32_t getMemoryRegionSize() override;

  private:
    uint32_t regionAddress;
    uint32_t regionSize;

    std::unique_ptr<stdplus::fd::Fd> fd;
    stdplus::fd::MMap mmap;
};

} // namespace bios_bmc_smm_error_logger

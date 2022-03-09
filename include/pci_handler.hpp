#pragma once

#include <stdplus/fd/managed.hpp>
#include <stdplus/fd/mmap.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace biosBmcSmmErrorLogger
{

/**
 * Each data transport mechanism must implement the DataInterface.
 */
class DataInterface
{
  public:
    virtual ~DataInterface() = default;

    /**
     * Read bytes from shared buffer (blocking call).
     *
     * @param[in] offset - offset to read from
     * @param[in] length - number of bytes to read
     * @return the bytes read
     */
    virtual std::vector<uint8_t> read(const uint32_t offset,
                                      const uint32_t length) = 0;

    /**
     * Write bytes to shared buffer.
     *
     * @param[in] offset - offset to write to
     * @param[in] bytes - byte vector of data.
     * @return return the byte length written
     */
    virtual uint32_t write(const uint32_t offset,
                           const std::span<const uint8_t> bytes) = 0;

    /**
     * Getter for Memory Region Size
     *
     * @return return Memory Region size allocated
     */
    virtual uint32_t getMemoryRegionSize() = 0;
};

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

} // namespace biosBmcSmmErrorLogger

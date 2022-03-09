#pragma once

#include "internal/sys.hpp"

#include <stdplus/handle/managed.hpp>

#include <cstdint>
#include <span>
#include <string>
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
     * Initialize data transport mechanism.  Calling this should be idempotent
     * if possible.
     *
     * @return true if successful
     */
    virtual bool open() = 0;

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
     * Close the data transport mechanism.
     */
    virtual void close() = 0;
};

/**
 * Data handler for reading and writing data via the PCI bridge.
 *
 */
class PciDataHandler : public DataInterface
{
  public:
    PciDataHandler(uint32_t regionAddress, size_t regionSize,
                   const internal::Sys* sys = &internal::sys_impl);

    bool open() override;
    std::vector<uint8_t> read(uint32_t offset, uint32_t length) override;
    uint32_t write(const uint32_t offset,
                   const std::span<const uint8_t> bytes) override;
    void close() override;

  private:
    /** @brief RAII wrapper and its destructor for opening a file descriptor */
    static void closeFd(int&& fd, const internal::Sys*& sys)
    {
        sys->close(fd);
    }
    using Fd = stdplus::Managed<int, const internal::Sys*>::Handle<closeFd>;

    uint32_t regionAddress;
    uint32_t memoryRegionSize;
    const internal::Sys* sys;

    int mappedFd = -1;
    uint8_t* mapped = nullptr;
};

} // namespace biosBmcSmmErrorLogger

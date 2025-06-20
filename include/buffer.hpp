#pragma once

#include "pci_handler.hpp"

#include <boost/endian/arithmetic.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <tuple>

namespace bios_bmc_smm_error_logger
{

/* Adding endianness */
using boost::endian::little_uint16_t;
using boost::endian::little_uint24_t;
using boost::endian::little_uint32_t;
using boost::endian::little_uint64_t;

// EntryPair.first = QueueEntryHeader
// EntryPair.second = Error entry in vector of bytes
using EntryPair = std::pair<struct QueueEntryHeader, std::vector<uint8_t>>;

enum class BufferFlags : uint32_t
{
    ueSwitch = 1 << 0,
    overflow = 1 << 1,
};

enum class BmcFlags : uint32_t
{
    ready = 1 << 2,
};

struct CircularBufferHeader
{
    little_uint32_t bmcInterfaceVersion;        // Offset 0x0
    little_uint32_t biosInterfaceVersion;       // Offset 0x4
    std::array<little_uint32_t, 4> magicNumber; // Offset 0x8
    little_uint24_t queueSize;                  // Offset 0x18
    little_uint16_t ueRegionSize;               // Offset 0x1b
    little_uint32_t bmcFlags;                   // Offset 0x1d
    little_uint24_t bmcReadPtr;                 // Offset 0x21
    std::array<uint8_t, 4> reserved1;           // Offset 0x24
    little_uint32_t biosFlags;                  // Offset 0x28
    little_uint24_t biosWritePtr;               // Offset 0x2c
    uint8_t reserved2;                          // Offset 0x2f
    // UE reserved region:                         Offset 0x30
    // Error log queue:       Offset 0x30 + UE reserved region

    bool operator==(const CircularBufferHeader& other) const
    {
        /* Skip comparing reserved1 and reserved2 */
        return std::tie(this->bmcInterfaceVersion, this->biosInterfaceVersion,
                        this->magicNumber, this->queueSize, this->ueRegionSize,
                        this->bmcFlags, this->bmcReadPtr, this->biosFlags,
                        this->biosWritePtr) ==
               std::tie(other.bmcInterfaceVersion, other.biosInterfaceVersion,
                        other.magicNumber, other.queueSize, other.ueRegionSize,
                        other.bmcFlags, other.bmcReadPtr, other.biosFlags,
                        other.biosWritePtr);
    }
};
static_assert(sizeof(CircularBufferHeader) == 0x30,
              "Size of CircularBufferHeader struct is incorrect.");

struct QueueEntryHeader
{
    little_uint16_t sequenceId; // Offset 0x0
    little_uint16_t entrySize;  // Offset 0x2
    uint8_t checksum;           // Offset 0x4
    uint8_t rdeCommandType;     // Offset 0x5
    // RDE Command                 Offset 0x6
    bool operator==(const QueueEntryHeader& other) const
    {
        return std::tie(this->sequenceId, this->entrySize, this->checksum,
                        this->rdeCommandType) ==
               std::tie(other.sequenceId, other.entrySize, other.checksum,
                        other.rdeCommandType);
    }
};
static_assert(sizeof(QueueEntryHeader) == 0x6,
              "Size of QueueEntryHeader struct is incorrect.");

/**
 * An interface class for the buffer helper APIs
 */
class BufferInterface
{
  public:
    virtual ~BufferInterface() = default;

    /**
     * Zero out the buffer first before populating the header
     *
     * @param[in] bmcInterfaceVersion - Used to initialize the header
     * @param[in] queueSize - Used to initialize the header
     * @param[in] ueRegionSize - Used to initialize the header
     * @param[in] magicNumber - Used to initialize the header
     * @return true if successful
     */
    virtual void initialize(uint32_t bmcInterfaceVersion, uint16_t queueSize,
                            uint16_t ueRegionSize,
                            const std::array<uint32_t, 4>& magicNumber) = 0;
    /**
     * Check for unread Uncorrecatble Error (UE) logs and read them if present
     */
    virtual std::vector<uint8_t> readUeLogFromReservedRegion() = 0;

    /**
     * Check for overflow and ackknolwedge if not acked yet
     */
    virtual bool checkForOverflowAndAcknowledge() = 0;

    /**
     * Read the buffer header from shared buffer
     */
    virtual void readBufferHeader() = 0;

    /**
     * Getter API for the cached buffer header
     * @return cached CircularBufferHeader
     */
    virtual struct CircularBufferHeader getCachedBufferHeader() const = 0;

    /**
     * Write to the bufferHeader and update the read pointer
     * @param[in] newReadPtr - read pointer to update to
     */
    virtual void updateReadPtr(const uint32_t newReadPtr) = 0;

    /**
     * Write to the bufferHeader and update the BMC flags
     * @param[in] newBmcFlags - new flag to update to
     */
    virtual void updateBmcFlags(const uint32_t newBmcFlags) = 0;

    /**
     * Wrapper for the dataInterface->read, performs wraparound read
     *
     * @param[in] relativeOffset - offset relative the "Error Log
     *  Queue region" = (sizeof(CircularBufferHeader) + UE reserved region)
     * @param[in] length - bytes to read
     * @return the bytes read
     */
    virtual std::vector<uint8_t> wraparoundRead(const uint32_t relativeOffset,
                                                const uint32_t length) = 0;
    /**
     * Read the entry header from shared buffer from the read pointer
     *
     * @return the entry header
     */
    virtual struct QueueEntryHeader readEntryHeader() = 0;

    /**
     * Read the queue entry from the error log queue from the read pointer
     *
     * * @return entry header and entry pair read from buffer
     */
    virtual EntryPair readEntry() = 0;

    /**
     * Read the buffer - this API should be used instead of individual functions
     * above
     *
     * @return vector of EntryPair which consists of entry header and entry
     */
    virtual std::vector<EntryPair> readErrorLogs() = 0;

    /**
     * Get max offset for the queue
     *
     * * @return Queue size - UE region size - Queue header size
     */
    virtual size_t getMaxOffset() = 0;

    /** @brief The Error log queue starts after the UE region, which is where
     * the read and write pointers are offset from relatively
     *  @return relative offset for read and write pointers
     */
    virtual size_t getQueueOffset() = 0;
};

/**
 * Buffer implementation class
 */
class BufferImpl : public BufferInterface
{
  public:
    /** @brief Constructor for BufferImpl
     *  @param[in] dataInterface     - DataInterface for this object
     */
    explicit BufferImpl(std::unique_ptr<DataInterface> dataInterface);
    void initialize(uint32_t bmcInterfaceVersion, uint16_t queueSize,
                    uint16_t ueRegionSize,
                    const std::array<uint32_t, 4>& magicNumber) override;
    std::vector<uint8_t> readUeLogFromReservedRegion() override;
    bool checkForOverflowAndAcknowledge() override;
    void readBufferHeader() override;
    struct CircularBufferHeader getCachedBufferHeader() const override;
    void updateReadPtr(const uint32_t newReadPtr) override;
    void updateBmcFlags(const uint32_t newBmcFlag) override;
    std::vector<uint8_t> wraparoundRead(const uint32_t relativeOffset,
                                        const uint32_t length) override;
    struct QueueEntryHeader readEntryHeader() override;
    EntryPair readEntry() override;
    std::vector<EntryPair> readErrorLogs() override;
    size_t getMaxOffset() override;
    size_t getQueueOffset() override;

  private:
    /** @brief Calculate the checksum by XOR each bytes in the span
     *  @param[in] entry     - Span to calculate the checksum on
     *  @return calculated checksum
     */
    uint8_t calculateChecksum(std::span<uint8_t> entry);

    std::unique_ptr<DataInterface> dataInterface;
    struct CircularBufferHeader cachedBufferHeader = {};
};

} // namespace bios_bmc_smm_error_logger

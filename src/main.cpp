#include "config.h"

#include "buffer.hpp"
#include "pci_handler.hpp"
#include "rde/external_storer_file.hpp"
#include "rde/external_storer_interface.hpp"
#include "rde/rde_handler.hpp"

#include <fmt/format.h>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/endian/conversion.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/impl.hpp>
#include <stdplus/fd/managed.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

namespace
{
constexpr std::chrono::milliseconds readIntervalinMs(READ_INTERVAL_MS);
constexpr std::size_t memoryRegionSize = MEMORY_REGION_SIZE;
constexpr std::size_t memoryRegionOffset = MEMORY_REGION_OFFSET;
// TODO: Hard code the initialization values for now, get from json later
constexpr uint32_t bmcInterfaceVersion = BMC_INTERFACE_VERSION;
constexpr uint16_t queueSize = QUEUE_REGION_SIZE;
constexpr uint16_t ueRegionSize = UE_REGION_SIZE;
static constexpr std::array<uint32_t, 4> magicNumber = {
    MAGIC_NUMBER_BYTE1, MAGIC_NUMBER_BYTE2, MAGIC_NUMBER_BYTE3,
    MAGIC_NUMBER_BYTE4};
} // namespace

using namespace bios_bmc_smm_error_logger;
using BufferAndRdeCommandHandler =
    std::pair<std::shared_ptr<BufferInterface>,
              std::shared_ptr<rde::RdeCommandHandler>>;

void readLoop(boost::asio::steady_timer* t,
              BufferAndRdeCommandHandler* bufferAndRdeCommandHandler)
{
    std::vector<EntryPair> entryPairs =
        bufferAndRdeCommandHandler->first->readErrorLogs();
    for (auto& entryPair : entryPairs)
    {
        fmt::print(stderr, "Read an entry of '{}' bytes\n",
                   entryPair.second.size());

        rde::RdeDecodeStatus rdeDecodeStatus =
            bufferAndRdeCommandHandler->second->decodeRdeCommand(
                entryPair.second, static_cast<rde::RdeCommandType>(
                                      entryPair.first.rdeCommandType));
        if (rdeDecodeStatus == rde::RdeDecodeStatus::RdeStopFlagReceived)
        {
            auto bufferHeader =
                bufferAndRdeCommandHandler->first->getCachedBufferHeader();
            auto newbmcFlags =
                boost::endian::little_to_native(bufferHeader.bmcFlags) |
                static_cast<uint32_t>(BmcFlags::ready);
            bufferAndRdeCommandHandler->first->updateBmcFlags(newbmcFlags);
        }
    }

    t->expires_from_now(readIntervalinMs);
    t->async_wait(boost::bind(readLoop, t, bufferAndRdeCommandHandler));
}

int main()
{
    boost::asio::io_context io;
    boost::asio::steady_timer t(io, readIntervalinMs);

    // bufferHandler initialization
    std::unique_ptr<stdplus::ManagedFd> managedFd =
        std::make_unique<stdplus::ManagedFd>(stdplus::fd::open(
            "/dev/mem",
            stdplus::fd::OpenFlags(stdplus::fd::OpenAccess::ReadWrite)
                .set(stdplus::fd::OpenFlag::Sync)));
    std::unique_ptr<DataInterface> pciDataHandler =
        std::make_unique<PciDataHandler>(memoryRegionOffset, memoryRegionSize,
                                         std::move(managedFd));
    std::shared_ptr<BufferInterface> bufferHandler =
        std::make_shared<BufferImpl>(std::move(pciDataHandler));

    // rdeCommandHandler initialization
    std::unique_ptr<rde::FileHandlerInterface> fileIface =
        std::make_unique<rde::ExternalStorerFileWriter>();
    std::unique_ptr<rde::ExternalStorerInterface> exFileIface =
        std::make_unique<rde::ExternalStorerFileInterface>(
            "/run/bmcweb", std::move(fileIface));
    std::shared_ptr<rde::RdeCommandHandler> rdeCommandHandler =
        std::make_unique<rde::RdeCommandHandler>(std::move(exFileIface));

    bufferHandler->initialize(bmcInterfaceVersion, queueSize, ueRegionSize,
                              magicNumber);

    BufferAndRdeCommandHandler bufferAndRdeCommandHandler{
        std::move(bufferHandler), std::move(rdeCommandHandler)};
    t.async_wait(boost::bind(readLoop, &t, &bufferAndRdeCommandHandler));

    io.run();

    return 0;
}

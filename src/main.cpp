#include "config.h"

#include "buffer.hpp"
#include "dbus/file_notifier.hpp"
#include "pci_handler.hpp"
#include "rde/external_storer_file.hpp"
#include "rde/external_storer_interface.hpp"
#include "rde/rde_handler.hpp"

#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/impl.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/print.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>

namespace
{
constexpr std::chrono::milliseconds readIntervalinMs(READ_INTERVAL_MS);
constexpr std::size_t memoryRegionSize = MEMORY_REGION_SIZE;
constexpr std::size_t memoryRegionOffset = MEMORY_REGION_OFFSET;
constexpr uint32_t bmcInterfaceVersion = BMC_INTERFACE_VERSION;
constexpr uint16_t queueSize = QUEUE_REGION_SIZE;
constexpr uint16_t ueRegionSize = UE_REGION_SIZE;
static constexpr std::array<uint32_t, 4> magicNumber = {
    MAGIC_NUMBER_BYTE1, MAGIC_NUMBER_BYTE2, MAGIC_NUMBER_BYTE3,
    MAGIC_NUMBER_BYTE4};
} // namespace

using namespace bios_bmc_smm_error_logger;

void readLoop(boost::asio::steady_timer* t,
              const std::shared_ptr<BufferInterface>& bufferInterface,
              const std::shared_ptr<rde::RdeCommandHandler>& rdeCommandHandler,
              const boost::system::error_code& error)
{
    if (error)
    {
        stdplus::print(stderr, "Async wait failed {}\n", error.message());
        return;
    }
    std::vector<EntryPair> entryPairs = bufferInterface->readErrorLogs();
    for (const auto& [entryHeader, entry] : entryPairs)
    {
        stdplus::print(stderr, "Read an entry of '{}' bytes\n", entry.size());

        rde::RdeDecodeStatus rdeDecodeStatus =
            rdeCommandHandler->decodeRdeCommand(
                entry,
                static_cast<rde::RdeCommandType>(entryHeader.rdeCommandType));
        if (rdeDecodeStatus == rde::RdeDecodeStatus::RdeStopFlagReceived)
        {
            auto bufferHeader = bufferInterface->getCachedBufferHeader();
            auto newbmcFlags =
                boost::endian::little_to_native(bufferHeader.bmcFlags) |
                static_cast<uint32_t>(BmcFlags::ready);
            bufferInterface->updateBmcFlags(newbmcFlags);
        }
    }

    t->expires_from_now(readIntervalinMs);
    t->async_wait(
        std::bind_front(readLoop, t, bufferInterface, rdeCommandHandler));
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
    std::shared_ptr<sdbusplus::asio::connection> conn =
        std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name("xyz.openbmc_project.bios_bmc_smm_error_logger");
    sdbusplus::bus_t& bus = static_cast<sdbusplus::bus_t&>(*conn);
    sdbusplus::server::manager_t manager(bus,
                                         rde::CperFileNotifier::cperBasePath);

    std::unique_ptr<rde::FileHandlerInterface> fileIface =
        std::make_unique<rde::ExternalStorerFileWriter>();
    std::unique_ptr<rde::ExternalStorerInterface> exFileIface =
        std::make_unique<rde::ExternalStorerFileInterface>(
            bus, "/run/bmcweb", std::move(fileIface));
    std::shared_ptr<rde::RdeCommandHandler> rdeCommandHandler =
        std::make_unique<rde::RdeCommandHandler>(std::move(exFileIface));

    bufferHandler->initialize(bmcInterfaceVersion, queueSize, ueRegionSize,
                              magicNumber);

    t.async_wait(std::bind_front(readLoop, &t, std::move(bufferHandler),
                                 std::move(rdeCommandHandler)));
    io.run();

    return 0;
}

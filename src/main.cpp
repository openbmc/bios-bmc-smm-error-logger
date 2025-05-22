#include "config.h"

#include "buffer.hpp"
#include "pci_handler.hpp"
#include "rde/external_storer_file.hpp"
#include "rde/external_storer_interface.hpp"
#include "rde/rde_handler.hpp"
#include "read_loop.hpp"

#include <boost/asio.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <chrono>
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
static constexpr std::array<uint32_t, 4> magicNumber =
{
    MAGIC_NUMBER_BYTE1, MAGIC_NUMBER_BYTE2, MAGIC_NUMBER_BYTE3,
    MAGIC_NUMBER_BYTE4
};
} // namespace

using namespace bios_bmc_smm_error_logger;

int main()
{
    boost::asio::io_context io;
    boost::asio::steady_timer t(io, readIntervalinMs);

    // bufferHandler initialization
    std::unique_ptr<stdplus::ManagedFd> managedFd =
        std::make_unique<stdplus::ManagedFd> (stdplus::fd::open(
                "/dev/mem",
                stdplus::fd::OpenFlags(stdplus::fd::OpenAccess::ReadWrite)
                .set(stdplus::fd::OpenFlag::Sync)));
    std::unique_ptr<DataInterface> pciDataHandler =
        std::make_unique<PciDataHandler> (memoryRegionOffset, memoryRegionSize,
                                          std::move(managedFd));
    std::shared_ptr<BufferInterface> bufferHandler =
        std::make_shared<BufferImpl> (std::move(pciDataHandler));

    // rdeCommandHandler initialization
    std::shared_ptr<sdbusplus::asio::connection> conn =
        std::make_shared<sdbusplus::asio::connection> (io);
    conn->request_name("xyz.openbmc_project.bios_bmc_smm_error_logger");

    std::unique_ptr<rde::FileHandlerInterface> fileIface =
        std::make_unique<rde::ExternalStorerFileWriter>();
    std::unique_ptr<rde::ExternalStorerInterface> exFileIface =
        std::make_unique<rde::ExternalStorerFileInterface> (
            conn, "/run/bmcweb", std::move(fileIface));
    std::shared_ptr<rde::RdeCommandHandler> rdeCommandHandler =
        std::make_unique<rde::RdeCommandHandler> (std::move(exFileIface));

    bufferHandler->initialize(bmcInterfaceVersion, queueSize, ueRegionSize,
                              magicNumber);

    t.async_wait(std::bind_front(readLoop, &t, std::move(bufferHandler),
                                 std::move(rdeCommandHandler)));
    io.run();

    return 0;
}

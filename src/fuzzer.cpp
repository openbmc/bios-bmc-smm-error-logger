#include "config.h"

#include "buffer.hpp"
#include "pci_handler.hpp"
#include "rde/external_storer_file.hpp"
#include "rde/external_storer_interface.hpp"
#include "rde/rde_handler.hpp"
#include "read_loop.hpp"

#include <boost/asio.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <chrono>
#include <memory>

#include <iostream>
#include <vector>
#include <algorithm>

using namespace bios_bmc_smm_error_logger;

namespace
{
constexpr std::chrono::milliseconds readIntervalinMs(READ_INTERVAL_MS);
constexpr std::size_t memoryRegionSize = MEMORY_REGION_SIZE;
constexpr uint32_t bmcInterfaceVersion = BMC_INTERFACE_VERSION;
constexpr uint16_t queueSize = QUEUE_REGION_SIZE;
constexpr uint16_t ueRegionSize = UE_REGION_SIZE;
static constexpr std::array<uint32_t, 4> magicNumber =
{
    MAGIC_NUMBER_BYTE1, MAGIC_NUMBER_BYTE2, MAGIC_NUMBER_BYTE3,
    MAGIC_NUMBER_BYTE4
};

// Global state variables
static bool isInitialized = false;
static std::vector<uint8_t> buffer;
static std::shared_ptr<BufferInterface> bufferHandler;
static std::shared_ptr<rde::RdeCommandHandler> rdeCommandHandler;
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    if (!isInitialized)
    {
        buffer.resize(memoryRegionSize);

        boost::asio::io_context io;
        boost::asio::steady_timer t(io, readIntervalinMs);    // ignored for fuzzing

        // bufferHandler initialization
        std::unique_ptr<DataInterface> pciDataHandler =
            std::make_unique<PciDataHandler> (buffer.data(), memoryRegionSize);
        bufferHandler = std::make_shared<BufferImpl> (std::move(pciDataHandler));

        // rdeCommandHandler initialization
        std::shared_ptr<sdbusplus::asio::connection> conn =
            std::make_shared<sdbusplus::asio::connection> (io);
        conn->request_name("xyz.openbmc_project.bios_bmc_smm_error_logger");

        std::unique_ptr<rde::FileHandlerInterface> fileIface =
            std::make_unique<rde::ExternalStorerFileWriter>();
        std::unique_ptr<rde::ExternalStorerInterface> exFileIface =
            std::make_unique<rde::ExternalStorerFileInterface> (
                conn, "/run/bmcweb", std::move(fileIface));

        rdeCommandHandler =
            std::make_unique<rde::RdeCommandHandler> (std::move(exFileIface));

        bufferHandler->initialize(bmcInterfaceVersion, queueSize, ueRegionSize,
                                  magicNumber);
        isInitialized = true;
    }

    std::fill(buffer.begin(), buffer.end(), 0);
    std::copy(Data, Data + std::min(Size, buffer.size()), buffer.begin());

    try
    {
        boost::system::error_code ec;
        bios_bmc_smm_error_logger::readLoop(
            static_cast<boost::asio::basic_waitable_timer<std::chrono::_V2::steady_clock>*>
            (nullptr),
            bufferHandler,
            rdeCommandHandler,
            ec
        );
    }
    catch (const std::runtime_error& e)
    {
        std::cout << "Caught std::runtime_error (ignored by fuzzer): " << e.what() <<
                  std::endl;
    }
    return 0;
}

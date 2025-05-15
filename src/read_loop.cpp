#include "config.h"

#include "buffer.hpp"
#include "rde/rde_handler.hpp"

#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <stdplus/print.hpp>

#include <chrono>
#include <functional>
#include <memory>


namespace bios_bmc_smm_error_logger {
    constexpr std::chrono::milliseconds readIntervalinMs(READ_INTERVAL_MS);
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

        if (t != nullptr) {
            t->expires_from_now(readIntervalinMs);
            t->async_wait(
            std::bind_front(readLoop, t, bufferInterface, rdeCommandHandler));
        }
    }
}

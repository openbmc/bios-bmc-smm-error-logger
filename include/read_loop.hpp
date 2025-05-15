#pragma once  // Ensures the file is only included once per translation unit

#include "buffer.hpp"
#include "rde/rde_handler.hpp"

#include <boost/asio.hpp>

#include <memory>

namespace bios_bmc_smm_error_logger {
    void readLoop(boost::asio::steady_timer* t,
        const std::shared_ptr<BufferInterface>& bufferInterface,
        const std::shared_ptr<rde::RdeCommandHandler>& rdeCommandHandler,
        const boost::system::error_code& error);
}

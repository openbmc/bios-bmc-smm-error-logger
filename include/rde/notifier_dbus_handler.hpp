#pragma once

#include "dbus/file_notifier.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <vector>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief A class to handle CPER DBus notification objects.
 */
class CperFileNotifierHandler
{
  public:
    /**
     * @brief Constructor for the CperFileNotifierHandler class.
     *
     * @param conn - sdbusplus asio connection.
     */
    explicit CperFileNotifierHandler(
        const std::shared_ptr<sdbusplus::asio::connection>& conn);

    /**
     * @brief Create a DBus object with the provided filePath value.
     *
     * @param filePath - file path of the CPER log JSON file.
     */
    void createEntry(const std::string& filePath);

  private:
    sdbusplus::server::manager_t objManager;
    sdbusplus::asio::object_server objServer;

    /**
     * @brief DBus index of the next entry.
     */
    uint64_t nextEntry = 0;
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

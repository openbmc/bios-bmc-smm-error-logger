#pragma once

#include "dbus/file_notifier.hpp"

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
     * @param bus - bus to attache to.
     */
    explicit CperFileNotifierHandler(sdbusplus::bus_t& bus);

    /**
     * @brief Destructor for the CperFileNotifierHandler class.
     */
    ~CperFileNotifierHandler();

    /**
     * @brief Create a DBus object with the provided filePath value.
     *
     * @param filePath - file path of the CPER log JSON file.
     */
    void createEntry(const std::string& filePath);

  private:
    sdbusplus::bus_t& bus;
    sdbusplus::server::manager_t objManager;

    /**
     * @brief A vector to keep track of DBus FilePath objects.
     */
    std::vector<std::unique_ptr<CperFileNotifier>> notifierObjs;

    /**
     * @brief DBus index of the next entry.
     */
    uint64_t nextEntry = 0;
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

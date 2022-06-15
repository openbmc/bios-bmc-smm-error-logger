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
    explicit CperFileNotifierHandler(sdbusplus::bus::bus& bus);

    /**
     * @brief Create a DBus object with the provided filePath value.
     *
     * @param filePath - file path of the CPER log JSON file.
     */
    void createEntry(std::string filePath);

  private:
    sdbusplus::bus::bus& bus;

    /**
     * @brief A vector to keep track of DBus FilePath objects.
     */
    std::vector<std::unique_ptr<CperFileNotifier>> notifierObjs;

    /**
     * @brief DBus index of the next entry.
     */
    uint64_t nextEntry;
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

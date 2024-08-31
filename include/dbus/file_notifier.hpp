#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Common/FilePath/server.hpp>

#include <format>
#include <string>

namespace bios_bmc_smm_error_logger
{

using FileNotifierInterface = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;

/**
 * @brief A class for notifying file paths of CPER logs.
 */
class CperFileNotifier : public FileNotifierInterface
{
  public:
    /**
     * @brief Constructor for the CperFileNotifier class.
     *
     * @param bus - bus to attach to.
     * @param filePath - full path of the CPER log JSON file.
     * @param entry - index of the DBus file path object.
     */
    CperFileNotifier(sdbusplus::bus_t& bus, const std::string& filePath,
                     uint64_t entry) :
        FileNotifierInterface(bus, generatePath(entry).c_str())
    {
        // We only need the interface added signal for the fault monitor. So
        // stop emitting properties changed signal.
        path(filePath, /*skipSignal=*/true);
    }

    static constexpr const char* cperBasePath =
        "/xyz/openbmc_project/external_storer/bios_bmc_smm_error_logger/CPER";

  private:
    /**
     * @brief Generate a path for the CperFileNotifier DBus object.
     *
     * @param[in] entry - unique index for the DBus object.
     */
    std::string generatePath(uint64_t entry)
    {
        return std::format("{}/entry{}", cperBasePath, entry);
    }
};

} // namespace bios_bmc_smm_error_logger

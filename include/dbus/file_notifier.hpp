#pragma once

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/FilePath/server.hpp>

#include <format>
#include <string>

namespace bios_bmc_smm_error_logger
{

/**
 * @brief A class for notifying file paths of CPER logs.
 */
class CperFileNotifier
{
  public:
    /**
     * @brief Constructor for the CperFileNotifier class.
     *
     * @param server - sdbusplus asio object server.
     * @param filePath - full path of the CPER log JSON file.
     * @param entry - index of the DBus file path object.
     */
    CperFileNotifier(sdbusplus::asio::object_server& server,
                     const std::string& filePath, uint64_t entry) :
        server(server)
    {
        pathIface = server.add_interface(generatePath(entry).c_str(),
                                         "xyz.openbmc_project.Common.FilePath");
        pathIface->register_property("Path", filePath);
        pathIface->initialize();
    }

    ~CperFileNotifier()
    {
        server.remove_interface(pathIface);
    }

    CperFileNotifier& operator=(const CperFileNotifier&) = delete;
    CperFileNotifier& operator=(CperFileNotifier&&) = delete;
    CperFileNotifier(const CperFileNotifier&) = delete;
    CperFileNotifier(CperFileNotifier&&) = default;

    static constexpr const char* cperBasePath =
        "/xyz/openbmc_project/external_storer/bios_bmc_smm_error_logger/CPER";

  private:
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> pathIface;

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

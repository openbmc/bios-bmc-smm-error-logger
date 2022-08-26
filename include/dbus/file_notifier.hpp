#pragma once

#include <fmt/format.h>

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Common/FilePath/server.hpp>

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
<<<<<<< HEAD
    CperFileNotifier(sdbusplus::bus_t& bus, const std::string& filePath,
                     uint64_t entry) :
        FileNotifierInterface(bus, generatePath(entry).c_str(),
                              action::emit_no_signals),
        entry(entry), bus(bus)
=======
    CperFileNotifier(sdbusplus::asio::object_server& server,
                     const std::string& filePath, uint64_t entry) :
        entry(entry),
        server(server)
>>>>>>> 73ac4e2 (Modify DBus object used for notifying FaultMonitor)
    {
        // More Logging.Entry fields can be populated if its required. For the
        // moment only severity field is updated. This interface is mainly
        // added so that we can use FilePath interface alongside with this.
        const std::string& severity =
            "xyz.openbmc_project.Logging.Entry.Level.Informational";
        logEntryIface = server.add_interface(
            generatePath(entry).c_str(), "xyz.openbmc_project.Logging.Entry");
        logEntryIface->register_property("Severity", severity);
        logEntryIface->initialize();

        pathIface = server.add_interface(generatePath(entry).c_str(),
                                         "xyz.openbmc_project.Common.FilePath");
        pathIface->register_property("Path", filePath);
        pathIface->initialize();
    }

    ~CperFileNotifier()
    {
        server.remove_interface(pathIface);
        server.remove_interface(logEntryIface);
    }

    CperFileNotifier& operator=(const CperFileNotifier&) = delete;
    CperFileNotifier(const CperFileNotifier&) = delete;
    CperFileNotifier(CperFileNotifier&&) = default;
    CperFileNotifier& operator=(CperFileNotifier&&) = default;

    static constexpr const char* cperBasePath =
        "/xyz/openbmc_project/logging/host0/management";

  private:
    /**
     * @brief DBus index of the entry.
     */
    uint64_t entry;

<<<<<<< HEAD
    sdbusplus::bus_t& bus;
=======
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> logEntryIface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> pathIface;
>>>>>>> 73ac4e2 (Modify DBus object used for notifying FaultMonitor)

    /**
     * @brief Generate a path for the CperFileNotifier DBus object.
     *
     * @param[in] entry - unique index for the DBus object.
     */
    std::string generatePath(uint64_t entry)
    {
        return fmt::format("{}/entry{}", cperBasePath, entry);
    }
};

} // namespace bios_bmc_smm_error_logger

#include "rde/notifier_dbus_handler.hpp"

namespace bios_bmc_smm_error_logger
{
namespace rde
{

CperFileNotifierHandler::CperFileNotifierHandler(
    const std::shared_ptr<sdbusplus::asio::connection>& conn) :
    objManager(static_cast<sdbusplus::bus_t&>(*conn),
               CperFileNotifier::cperBasePath),
    objServer(conn)
{}

void CperFileNotifierHandler::createEntry(const std::string& filePath)
{
    auto obj =
        std::make_unique<CperFileNotifier>(objServer, filePath, nextEntry);
    ++nextEntry;
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

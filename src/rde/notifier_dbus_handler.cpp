#include "rde/notifier_dbus_handler.hpp"

namespace bios_bmc_smm_error_logger
{
namespace rde
{

CperFileNotifierHandler::CperFileNotifierHandler(sdbusplus::bus::bus& bus) :
    bus(bus), nextEntry(0)
{}

void CperFileNotifierHandler::createEntry(const std::string& filePath)
{
    notifierObjs.push_back(
        std::make_unique<CperFileNotifier>(bus, filePath, nextEntry));
    ++nextEntry;
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger
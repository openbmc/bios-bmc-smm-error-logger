#include "rde/notifier_dbus_handler.hpp"

namespace bios_bmc_smm_error_logger
{
namespace rde
{

CperFileNotifierHandler::CperFileNotifierHandler(sdbusplus::bus_t& bus) :
    bus(bus), objManager(bus, CperFileNotifier::cperBasePath)
{}

CperFileNotifier::~CperFileNotifier()
{
    // Clean up any objects from the bus explicitly
    for (auto obj : notifierObjs)
    {
        bus.remove_object(obj);
    }
}

void CperFileNotifierHandler::createEntry(const std::string& filePath)
{
    auto obj = std::make_unique<CperFileNotifier>(bus, filePath, nextEntry);
    // Notify fault log monitor through InterfacesAdded signal.
    obj->emit_added();
    notifierObjs.push_back(std::move(obj));
    ++nextEntry;
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

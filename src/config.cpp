#include "config.hpp"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace bios_bmc_smm_error_logger
{

Config createConfig(std::string_view configPath)
{
    Config config;

    // Create a string copy to ensure null-termination
    std::string configPathStr{configPath};
    std::ifstream configJson(configPathStr.c_str());
    if (!configJson.is_open())
    {
        throw std::runtime_error(
            fmt::format("Config file '{}' is missing", configPath));
    }

    const auto& data = nlohmann::json::parse(configJson, nullptr, false);
    if (data.is_discarded())
    {
        throw std::runtime_error("failed to parse the config.json");
    }

    const std::vector<std::string> magicNumberStringVector =
        data.at("MagicNumber");
    if (magicNumberStringVector.size() != config.magicNumber.size())
    {
        throw std::runtime_error(
            fmt::format("MagicNumber vector size [{}] is incorrect",
                        magicNumberStringVector.size()));
    }
    for (size_t i = 0; i < magicNumberStringVector.size(); i++)
    {
        config.magicNumber[i] =
            std::stoul(magicNumberStringVector[i], nullptr, 16);
    }

    data.at("BMCInterfaceVersion").get_to(config.bmcInterfaceVersion);
    data.at("QueueSizeBytes").get_to(config.queueSize);
    data.at("UERegionSizeBytes").get_to(config.ueRegionSize);
    data.at("MemoryRegionSize").get_to(config.memoryRegionSize);
    data.at("MemoryRegionOffset").get_to(config.memoryRegionOffset);
    config.pollintIntervalMS =
        std::chrono::milliseconds(data.at("PollingIntervalMS"));

    return config;
}

} // namespace bios_bmc_smm_error_logger

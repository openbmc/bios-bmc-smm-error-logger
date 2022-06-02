#pragma once

#include <array>
#include <chrono>
#include <string_view>

namespace bios_bmc_smm_error_logger
{

/** @struct Config
 *  @brief BIOS-BMC configuration populated from JSON
 */
struct Config
{
    uint32_t bmcInterfaceVersion;
    std::array<uint32_t, 4> magicNumber{};
    uint16_t queueSize;
    // UE refers to Uncorrectable Error
    uint16_t ueRegionSize;
    std::chrono::milliseconds pollintIntervalMS;
    size_t memoryRegionSize;
    size_t memoryRegionOffset;
};

/** @brief Parse JSON file to create the BIOS-BMC configuration
 *
 * @param[in] configPath  Path format to find the config file
 *
 * @return BIOS-BMC configuration
 */
Config createConfig(std::string_view configPath =
                        "/usr/share/bios-bmc-smm-error-logger/config.json");

} // namespace bios_bmc_smm_error_logger

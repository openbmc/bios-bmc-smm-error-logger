#pragma once
#include "data_interface.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <gmock/gmock.h>

namespace bios_bmc_smm_error_logger
{

class DataInterfaceMock : public DataInterface
{
  public:
    MOCK_METHOD(std::vector<uint8_t>, read,
                (const uint32_t offset, const uint32_t length), (override));
    MOCK_METHOD(uint32_t, write,
                (const uint32_t offset, const std::span<const uint8_t> bytes),
                (override));
    MOCK_METHOD(uint32_t, getMemoryRegionSize, (), (override));
};

} // namespace bios_bmc_smm_error_logger

#pragma once

#include "nlohmann/json.hpp"
#include "rde/external_storer_file.hpp"

#include <gmock/gmock.h>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

class MockFileWriter : public FileHandlerInterface
{
  public:
    MOCK_METHOD(bool, createFolder, (std::string_view path), (const, override));
    MOCK_METHOD(bool, createFile,
                (std::string_view path, nlohmann::json& jsonPdr),
                (const, override));
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger
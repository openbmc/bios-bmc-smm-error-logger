#pragma once

#include <string_view>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief Base class for publishing data to ExternalStorer.
 */
class ExternalStorerInterface
{
  public:
    virtual ~ExternalStorerInterface() = default;

    /**
     * @brief Publish JSON string to ExternalStorer.
     *
     * @param[in] jsonStr - a valid JSON string.
     * @return true if successful.
     */
    virtual bool publishJson(std::string_view jsonStr) = 0;

    /**
     * @brief Getter for the FileHandlerInterface
     *
     * @return FileHandlerInterface member
     */
    virtual FileHandlerInterface* getFileHandler() const = 0
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

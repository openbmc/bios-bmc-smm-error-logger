#pragma once

#include "external_storer_interface.hpp"
#include "nlohmann/json.hpp"
#include "notifier_dbus_handler.hpp"

#include <boost/uuid/uuid_generators.hpp>

#include <filesystem>
#include <queue>
#include <string>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief Simple Base class for writing JSON data to files.
 *
 * This class allows us to unit test the ExternalStorerFileInterface
 * functionality.
 */
class FileHandlerInterface
{
  public:
    virtual ~FileHandlerInterface() = default;

    /**
     * @brief Create a folder at the provided path.
     *
     * @param[in] folderPath - folder path.
     * @return true if successful.
     */
    virtual bool createFolder(const std::string& folderPath) const = 0;

    /**
     * @brief Create an index.json and write the JSON content to it.
     *
     * If the file already exists, this will overwrite it.
     *
     * @param[in] folderPath - path of the file without including the file name.
     * @param[in] jsonPdr - PDR in nlohmann::json format.
     * @return true if successful.
     */
    virtual bool createFile(const std::string& folderPath,
                            const nlohmann::json& jsonPdr) const = 0;

    /**
     * @brief Call remove_all on the filePath
     *
     * @param[in] filePath - path of the file/folder to remove
     * @return true if successful.
     */
    virtual bool removeAll(const std::string& filePath) const = 0;
};

/**
 * @brief Class for handling folder and file creation for ExternalStorer.
 */
class ExternalStorerFileWriter : public FileHandlerInterface
{
  public:
    bool createFolder(const std::string& folderPath) const override;
    bool createFile(const std::string& folderPath,
                    const nlohmann::json& jsonPdr) const override;
    bool removeAll(const std::string& filePath) const override;
};

/**
 * @brief Categories for different redfish JSON strings.
 */
enum class JsonPdrType
{
    logEntry,
    logService,
    other
};

/**
 * @brief Class for handling ExternalStorer file operations.
 */
class ExternalStorerFileInterface : public ExternalStorerInterface
{
  public:
    /**
     * @brief Constructor for the ExternalStorerFileInterface.
     *
     * @param[in] conn - sdbusplus asio connection.
     * @param[in] rootPath - root path for creating redfish folders.
     * Eg: "/run/bmcweb"
     * @param[in] fileHandler - an ExternalStorerFileWriter object. This class
     * will take the ownership of this object.
     * @param[in] numSavedLogEntries - first N number of log entries to be saved
     * in the queue (default shall be 20)
     * @param[in] numLogEntries - number of non-saved log entries in the queue
     * (default shall be 1000 - 20 = 980)
     */
    ExternalStorerFileInterface(
        const std::shared_ptr<sdbusplus::asio::connection>& conn,
        std::string_view rootPath,
        std::unique_ptr<FileHandlerInterface> fileHandler,
        uint32_t numSavedLogEntries = 20, uint32_t numLogEntries = 980);

    bool publishJson(std::string_view jsonStr) override;

  private:
    std::string rootPath;
    std::unique_ptr<FileHandlerInterface> fileHandler;
    std::string logServiceId;
    std::unique_ptr<CperFileNotifierHandler> cperNotifier;
    boost::uuids::random_generator randomGen;
    std::queue<std::string> logEntrySavedQueue;
    std::queue<std::string> logEntryQueue;
    // Default should be 20
    const uint32_t maxNumSavedLogEntries;
    // Default should be 1000 - maxNumSavedLogEntries(20) = 980
    const uint32_t maxNumLogEntries;

    /**
     * @brief Get the type of the received PDR.
     *
     * @param[in] jsonSchema - PDR in nlohmann::json format.
     * @return JsonPdrType of the PDR.
     */
    JsonPdrType getSchemaType(const nlohmann::json& jsonSchema) const;

    /**
     * @brief Process a LogEntry type PDR.
     *
     * @param[in] logEntry - PDR in nlohmann::json format.
     * @return true if successful.
     */
    bool processLogEntry(nlohmann::json& logEntry);

    /**
     * @brief Process a LogService type PDR.
     *
     * @param[in] logService - PDR in nlohmann::json format.
     * @return true if successful.
     */
    bool processLogService(const nlohmann::json& logService);

    /**
     * @brief Process PDRs that doesn't have a specific category.
     *
     * @param[in] jsonPdr - PDR in nlohmann::json format.
     * @return true if successful.
     */
    bool processOtherTypes(const nlohmann::json& jsonPdr) const;

    /**
     * @brief Create the needed folders and the index.json.
     *
     * @param subPath - path within the root folder.
     * @param jsonPdr - PDR in nlohmann::json format.
     * @return true if successful.
     */
    bool createFile(const std::string& subPath,
                    const nlohmann::json& jsonPdr) const;
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

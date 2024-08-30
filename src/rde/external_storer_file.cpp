#include "rde/external_storer_file.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <stdplus/print.hpp>

#include <format>
#include <fstream>
#include <string_view>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

bool ExternalStorerFileWriter::createFolder(const std::string& folderPath) const
{
    std::filesystem::path path(folderPath);
    if (!std::filesystem::is_directory(path))
    {
        if (!std::filesystem::create_directories(path))
        {
            stdplus::print(stderr, "Failed to create a folder at {}\n",
                           folderPath);
            return false;
        }
    }
    return true;
}

bool ExternalStorerFileWriter::createFile(const std::string& folderPath,
                                          const nlohmann::json& jsonPdr) const
{
    if (!createFolder(folderPath))
    {
        return false;
    }
    std::filesystem::path path(folderPath);
    path /= "index.json";
    // If the file already exist, overwrite it.
    std::ofstream output(path);
    output << jsonPdr;
    output.close();
    stdplus::print(stderr, "Created: {}\n", path.string());
    return true;
}

ExternalStorerFileInterface::ExternalStorerFileInterface(
    sdbusplus::bus_t& bus, std::string_view rootPath,
    std::unique_ptr<FileHandlerInterface> fileHandler) :
    rootPath(rootPath), fileHandler(std::move(fileHandler)), logServiceId(""),
    cperNotifier(std::make_unique<CperFileNotifierHandler>(bus))
{}

bool ExternalStorerFileInterface::publishJson(std::string_view jsonStr)
{
    nlohmann::json jsonDecoded;
    try
    {
        jsonDecoded = nlohmann::json::parse(jsonStr);
    }
    catch (nlohmann::json::parse_error& e)
    {
        stdplus::print(stderr, "JSON parse error: \n{}\n", e.what());
        return false;
    }

    // We need to know the type to determine how to process the decoded JSON
    // output.
    if (!jsonDecoded.contains("@odata.type"))
    {
        stdplus::print(stderr, "@odata.type field doesn't exist in:\n {}\n",
                       jsonDecoded.dump(4));
        return false;
    }

    auto schemaType = getSchemaType(jsonDecoded);
    if (schemaType == JsonPdrType::logEntry)
    {
        return processLogEntry(jsonDecoded);
    }
    if (schemaType == JsonPdrType::logService)
    {
        return processLogService(jsonDecoded);
    }
    return processOtherTypes(jsonDecoded);
}

JsonPdrType ExternalStorerFileInterface::getSchemaType(
    const nlohmann::json& jsonSchema) const
{
    auto logEntryFound =
        std::string(jsonSchema["@odata.type"]).find("LogEntry");
    if (logEntryFound != std::string::npos)
    {
        return JsonPdrType::logEntry;
    }

    auto logServiceFound =
        std::string(jsonSchema["@odata.type"]).find("LogService");
    if (logServiceFound != std::string::npos)
    {
        return JsonPdrType::logService;
    }

    return JsonPdrType::other;
}

bool ExternalStorerFileInterface::processLogEntry(nlohmann::json& logEntry)
{
    // TODO: Add policies for LogEntry retention.
    // https://github.com/openbmc/bios-bmc-smm-error-logger/issues/1.
    if (logServiceId.empty())
    {
        stdplus::print(stderr,
                       "First need a LogService PDR with a new UUID.\n");
        return false;
    }

    // Check to see if we are hitting the limit of filePathQueue, delete oldest
    // log entry first before processing another entry
    if (logEntryQueue.size() > MAX_NUM_LOG_ENTRIES)
    {
        std::string oldestFilePath = std::move(logEntryQueue.front());
        logEntryQueue.pop();

        // Attempt to delete the file
        if (!std::filesystem::remove_all(oldestFilePath))
        {
            stdplus::print(
                stderr,
                "Failed to delete the oldest entry path, not processing the next log: {}\n",
                oldestFilePath);
            return false;
        }
    }

    std::string id = boost::uuids::to_string(randomGen());
    std::string fullPath =
        std::format("{}/redfish/v1/Systems/system/LogServices/{}/Entries/{}",
                    rootPath, logServiceId, id);

    // Populate the "Id" with the UUID we generated.
    logEntry["Id"] = id;
    // Remove the @odata.id from the JSON since ExternalStorer will fill it for
    // a client.
    logEntry.erase("@odata.id");

    if (!fileHandler->createFile(fullPath, logEntry))
    {
        stdplus::print(stderr,
                       "Failed to create a file for log entry path: {}\n",
                       fullPath);
        return false;
    }

    cperNotifier->createEntry(fullPath + "/index.json");

    // Attempt to push to logEntrySavedQueue first, before pushing to
    // logEntryQueue that can be popped
    if (logEntrySavedQueue.size() < MAX_NUM_SAVED_LOG_ENTRIES)
    {
        logEntrySavedQueue.push(std::move(fullPath));
    }
    else
    {
        logEntryQueue.push(std::move(fullPath));
    }

    return true;
}

bool ExternalStorerFileInterface::processLogService(
    const nlohmann::json& logService)
{
    if (!logService.contains("@odata.id"))
    {
        stdplus::print(stderr, "@odata.id field doesn't exist in:\n {}\n",
                       logService.dump(4));
        return false;
    }

    if (!logService.contains("Id"))
    {
        stdplus::print(stderr, "Id field doesn't exist in:\n {}\n",
                       logService.dump(4));
        return false;
    }

    logServiceId = logService["Id"].get<std::string>();

    if (!createFile(logService["@odata.id"].get<std::string>(), logService))
    {
        stdplus::print(stderr,
                       "Failed to create LogService index file for:\n{}\n",
                       logService.dump(4));
        return false;
    }
    // ExternalStorer needs a .../Entries/index.json file with no data.
    nlohmann::json jEmpty = "{}"_json;
    return createFile(logService["@odata.id"].get<std::string>() + "/Entries",
                      jEmpty);
}

bool ExternalStorerFileInterface::processOtherTypes(
    const nlohmann::json& jsonPdr) const
{
    if (!jsonPdr.contains("@odata.id"))
    {
        stdplus::print(stderr, "@odata.id field doesn't exist in:\n {}\n",
                       jsonPdr.dump(4));
        return false;
    }
    return createFile(jsonPdr["@odata.id"].get<std::string>(), jsonPdr);
}

bool ExternalStorerFileInterface::createFile(
    const std::string& subPath, const nlohmann::json& jsonPdr) const
{
    return fileHandler->createFile(rootPath + subPath, jsonPdr);
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

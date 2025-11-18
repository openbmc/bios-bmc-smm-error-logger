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

ExternalStorerFileWriter::ExternalStorerFileWriter(std::string_view baseDir) :
    baseDir(baseDir)
{}

bool ExternalStorerFileWriter::isValidPath(const std::string& folderPath) const
{
    // Canonicalize the combined path to resolve '..', etc.
    std::filesystem::path canonicalBase;
    // Check if the parent path is still within the canonical base path
    try
    {
        canonicalBase = std::filesystem::canonical(baseDir);
    }
    catch (...)
    {
        return false;
    }


    // Combine base path and user-controlled path
    std::filesystem::path combinedPath = baseDir / folderPath;

    try
    {
        // Check if the canonical combined path starts with the canonical base path
        // The string comparison (rfind starting at 0) checks for a prefix match. 
        return std::filesystem::canonical(combinedPath).string()
                   .rfind(canonicalBase.string(), 0) == 0;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Handle case where path doesn't exist (e.g., if a new folder is being
        // created) A safer check might be to check if the parent directory
        // exists and is still within baseDir. For simplicity, we'll try an
        // existence check first.

        // If the path doesn't exist, we must canonicalize its *parent* and
        // check that.
        std::filesystem::path parentCanonical;
        try
        {
            parentCanonical =
                std::filesystem::canonical(combinedPath.parent_path());
        }
        catch (...)
        {
            // Parent path doesn't exist either, assume bad path or handle
            // based on policy
            return false;
        }

        // Return true if the parent path is safe, so we can allow the folder/file
        // creation.
        return parentCanonical.string().rfind(canonicalBase.string(), 0) == 0;
    }
}

bool ExternalStorerFileWriter::createFolder(const std::string& folderPath) const
{
    if (!isValidPath(folderPath))
    {
        stdplus::print(stderr, "Invalid path detected: {}\n", folderPath);
        return false;
    }
    std::filesystem::path path(baseDir / folderPath);
    if (!std::filesystem::is_directory(path))
    {
        stdplus::print(stderr, "no directory at {}, creating.\n",
                       path.string());
        if (!std::filesystem::create_directories(path))
        {
            stdplus::print(stderr, "Failed to create a folder at {}\n",
                           path.string());
            return false;
        }
    }
    return true;
}

bool ExternalStorerFileWriter::createFile(const std::string& folderPath,
                                          const nlohmann::json& jsonPdr) const
{
    if (!isValidPath(folderPath))
    {
        stdplus::print(stderr, "Invalid path detected: {}\n", folderPath);
        return false;
    }
    if (!createFolder(folderPath))
    {
        return false;
    }
    std::filesystem::path path(baseDir / folderPath);
    path /= "index.json";
    // If the file already exist, overwrite it.
    std::ofstream output(path);
    output << jsonPdr;
    output.close();
    return true;
}

bool ExternalStorerFileWriter::removeAll(const std::string& filePath) const
{
    if (!isValidPath(filePath))
    {
        stdplus::print(stderr, "Invalid path detected: {}\n", filePath);
        return false;
    }
    // Attempt to delete the file
    std::error_code ec;
    std::filesystem::remove_all(baseDir / filePath, ec);
    if (ec)
    {
        return false;
    }
    return true;
}

ExternalStorerFileInterface::ExternalStorerFileInterface(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    std::string_view rootPath,
    std::unique_ptr<FileHandlerInterface> fileHandler,
    uint32_t numSavedLogEntries, uint32_t numLogEntries) :
    rootPath(rootPath), fileHandler(std::move(fileHandler)), logServiceId(""),
    cperNotifier(std::make_unique<CperFileNotifierHandler>(conn)),
    maxNumSavedLogEntries(numSavedLogEntries), maxNumLogEntries(numLogEntries)
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
    if (logEntryQueue.size() == maxNumLogEntries)
    {
        std::string oldestFilePath = std::move(logEntryQueue.front());
        logEntryQueue.pop();

        if (!fileHandler->removeAll(oldestFilePath))
        {
            stdplus::print(
                stderr,
                "Failed to delete the oldest entry path, not processing the next log,: {}\n",
                oldestFilePath);
            return false;
        }
    }

    std::string id = boost::uuids::to_string(randomGen());
    std::string subPath =
        std::format("/redfish/v1/Systems/system/LogServices/{}/Entries/{}",
                    logServiceId, id);

    // Populate the "Id" with the UUID we generated.
    logEntry["Id"] = id;
    // Remove the @odata.id from the JSON since ExternalStorer will fill it for
    // a client.
    logEntry.erase("@odata.id");

    stdplus::print(stderr, "Creating CPER file under path: {}. \n",
                   rootPath + subPath);
    if (!fileHandler->createFile(subPath, logEntry))
    {
        stdplus::print(stderr,
                       "Failed to create a file for log entry path: {}\n",
                       rootPath + subPath);
        return false;
    }

    cperNotifier->createEntry(rootPath + subPath + "/index.json");

    // Attempt to push to logEntrySavedQueue first, before pushing to
    // logEntryQueue that can be popped
    if (logEntrySavedQueue.size() < maxNumSavedLogEntries)
    {
        logEntrySavedQueue.push(std::move(subPath));
    }
    else
    {
        logEntryQueue.push(std::move(subPath));
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

    const std::string& path = jsonPdr["@odata.id"].get<std::string>();

    stdplus::print(stderr,
                   "Creating error counter file under path: {}.  content: {}\n",
                   path, jsonPdr.dump());
    return createFile(path, jsonPdr);
}

bool ExternalStorerFileInterface::createFile(
    const std::string& subPath, const nlohmann::json& jsonPdr) const
{
    return fileHandler->createFile(subPath, jsonPdr);
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

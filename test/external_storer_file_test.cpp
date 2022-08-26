#include "rde/external_storer_file.hpp"

#include <boost/asio/io_context.hpp>

#include <string_view>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

class MockFileWriter : public FileHandlerInterface
{
  public:
    MOCK_METHOD(bool, createFolder, (const std::string& path),
                (const, override));
    MOCK_METHOD(bool, createFile,
                (const std::string& path, const nlohmann::json& jsonPdr),
                (const, override));
};

class ExternalStorerFileTest : public ::testing::Test
{
  public:
    ExternalStorerFileTest() :
        conn(std::make_shared<sdbusplus::asio::connection>(io)),
        mockFileWriter(std::make_unique<MockFileWriter>())
    {
        mockFileWriterPtr = dynamic_cast<MockFileWriter*>(mockFileWriter.get());
        exStorer = std::make_unique<ExternalStorerFileInterface>(
            conn, rootPath, std::move(mockFileWriter));
    }

  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> conn;

    std::unique_ptr<FileHandlerInterface> mockFileWriter;
    std::unique_ptr<ExternalStorerFileInterface> exStorer;
    MockFileWriter* mockFileWriterPtr;
    const std::string rootPath = "/some/path";
};

TEST_F(ExternalStorerFileTest, InvalidJsonTest)
{
    // Try an invalid JSON.
    std::string jsonStr = "Invalid JSON";
    EXPECT_THAT(exStorer->publishJson(jsonStr), false);
}

TEST_F(ExternalStorerFileTest, NoOdataTypeFailureTest)
{
    // Try a JSON without @odata.type.
    std::string jsonStr = R"(
      {
        "@odata.id": "/redfish/v1/Systems/system/Memory/dimm0/MemoryMetrics",
        "Id":"Metrics"
      }
    )";
    EXPECT_THAT(exStorer->publishJson(jsonStr), false);
}

TEST_F(ExternalStorerFileTest, LogServiceNoOdataIdTest)
{
    // Try a LogService without @odata.id.
    std::string jsonStr = R"(
      {
        "@odata.type": "#LogService.v1_1_0.LogService","Id":"6F7-C1A7C"
      }
    )";
    EXPECT_THAT(exStorer->publishJson(jsonStr), false);
}

TEST_F(ExternalStorerFileTest, LogServiceNoIdTest)
{
    // Try a LogService without Id.
    std::string jsonStr = R"(
      {
        "@odata.id": "/redfish/v1/Systems/system/LogServices/6F7-C1A7C",
        "@odata.type": "#LogService.v1_1_0.LogService"
      }
    )";
    EXPECT_THAT(exStorer->publishJson(jsonStr), false);
}

TEST_F(ExternalStorerFileTest, LogServiceTest)
{
    // A valid LogService test.
    std::string jsonStr = R"(
      {
        "@odata.id": "/redfish/v1/Systems/system/LogServices/6F7-C1A7C",
        "@odata.type": "#LogService.v1_1_0.LogService","Id":"6F7-C1A7C"
        }
      )";
    std::string exServiceFolder =
        "/some/path/redfish/v1/Systems/system/LogServices/6F7-C1A7C";
    std::string exEntriesFolder =
        "/some/path/redfish/v1/Systems/system/LogServices/6F7-C1A7C/Entries";
    nlohmann::json exEntriesJson = "{}"_json;
    nlohmann::json exServiceJson = nlohmann::json::parse(jsonStr);
    EXPECT_CALL(*mockFileWriterPtr, createFile(exServiceFolder, exServiceJson))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockFileWriterPtr, createFile(exEntriesFolder, exEntriesJson))
        .WillOnce(Return(true));
    EXPECT_THAT(exStorer->publishJson(jsonStr), true);
}

TEST_F(ExternalStorerFileTest, LogEntryWithoutLogServiceTest)
{
    // Try a LogEntry without sending a LogService first.
    std::string jsonLogEntry = R"(
      {
        "@odata.type": "#LogEntry.v1_13_0.LogEntry"
      }
    )";
    EXPECT_THAT(exStorer->publishJson(jsonLogEntry), false);
}

TEST_F(ExternalStorerFileTest, LogEntryTest)
{
    // Before sending a LogEntry, first we need to push a LogService.
    std::string jsonLogSerivce = R"(
      {
        "@odata.id": "/redfish/v1/Systems/system/LogServices/6F7-C1A7C",
        "@odata.type": "#LogService.v1_1_0.LogService","Id":"6F7-C1A7C"
      }
    )";
    std::string exServiceFolder =
        "/some/path/redfish/v1/Systems/system/LogServices/6F7-C1A7C";
    std::string exEntriesFolder =
        "/some/path/redfish/v1/Systems/system/LogServices/6F7-C1A7C/Entries";
    nlohmann::json exEntriesJson = "{}"_json;
    nlohmann::json exServiceJson = nlohmann::json::parse(jsonLogSerivce);
    EXPECT_CALL(*mockFileWriterPtr, createFile(exServiceFolder, exServiceJson))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockFileWriterPtr, createFile(exEntriesFolder, exEntriesJson))
        .WillOnce(Return(true));
    EXPECT_THAT(exStorer->publishJson(jsonLogSerivce), true);

    // Now send a LogEntry
    std::string jsonLogEntry = R"(
      {
        "@odata.id": "/some/odata/id",
        "@odata.type": "#LogEntry.v1_13_0.LogEntry"
      }
    )";

    nlohmann::json logEntryOut;
    EXPECT_CALL(*mockFileWriterPtr, createFile(_, _))
        .WillOnce(DoAll(SaveArg<1>(&logEntryOut), Return(true)));

    EXPECT_THAT(exStorer->publishJson(jsonLogEntry), true);
    EXPECT_NE(logEntryOut["Id"], nullptr);
    EXPECT_EQ(logEntryOut["@odata.id"], nullptr);
}

TEST_F(ExternalStorerFileTest, OtherSchemaNoOdataIdTest)
{
    // Try a another PDRs without @odata.id.
    std::string jsonStr = R"(
      {
        "@odata.type": "#MemoryMetrics.v1_4_1.MemoryMetrics",
        "Id":"Metrics"
      }
    )";
    EXPECT_THAT(exStorer->publishJson(jsonStr), false);
}

TEST_F(ExternalStorerFileTest, OtherSchemaTypeTest)
{
    // A valid MemoryMetrics PDR.
    std::string jsonStr = R"(
      {
        "@odata.id": "/redfish/v1/Systems/system/Memory/dimm0/MemoryMetrics",
        "@odata.type": "#MemoryMetrics.v1_4_1.MemoryMetrics",
        "Id": "Metrics"
      }
    )";
    std::string exFolder =
        "/some/path/redfish/v1/Systems/system/Memory/dimm0/MemoryMetrics";
    nlohmann::json exJson = nlohmann::json::parse(jsonStr);
    EXPECT_CALL(*mockFileWriterPtr, createFile(exFolder, exJson))
        .WillOnce(Return(true));
    EXPECT_THAT(exStorer->publishJson(jsonStr), true);
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

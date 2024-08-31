#include "rde/external_storer_file.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/test/sdbus_mock.hpp>

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
using ::testing::StrEq;

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
        mockFileWriter(std::make_unique<MockFileWriter>()),
        bus(sdbusplus::get_mocked_new(&sdbusMock))
    {
        mockFileWriterPtr = dynamic_cast<MockFileWriter*>(mockFileWriter.get());

        EXPECT_CALL(
            sdbusMock,
            sd_bus_add_object_manager(
                nullptr, _,
                StrEq(
                    "/xyz/openbmc_project/external_storer/bios_bmc_smm_error_logger/CPER")))
            .WillOnce(Return(0));

        exStorer = std::make_unique<ExternalStorerFileInterface>(
            bus, rootPath, std::move(mockFileWriter));
    }

  protected:
    std::unique_ptr<FileHandlerInterface> mockFileWriter;
    std::unique_ptr<ExternalStorerFileInterface> exStorer;
    MockFileWriter* mockFileWriterPtr;
    const std::string rootPath = "/some/path";

    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus_t bus;
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

    constexpr const char* dbusPath =
        "/xyz/openbmc_project/external_storer/bios_bmc_smm_error_logger/CPER/entry0";
    constexpr const char* dbusInterface = "xyz.openbmc_project.Common.FilePath";

    EXPECT_CALL(sdbusMock, sd_bus_add_object_vtable(nullptr, _, StrEq(dbusPath),
                                                    StrEq(dbusInterface), _, _))
        .WillOnce(Return(0));
    // EXPECT_CALL(sdbusMock,
    //             sd_bus_emit_interfaces_added_strv(nullptr, StrEq(dbusPath),
    //             _))
    //     .WillOnce(Return(0));

    // EXPECT_CALL(sdbusMock, sd_bus_emit_interfaces_removed_strv(
    //                            nullptr, StrEq(dbusPath), _))
    //     .WillOnce(Return(0));

    EXPECT_THAT(exStorer->publishJson(jsonLogEntry), true);
    EXPECT_NE(logEntryOut["Id"], nullptr);
    EXPECT_EQ(logEntryOut["@odata.id"], nullptr);

    // EXPECT_CALL(sdbusMock, sd_bus_emit_interfaces_removed_strv(nullptr, _,
    // _))
    //     .WillOnce(Return(0));

    // EXPECT_CALL(sdbusMock, sd_bus_emit_interfaces_removed_strv(
    //                            nullptr, StrEq(dbusPath), _))
    //     .WillOnce(Return(0));
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

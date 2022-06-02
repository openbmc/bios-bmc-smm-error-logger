#include "config.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bios_bmc_smm_error_logger
{
namespace
{

using ::testing::ElementsAreArray;

TEST(ConfigTest, EmptyConfigPath)
{
    EXPECT_THROW(
        try { createConfig(""); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Configuration file is missing");
            throw;
        },
        std::runtime_error);
}

TEST(ConfigTest, InvalidConfigOpen)
{
    EXPECT_THROW(
        try {
            createConfig("invalid.json");
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Config file 'invalid.json' is missing");
            throw;
        },
        std::runtime_error);
}

TEST(ConfigTest, EmptyConfig)
{
    std::ofstream testfile;
    testfile.open("empty.json", std::ios::out);
    auto empty = R"({})"_json;
    testfile << empty.dump();
    testfile.flush();

    EXPECT_THROW(createConfig("empty.json"), nlohmann::detail::out_of_range);
}

TEST(ConfigTest, InvalidMagicNumber)
{
    std::ofstream smallTestfile;
    smallTestfile.open("too_small_magic_number.json", std::ios::out);
    auto small = R"(
        {
            "MagicNumber": ["0x12345678", "0x12345678", "0x12345678"]
        }
    )"_json;
    smallTestfile << small.dump();
    smallTestfile.flush();

    EXPECT_THROW(createConfig("too_small_magic_number.json"),
                 std::runtime_error);

    std::ofstream bigTestfile;
    bigTestfile.open("too_big_magic_number.json", std::ios::out);
    auto big = R"(
        {
            "MagicNumber": ["0x12345678", "0x12345678", "0x12345678", "0x12345678", "0x12345678"]
        }
    )"_json;
    bigTestfile << big.dump();
    bigTestfile.flush();

    EXPECT_THROW(createConfig("too_big_magic_number.json"), std::runtime_error);
}

TEST(ConfigTest, ValidJson)
{
    std::ofstream testFile;
    testFile.open("valid.json", std::ios::out);
    auto valid = R"(
        {
            "BMCInterfaceVersion": 3,
            "MagicNumber": ["0x12345678", "0x22345678", "0x32345678", "0x42345678"],
            "QueueSizeBytes": 15360,
            "UERegionSizeBytes": 768,
            "PollingIntervalMS": 100,
            "MemoryRegionSize": 16384,
            "MemoryRegionOffset": 4035215360
        }
    )"_json;
    testFile << valid.dump();
    testFile.flush();

    auto config = createConfig("valid.json");
    std::array<uint32_t, 4> expectedMagicNumber{0x12345678, 0x22345678,
                                                0x32345678, 0x42345678};
    EXPECT_THAT(config.magicNumber, ElementsAreArray(expectedMagicNumber));

    EXPECT_EQ(config.bmcInterfaceVersion, 3);
    EXPECT_EQ(config.queueSize, 15360);
    EXPECT_EQ(config.ueRegionSize, 768);
    EXPECT_EQ(config.memoryRegionSize, 16384);
    EXPECT_EQ(config.memoryRegionOffset, 4035215360);
    EXPECT_EQ(config.pollintIntervalMS, std::chrono::milliseconds(100));
}

} // namespace
} // namespace bios_bmc_smm_error_logger

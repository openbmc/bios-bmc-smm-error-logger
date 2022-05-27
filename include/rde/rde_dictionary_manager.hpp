#pragma once

#include <stdint.h>

#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

/**
 * @brief Resource ID for the annotation dictionary. The other entity
 * communicating with the BMC (Eg: BIOS) should use the same resource ID for the
 * annotation dictionary.
 */
constexpr uint32_t annotationResourceId = 0;

/**
 * @brief Holds an RDE BEJ dictionary entry.
 */
struct DictionaryEntry
{
    DictionaryEntry(bool valid, std::span<const uint8_t> data) :
        valid(valid), data(data.begin(), data.end())
    {}
    // True indicates that the dictionary data is ready to be used.
    bool valid;
    std::vector<uint8_t> data;
};

/**
 * @brief Manages RDE BEJ dictionaries.
 */
class DictionaryManager
{
  public:
    DictionaryManager();

    /**
     * @brief Starts a dictionary entry with the provided data.
     *
     * @param[in] resourceId - PDR resource id corresponding to the dictionary.
     * @param[in] data - dictionary data.
     */
    void startDictionaryEntry(uint32_t resourceId,
                              std::span<const uint8_t> data);

    /**
     * @brief Set the dictionary valid status. Until this is called, dictionary
     * data is considered to be incomplete for use.
     *
     * @param[in] resourceId - PDR resource id corresponding to the dictionary.
     * @return true if successful.
     */
    bool markDataComplete(uint32_t resourceId);

    /**
     * @brief Add more dictionary data for an existing entry. Adding data to a
     * completed dictionary will mark the dictionary as incomplete.
     *
     * @param[in] resourceId - PDR resource id corresponding to the dictionary.
     * @param[in] data - dictionary data.
     * @return true if successful.
     */
    bool addDictionaryData(uint32_t resourceId, std::span<const uint8_t> data);

    /**
     * @brief Get a dictionary.
     *
     * @param[in] resourceId - PDR resource id corresponding to the dictionary.
     * @return a pointer to the dictionary, if the dictionary is complete else
     * std::nullopt.
     */
    std::optional<std::span<const uint8_t>> getDictionary(uint32_t resourceId);

    /**
     * @brief Get the annotation dictionary.
     *
     * @return a pointer to the annotation dictionary, if the dictionary is
     * complete else std::nullopt.
     */
    std::optional<std::span<const uint8_t>> getAnnotationDictionary();

    /**
     * @brief Get the completed dictionary count.
     *
     * @return number of completed dictionaries available.
     */
    uint32_t getDictionaryCount();

    /**
     * @brief Invalidate all dictionaries.
     */
    void invalidateDictionaries();

  private:
    uint32_t validDictionaryCount;
    std::unordered_map<uint32_t, std::unique_ptr<DictionaryEntry>> dictionaries;

    /**
     * @brief Set a dictionary entry to be invalid and reduce the valid
     * dictionary count.
     *
     * @param[in] entry - A dictionary entry.
     */
    void invalidateDictionaryEntry(DictionaryEntry& entry);

    /**
     * @brief Set the dictionary entry valid flag and increase the valid
     * dictionary count.
     *
     * @param[in] entry - A dictionary entry.
     */
    void validateDictionaryEntry(DictionaryEntry& entry);
};

} // namespace rde
} // namespace bios_bmc_smm_error_logger

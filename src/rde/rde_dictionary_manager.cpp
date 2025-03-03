#include "rde/rde_dictionary_manager.hpp"

#include <stdplus/print.hpp>

#include <format>

namespace bios_bmc_smm_error_logger
{
namespace rde
{

DictionaryManager::DictionaryManager() : validDictionaryCount(0) {}

void DictionaryManager::startDictionaryEntry(
    uint32_t resourceId, const std::span<const uint8_t> data)
{
    // Check whether the resourceId is already available.
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        dictionaries[resourceId] =
            std::make_unique<DictionaryEntry>(false, data);
        return;
    }

    // Since we are creating a new dictionary on an existing entry, invalidate
    // the existing entry.
    invalidateDictionaryEntry(*itemIt->second);

    // Flush the existing data.
    itemIt->second->data.clear();
    itemIt->second->data.insert(itemIt->second->data.begin(), data.begin(),
                                data.end());
}

bool DictionaryManager::markDataComplete(uint32_t resourceId)
{
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        stdplus::print(stderr, "Resource ID {} not found.\n", resourceId);
        return false;
    }
    validateDictionaryEntry(*itemIt->second);
    return true;
}

bool DictionaryManager::addDictionaryData(uint32_t resourceId,
                                          const std::span<const uint8_t> data)
{
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        stdplus::print(stderr, "Resource ID {} not found.\n", resourceId);
        return false;
    }
    // Since we are modifying an existing entry, invalidate the existing entry.
    invalidateDictionaryEntry(*itemIt->second);
    itemIt->second->data.insert(itemIt->second->data.end(), data.begin(),
                                data.end());
    return true;
}

std::optional<std::span<const uint8_t>> DictionaryManager::getDictionary(
    uint32_t resourceId)
{
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        stdplus::print(stderr, "Resource ID {} not found.\n", resourceId);
        return std::nullopt;
    }

    if (!itemIt->second->valid)
    {
        stdplus::print(stderr,
                       "Requested an incomplete dictionary. Resource ID {}\n",
                       resourceId);
        return std::nullopt;
    }
    return itemIt->second->data;
}

std::optional<std::span<const uint8_t>>
    DictionaryManager::getAnnotationDictionary()
{
    return getDictionary(annotationResourceId);
}

uint32_t DictionaryManager::getDictionaryCount()
{
    return validDictionaryCount;
}

void DictionaryManager::invalidateDictionaries()
{
    // We won't flush the existing data. The data will be flushed if a new entry
    // is added for an existing resource ID.
    for (const auto& element : dictionaries)
    {
        element.second->valid = false;
    }
    validDictionaryCount = 0;
}

void DictionaryManager::invalidateDictionaryEntry(DictionaryEntry& entry)
{
    // If this is a valid entry, reduce the valid dictionary count.
    if (entry.valid)
    {
        --validDictionaryCount;
    }
    entry.valid = false;
}

void DictionaryManager::validateDictionaryEntry(DictionaryEntry& entry)
{
    if (!entry.valid)
    {
        ++validDictionaryCount;
    }
    entry.valid = true;
}

} // namespace rde
} // namespace bios_bmc_smm_error_logger

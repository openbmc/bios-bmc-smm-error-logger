#include "rde/rde_dictionary_manager.hpp"

#include <fmt/format.h>

namespace biosBmcSmmErrorLogger
{
namespace rde
{

DictionaryManager::DictionaryManager() : dictionaryCount(0)
{}

void DictionaryManager::startDictionaryEntry(uint32_t resourceId,
                                             std::span<const uint8_t> data)
{
    // Check whether the resourceId is already available.
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        dictionaries[resourceId] =
            std::make_unique<DictionaryEntry>(false, data);
        return;
    }
    // Reduce the dictionary count since we are creating a new dictionary on a
    // existing entry.
    if (itemIt->second->valid)
    {
        --dictionaryCount;
    }
    itemIt->second->valid = false;
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
        fmt::print(stderr, "Resource ID {} not found.\n", resourceId);
        return false;
    }
    itemIt->second->valid = true;
    ++dictionaryCount;
    return true;
}

bool DictionaryManager::addMoreDictionaryData(uint32_t resourceId,
                                              std::span<const uint8_t> data)
{
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        fmt::print(stderr, "Resource ID {} not found.\n", resourceId);
        return false;
    }
    itemIt->second->valid = false;
    itemIt->second->data.insert(itemIt->second->data.end(), data.begin(),
                                data.end());
    return true;
}

std::optional<std::span<const uint8_t>>
    DictionaryManager::getDictionary(uint32_t resourceId)
{
    auto itemIt = dictionaries.find(resourceId);
    if (itemIt == dictionaries.end())
    {
        fmt::print(stderr, "Resource ID {} not found.\n", resourceId);
        return std::nullopt;
    }

    if (!itemIt->second->valid)
    {
        fmt::print(stderr,
                   "Requested an incomplete dictionary. Resource ID {}\n",
                   resourceId);
        return std::nullopt;
    }
    return std::span(itemIt->second->data);
}

std::optional<std::span<const uint8_t>>
    DictionaryManager::getAnnotationDictionary()
{
    return getDictionary(annotationResourceId);
}

uint32_t DictionaryManager::getDictionaryCount()
{
    return dictionaryCount;
}

void DictionaryManager::invalidateDictionaries()
{
    // We won't flush the existing data. The data will be flushed if a new entry
    // is added for an existing resource ID.
    for (auto& element : dictionaries)
    {
        element.second->valid = false;
    }
    dictionaryCount = 0;
}

} // namespace rde
} // namespace biosBmcSmmErrorLogger

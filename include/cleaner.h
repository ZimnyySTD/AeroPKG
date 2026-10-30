#pragma once

#include "common.h"
#include <string>
#include <vector>

struct CleanableItem {
    std::string path;
    std::string description;
    uint64_t sizeBytes;
    bool isSelected;
};

class StorageCleaner {
public:
    StorageCleaner();
    ~StorageCleaner();

    void scanStorage();
    const std::vector<CleanableItem>& getItems() const { return cleanableItems; }
    uint64_t getTotalReclaimableBytes() const;

    bool purgeSelected();
    bool purgeAll();

private:
    std::vector<CleanableItem> cleanableItems;
    void scanDirectory(const std::string& dirPath, const std::string& desc);
};

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
    uint64_t getSystemTotalBytes() const { return systemTotalBytes; }
    uint64_t getSystemFreeBytes() const { return systemFreeBytes; }

    bool purgeSelected();
    bool purgeAll();

private:
    std::vector<CleanableItem> cleanableItems;
    uint64_t systemTotalBytes = 0;
    uint64_t systemFreeBytes = 0;
    void scanDirectory(const std::string& dirPath, const std::string& desc, int depth = 0);
};

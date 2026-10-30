#include "cleaner.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

StorageCleaner::StorageCleaner() {}
StorageCleaner::~StorageCleaner() {}

void StorageCleaner::scanDirectory(const std::string& dirPath, const std::string& desc) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string fullPath = dirPath + "/" + entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively check subdirectories
                scanDirectory(fullPath, desc);
            } else {
                CleanableItem item;
                item.path = fullPath;
                item.description = desc + ": " + entry->d_name;
                item.sizeBytes = st.st_size;
                item.isSelected = true;
                cleanableItems.push_back(item);
            }
        }
    }
    closedir(dir);
}

void StorageCleaner::scanStorage() {
    cleanableItems.clear();
    scanDirectory(PATH_DATA_PKG, "Residual Package Cache");
    scanDirectory(PATH_USER_DOWNLOAD, "Orphaned Download Chunk");
    LOGI("Scan complete: found %zu cleanable items (%llu MB)",
         cleanableItems.size(),
         (unsigned long long)(getTotalReclaimableBytes() / (1024 * 1024)));
}

uint64_t StorageCleaner::getTotalReclaimableBytes() const {
    uint64_t total = 0;
    for (const auto& item : cleanableItems) {
        if (item.isSelected) total += item.sizeBytes;
    }
    return total;
}

bool StorageCleaner::purgeSelected() {
    int removed = 0;
    for (auto it = cleanableItems.begin(); it != cleanableItems.end();) {
        if (it->isSelected) {
            unlink(it->path.c_str());
            it = cleanableItems.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    LOGI("Purged %d cache files", removed);
    return true;
}

bool StorageCleaner::purgeAll() {
    for (auto& item : cleanableItems) {
        item.isSelected = true;
    }
    return purgeSelected();
}

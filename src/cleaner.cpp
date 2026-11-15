#include "cleaner.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <set>

StorageCleaner::StorageCleaner() {}
StorageCleaner::~StorageCleaner() {}

void StorageCleaner::scanDirectory(const std::string& dirPath, const std::string& desc, int depth) {
    if (depth > 5) return;
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string fullPath = dirPath + "/" + entry->d_name;
        struct stat st{};
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scanDirectory(fullPath, desc, depth + 1);
        } else if (S_ISREG(st.st_mode) && st.st_size > 0) {
            CleanableItem item;
            item.path = fullPath;
            item.description = desc + ": " + entry->d_name;
            item.sizeBytes = static_cast<uint64_t>(st.st_size);
            item.isSelected = true;
            cleanableItems.push_back(item);
        }
    }
    closedir(dir);
}

void StorageCleaner::scanStorage() {
    cleanableItems.clear();
    const std::pair<const char*, const char*> roots[] = {
        {PATH_DATA_PKG, "Residual Package Cache"},
        {PATH_USER_DOWNLOAD, "Orphaned Download"},
        {"/data/GoldHEN", "GoldHEN Temporary Data"},
        {"/data/notifications", "Notification Download Cache"},
        {"/system_data/priv/download", "System Download Cache"}
    };
    std::set<std::string> seen;
    for (const auto& root : roots) {
        size_t before = cleanableItems.size();
        scanDirectory(root.first, root.second);
        for (size_t i = before; i < cleanableItems.size();) {
            if (!seen.insert(cleanableItems[i].path).second) cleanableItems.erase(cleanableItems.begin() + i);
            else ++i;
        }
    }
    LOGI("Storage scan found %zu files (%llu MB)", cleanableItems.size(), static_cast<unsigned long long>(getTotalReclaimableBytes() / (1024 * 1024)));
}

uint64_t StorageCleaner::getTotalReclaimableBytes() const {
    uint64_t total = 0;
    for (const auto& item : cleanableItems) if (item.isSelected) total += item.sizeBytes;
    return total;
}

bool StorageCleaner::purgeSelected() {
    bool success = true;
    int removed = 0;
    for (auto it = cleanableItems.begin(); it != cleanableItems.end();) {
        if (!it->isSelected) { ++it; continue; }
        if (unlink(it->path.c_str()) == 0) ++removed;
        else success = false;
        it = cleanableItems.erase(it);
    }
    LOGI("Purged %d cache files", removed);
    return success;
}

bool StorageCleaner::purgeAll() {
    for (auto& item : cleanableItems) item.isSelected = true;
    return purgeSelected();
}

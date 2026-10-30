#include "avatar_mgr.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>

AvatarManager::AvatarManager() {
    currentUserId = "10000000";
}

AvatarManager::~AvatarManager() {}

static bool copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) return false;
    out << in.rdbuf();
    return true;
}

bool AvatarManager::init() {
    // Scan /user/home to find the active user ID directory
    DIR* dir = opendir(PATH_USER_HOME);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] != '.' && entry->d_type == DT_DIR) {
                currentUserId = entry->d_name;
                break;
            }
        }
        closedir(dir);
    }

    currentAvatarPath = getUserHomeDir() + "/avatar.png";
    LOGI("Active PS4 User ID: %s, Avatar: %s", currentUserId.c_str(), currentAvatarPath.c_str());
    return true;
}

std::string AvatarManager::getUserHomeDir() const {
    return std::string(PATH_USER_HOME) + "/" + currentUserId;
}

std::vector<AvatarPreset> AvatarManager::getAvailablePresets() {
    std::vector<AvatarPreset> presets;
    presets.push_back({"PlayStation Classic", "/app0/assets/avatars/ps_classic.png"});
    presets.push_back({"Retro Trophy",       "/app0/assets/avatars/trophy.png"});
    presets.push_back({"Cyber Dark",         "/app0/assets/avatars/cyber.png"});
    presets.push_back({"GoldHEN Crest",       "/app0/assets/avatars/goldhen.png"});
    return presets;
}

std::vector<std::string> AvatarManager::scanUsbAvatars() {
    std::vector<std::string> found;
    const char* usbMounts[] = { PATH_USB0, PATH_USB1 };

    for (const char* mount : usbMounts) {
        DIR* dir = opendir(mount);
        if (!dir) continue;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find(".png") != std::string::npos || name.find(".jpg") != std::string::npos) {
                found.push_back(std::string(mount) + "/" + name);
            }
        }
        closedir(dir);
    }
    return found;
}

bool AvatarManager::applyAvatar(const std::string& imagePath) {
    std::string dst = getUserHomeDir() + "/avatar.png";
    std::string cacheDst = "/system_data/priv/cache/user/" + currentUserId + "/avatar.png";

    bool s1 = copyFile(imagePath, dst);
    bool s2 = copyFile(imagePath, cacheDst);

    // Also copy DDS/sub-dimensions if present
    if (s1 || s2) {
        LOGI("Applied new avatar for user %s from %s", currentUserId.c_str(), imagePath.c_str());
        currentAvatarPath = dst;
        return true;
    }
    LOGE("Failed to write avatar to %s", dst.c_str());
    return false;
}

bool AvatarManager::applyFromUsb(const std::string& usbPath) {
    return applyAvatar(usbPath);
}

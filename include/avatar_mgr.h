#pragma once

#include "common.h"
#include <string>
#include <vector>

struct AvatarPreset {
    std::string name;
    std::string path;
};

class AvatarManager {
public:
    AvatarManager();
    ~AvatarManager();

    bool init();

    std::string getCurrentUserId() const { return currentUserId; }
    std::string getCurrentAvatarPath() const { return currentAvatarPath; }

    // Preset & USB discovery
    std::vector<AvatarPreset> getAvailablePresets();
    std::vector<std::string> scanUsbAvatars();

    // Application
    bool applyAvatar(const std::string& imagePath);
    bool applyFromUsb(const std::string& usbFilename);

private:
    std::string currentUserId;
    std::string currentAvatarPath;
    std::string getUserHomeDir() const;
};

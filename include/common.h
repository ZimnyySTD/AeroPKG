#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <cstdio>

#define APP_TITLE_ID        "APKG00001"
#define APP_NAME            "AeroPKG"
#define APP_VERSION         "1.00"
#define WEB_SERVER_PORT     8080

// System paths on PS4 (GoldHEN environment)
#define PATH_APP_DB         "/system_data/priv/mms/app.db"
#define PATH_APP_DB_BAK     "/system_data/priv/mms/app.db.aeropkg_bak"
#define PATH_USER_HOME      "/user/home"
#define PATH_USER_APPMETA   "/user/appmeta"
#define PATH_USER_DOWNLOAD  "/user/download"
#define PATH_DATA_PKG       "/data/pkg"
#define PATH_AEROPKG_DATA   "/data/AeroPKG"
#define PATH_USB0           "/mnt/usb0"
#define PATH_USB1           "/mnt/usb1"

// Logging macro
#define LOG_TAG "[AeroPKG] "
#define LOGI(...) printf(LOG_TAG __VA_ARGS__); printf("\n")
#define LOGE(...) fprintf(stderr, LOG_TAG "ERROR: " __VA_ARGS__); fprintf(stderr, "\n")

// Installed Application Metadata Structure
struct InstalledApp {
    std::string titleId;
    std::string titleName;
    std::string version;
    std::string iconPath;
    uint64_t lastPlayedTime;
    uint64_t sizeBytes;
    int category; // 0 = Game, 1 = App/Homebrew, 2 = System
    bool isModified;
};

// Catalog Item (FPKGi games.json compatible)
struct CatalogItem {
    std::string titleId;
    std::string name;
    std::string region;
    std::string version;
    std::string type; // "Game", "Update", "DLC"
    uint64_t sizeBytes;
    std::string pkgUrl;
    std::string iconUrl;
};

// Queue Item for Package Installations
struct PkgInstallTask {
    std::string title;
    std::string filePathOrUrl;
    bool isFromUsb;
    int progressPercent;
    std::string status; // "Queued", "Downloading", "Installing", "Completed", "Failed"
};

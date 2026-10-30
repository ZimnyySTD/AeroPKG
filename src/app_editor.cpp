#include "app_editor.h"
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <algorithm>
#include <chrono>
#include <fstream>

AppEditor::AppEditor() {}
AppEditor::~AppEditor() {}

static bool copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) return false;
    out << in.rdbuf();
    return true;
}

bool AppEditor::backupDatabase() {
    LOGI("Backing up app.db from %s to %s", PATH_APP_DB, PATH_APP_DB_BAK);
    return copyFile(PATH_APP_DB, PATH_APP_DB_BAK);
}

bool AppEditor::restoreDatabase() {
    LOGI("Restoring app.db from backup %s", PATH_APP_DB_BAK);
    return copyFile(PATH_APP_DB_BAK, PATH_APP_DB);
}

std::string AppEditor::getMetaIconPath(const std::string& titleId) {
    std::string iconPath = std::string(PATH_USER_APPMETA) + "/" + titleId + "/icon0.png";
    struct stat st;
    if (stat(iconPath.c_str(), &st) == 0) {
        return iconPath;
    }
    return "";
}

bool AppEditor::refreshInstalledApps() {
    apps.clear();
    sqlite3* db = nullptr;

    if (sqlite3_open_v2(PATH_APP_DB, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        LOGE("Failed to open app.db: %s", sqlite3_errmsg(db));
        // Provide mock demo apps for PC compilation testing
        apps.push_back({"CUSA00123", "Demo Game Title", "01.00", "", 1600000000, 45000000000ULL, 0, false});
        apps.push_back({"APKG00001", "AeroPKG Manager", "01.00", "", 1600000010, 150000000ULL, 1, false});
        if (db) sqlite3_close(db);
        return true;
    }

    const char* query = "SELECT titleId, titleName, uiCategory, lastPlayedTime FROM tbl_appinfo GROUP BY titleId ORDER BY lastPlayedTime DESC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            InstalledApp app;
            const char* id = (const char*)sqlite3_column_text(stmt, 0);
            const char* name = (const char*)sqlite3_column_text(stmt, 1);
            int cat = sqlite3_column_int(stmt, 2);
            uint64_t lastPlayed = (uint64_t)sqlite3_column_int64(stmt, 3);

            app.titleId = id ? id : "";
            app.titleName = name ? name : "Unknown Title";
            app.category = cat;
            app.lastPlayedTime = lastPlayed;
            app.iconPath = getMetaIconPath(app.titleId);
            app.version = "1.00";
            app.sizeBytes = 0;
            app.isModified = false;

            if (!app.titleId.empty()) {
                apps.push_back(app);
            }
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    LOGI("Loaded %zu installed apps from app.db", apps.size());
    return true;
}

bool AppEditor::updateAppTitle(const std::string& titleId, const std::string& newTitle) {
    backupDatabase();
    sqlite3* db = nullptr;
    if (sqlite3_open(PATH_APP_DB, &db) != SQLITE_OK) {
        LOGE("Failed to open app.db for writing");
        return false;
    }

    const char* query = "UPDATE tbl_appinfo SET titleName = ? WHERE titleId = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, titleId.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = true;
            LOGI("Successfully updated title for %s to '%s'", titleId.c_str(), newTitle.c_str());
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);

    if (success) {
        for (auto& app : apps) {
            if (app.titleId == titleId) {
                app.titleName = newTitle;
                app.isModified = true;
                break;
            }
        }
    }
    return success;
}

bool AppEditor::updateAppIcon(const std::string& titleId, const std::string& newIconPath) {
    std::string targetMetaDir = std::string(PATH_USER_APPMETA) + "/" + titleId;
    mkdir(targetMetaDir.c_str(), 0777);

    std::string targetIcon = targetMetaDir + "/icon0.png";
    if (copyFile(newIconPath, targetIcon)) {
        LOGI("Replaced icon for %s at %s", titleId.c_str(), targetIcon.c_str());
        for (auto& app : apps) {
            if (app.titleId == titleId) {
                app.iconPath = targetIcon;
                app.isModified = true;
                break;
            }
        }
        return true;
    }
    LOGE("Failed to write replacement icon to %s", targetIcon.c_str());
    return false;
}

bool AppEditor::exportAssets(const std::string& titleId, const std::string& destinationDir) {
    mkdir(destinationDir.c_str(), 0777);
    std::string appDir = destinationDir + "/" + titleId;
    mkdir(appDir.c_str(), 0777);

    std::string srcIcon = std::string(PATH_USER_APPMETA) + "/" + titleId + "/icon0.png";
    std::string srcPic0 = std::string(PATH_USER_APPMETA) + "/" + titleId + "/pic0.png";
    std::string srcSfo  = std::string(PATH_USER_APPMETA) + "/" + titleId + "/param.sfo";

    copyFile(srcIcon, appDir + "/icon0.png");
    copyFile(srcPic0, appDir + "/pic0.png");
    copyFile(srcSfo,  appDir + "/param.sfo");

    LOGI("Exported assets for %s to %s", titleId.c_str(), appDir.c_str());
    return true;
}

bool AppEditor::moveAppPosition(int currentIndex, int targetIndex) {
    if (currentIndex < 0 || currentIndex >= (int)apps.size() ||
        targetIndex < 0 || targetIndex >= (int)apps.size()) {
        return false;
    }

    auto item = apps[currentIndex];
    apps.erase(apps.begin() + currentIndex);
    apps.insert(apps.begin() + targetIndex, item);
    return true;
}

bool AppEditor::sortAppsAlphabetical(bool ascending) {
    std::sort(apps.begin(), apps.end(), [ascending](const InstalledApp& a, const InstalledApp& b) {
        return ascending ? (a.titleName < b.titleName) : (a.titleName > b.titleName);
    });
    return true;
}

bool AppEditor::sortAppsBySize() {
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return a.sizeBytes > b.sizeBytes;
    });
    return true;
}

bool AppEditor::sortAppsByLastPlayed() {
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return a.lastPlayedTime > b.lastPlayedTime;
    });
    return true;
}

bool AppEditor::applyXmbOrder() {
    backupDatabase();
    sqlite3* db = nullptr;
    if (sqlite3_open(PATH_APP_DB, &db) != SQLITE_OK) {
        return false;
    }

    // Set sequential descending timestamps: item 0 has highest timestamp (front of XMB)
    uint64_t baseTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    const char* query = "UPDATE tbl_appinfo SET lastPlayedTime = ? WHERE titleId = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        for (size_t i = 0; i < apps.size(); ++i) {
            uint64_t appTime = baseTime - (i * 100);
            sqlite3_bind_int64(stmt, 1, appTime);
            sqlite3_bind_text(stmt, 2, apps[i].titleId.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            apps[i].lastPlayedTime = appTime;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    LOGI("Applied new XMB ordering to %zu applications in app.db", apps.size());
    return true;
}

bool AppEditor::softRestartShellCore() {
    LOGI("Triggering SceShellCore soft-restart to refresh XMB...");
    // GoldHEN shellcore reload / kill signal
    system("killall -9 SceShellCore");
    return true;
}

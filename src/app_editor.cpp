#include "app_editor.h"
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

AppEditor::AppEditor() {}
AppEditor::~AppEditor() {}

static bool copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) return false;
    out << in.rdbuf();
    return out.good();
}

static std::string metaIconPath(const std::string& titleId) {
    const std::string base = std::string(PATH_USER_APPMETA) + "/" + titleId + "/";
    const char* names[] = {"icon0.png", "icon0.PNG"};
    for (const char* name : names) {
        std::string path = base + name;
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return path;
    }
    return "";
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
    return metaIconPath(titleId);
}

static bool loadAppsQuery(sqlite3* db, const char* query, std::vector<InstalledApp>& apps) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::set<std::string> seen;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (!id || !*id || !seen.insert(id).second) continue;

        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        InstalledApp app;
        app.titleId = id;
        app.titleName = (name && *name) ? name : app.titleId;
        app.category = sqlite3_column_int(stmt, 2);
        app.lastPlayedTime = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
        app.iconPath = metaIconPath(app.titleId);
        app.version = "";
        app.sizeBytes = 0;
        app.isModified = false;
        apps.push_back(app);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool AppEditor::refreshInstalledApps() {
    apps.clear();
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(PATH_APP_DB, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        LOGE("Failed to open app.db");
        if (db) sqlite3_close(db);
        return false;
    }

    const char* queries[] = {
        "SELECT titleId, titleName, uiCategory, lastPlayedTime FROM tbl_appinfo ORDER BY lastPlayedTime DESC;",
        "SELECT titleId, titleName, uiCategory, 0 FROM tbl_appinfo ORDER BY titleName COLLATE NOCASE;",
        "SELECT titleId, titleName, 0, 0 FROM appinfo ORDER BY titleName COLLATE NOCASE;"
    };

    bool loaded = false;
    for (const char* query : queries) {
        if (loadAppsQuery(db, query, apps)) {
            loaded = true;
            if (!apps.empty()) break;
        }
    }

    if (!loaded) LOGE("Could not read installed applications from app.db: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    LOGI("Loaded %zu installed applications from app.db", apps.size());
    return loaded;
}

bool AppEditor::updateAppTitle(const std::string& titleId, const std::string& newTitle) {
    if (newTitle.empty() || !backupDatabase()) return false;
    sqlite3* db = nullptr;
    if (sqlite3_open(PATH_APP_DB, &db) != SQLITE_OK) return false;

    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    const char* query = "UPDATE tbl_appinfo SET titleName = ? WHERE titleId = ?;";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, titleId.c_str(), -1, SQLITE_TRANSIENT);
        success = sqlite3_step(stmt) == SQLITE_DONE;
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
    if (!copyFile(newIconPath, targetIcon)) return false;
    for (auto& app : apps) {
        if (app.titleId == titleId) {
            app.iconPath = targetIcon;
            app.isModified = true;
            break;
        }
    }
    return true;
}

bool AppEditor::exportAssets(const std::string& titleId, const std::string& destinationDir) {
    mkdir(destinationDir.c_str(), 0777);
    std::string appDir = destinationDir + "/" + titleId;
    mkdir(appDir.c_str(), 0777);
    const std::string source = std::string(PATH_USER_APPMETA) + "/" + titleId + "/";
    copyFile(source + "icon0.png", appDir + "/icon0.png");
    copyFile(source + "pic0.png", appDir + "/pic0.png");
    copyFile(source + "param.sfo", appDir + "/param.sfo");
    return true;
}

bool AppEditor::moveAppPosition(int currentIndex, int targetIndex) {
    if (currentIndex < 0 || currentIndex >= (int)apps.size() || targetIndex < 0 || targetIndex >= (int)apps.size()) return false;
    auto item = apps[currentIndex];
    apps.erase(apps.begin() + currentIndex);
    apps.insert(apps.begin() + targetIndex, item);
    return true;
}

bool AppEditor::sortAppsAlphabetical(bool ascending) {
    std::sort(apps.begin(), apps.end(), [ascending](const InstalledApp& a, const InstalledApp& b) {
        return ascending ? a.titleName < b.titleName : a.titleName > b.titleName;
    });
    return true;
}

bool AppEditor::sortAppsBySize() {
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) { return a.sizeBytes > b.sizeBytes; });
    return true;
}

bool AppEditor::sortAppsByLastPlayed() {
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) { return a.lastPlayedTime > b.lastPlayedTime; });
    return true;
}

bool AppEditor::applyXmbOrder() {
    if (apps.empty() || !backupDatabase()) return false;
    sqlite3* db = nullptr;
    if (sqlite3_open(PATH_APP_DB, &db) != SQLITE_OK) return false;

    const uint64_t baseTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bool success = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_stmt* stmt = nullptr;
    const char* query = "UPDATE tbl_appinfo SET lastPlayedTime = ? WHERE titleId = ?;";
    if (success && sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
        for (size_t i = 0; i < apps.size(); ++i) {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(baseTime - i * 100));
            sqlite3_bind_text(stmt, 2, apps[i].titleId.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE) success = false;
            sqlite3_reset(stmt);
            apps[i].lastPlayedTime = baseTime - i * 100;
        }
        sqlite3_finalize(stmt);
    } else {
        success = false;
    }
    sqlite3_exec(db, success ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return success;
}

bool AppEditor::softRestartShellCore() {
    LOGI("Triggering SceShellCore soft-restart to refresh XMB...");
    return system("killall -9 SceShellCore") == 0;
}

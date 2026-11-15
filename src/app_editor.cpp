#include "app_editor.h"
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <cerrno>
#include <cstring>

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

static std::string metaDir(const std::string& titleId) {
    return std::string(PATH_USER_APPMETA) + "/" + titleId;
}

static std::string metaIconPath(const std::string& titleId) {
    const char* roots[] = {PATH_USER_APPMETA, PATH_SYSTEM_APPMETA, PATH_MMS_APPMETA, PATH_DATA_APPMETA};
    const char* names[] = {"icon0.png", "icon0.PNG", "icon0.dds"};
    for (const char* root : roots) {
        for (const char* name : names) {
            std::string path = std::string(root) + "/" + titleId + "/" + name;
            struct stat st{};
            if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return path;
        }
    }
    return "";
}

static uint16_t readU16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static uint32_t readU32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static std::string readSfoField(const std::string& sfoPath, const std::string& wantedKey) {
    std::ifstream file(sfoPath, std::ios::binary);
    if (!file.is_open()) return "";
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() < 20 || data[0] != 'P' || data[1] != 'S' || data[2] != 'F' || data[3] != 0) return "";

    const uint32_t keyOffset = readU32(&data[8]);
    const uint32_t dataOffset = readU32(&data[12]);
    const uint32_t count = readU32(&data[16]);
    const size_t indexStart = 20;
    if (keyOffset >= data.size() || dataOffset >= data.size() || count > 4096 || indexStart + count * 16 > data.size()) return "";

    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char* entry = &data[indexStart + i * 16];
        const uint16_t keyRel = readU16(entry);
        const uint32_t length = readU32(entry + 4);
        const uint32_t valueRel = readU32(entry + 12);
        size_t keyPos = static_cast<size_t>(keyOffset) + keyRel;
        size_t valuePos = static_cast<size_t>(dataOffset) + valueRel;
        if (keyPos >= data.size() || valuePos >= data.size()) continue;
        size_t keyLength = 0;
        while (keyPos + keyLength < data.size() && data[keyPos + keyLength] != 0) ++keyLength;
        std::string key(reinterpret_cast<const char*>(&data[keyPos]), keyLength);
        if (key != wantedKey) continue;
        size_t safeLength = std::min<size_t>(length, data.size() - valuePos);
        while (safeLength > 0 && data[valuePos + safeLength - 1] == 0) --safeLength;
        return std::string(reinterpret_cast<const char*>(&data[valuePos]), safeLength);
    }
    return "";
}

static bool isDirectory(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static uint64_t directorySize(const std::string& path, int depth = 0) {
    if (depth > 8) return 0;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return static_cast<uint64_t>(st.st_size);
    uint64_t total = 0;
    DIR* dir = opendir(path.c_str());
    if (!dir) return 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        total += directorySize(path + "/" + entry->d_name, depth + 1);
    }
    closedir(dir);
    return total;
}

static void scanAppMetadata(std::vector<InstalledApp>& apps) {
    std::set<std::string> seen;
    for (const auto& app : apps) seen.insert(app.titleId);

    const char* metaRoots[] = {PATH_USER_APPMETA, PATH_SYSTEM_APPMETA, PATH_MMS_APPMETA, PATH_DATA_APPMETA};
    for (const char* root : metaRoots) {
        DIR* dir = opendir(root);
        if (!dir) {
            LOGI("App metadata root unavailable: %s (errno=%d: %s)", root, errno, strerror(errno));
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string directory = std::string(root) + "/" + entry->d_name;
            if (!isDirectory(directory)) continue;
            const std::string sfoPath = directory + "/param.sfo";
            std::string titleId = readSfoField(sfoPath, "TITLE_ID");
            if (titleId.empty()) titleId = entry->d_name;
            if (!seen.insert(titleId).second) continue;
            InstalledApp app;
            app.titleId = titleId;
            app.titleName = readSfoField(sfoPath, "TITLE");
            if (app.titleName.empty()) app.titleName = titleId;
            app.version = readSfoField(sfoPath, "VERSION");
            app.iconPath = metaIconPath(titleId);
            app.lastPlayedTime = 0;
            app.sizeBytes = directorySize(std::string(PATH_USER_APP) + "/" + titleId);
            app.category = 0;
            app.isModified = false;
            apps.push_back(app);
        }
        closedir(dir);
    }

    DIR* appRoot = opendir(PATH_USER_APP);
    if (!appRoot) return;
    struct dirent* entry;
    while ((entry = readdir(appRoot)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        const std::string titleDirectory = std::string(PATH_USER_APP) + "/" + entry->d_name;
        if (!isDirectory(titleDirectory)) continue;
        const std::string sfoPath = titleDirectory + "/sce_sys/param.sfo";
        std::string titleId = readSfoField(sfoPath, "TITLE_ID");
        if (titleId.empty()) titleId = entry->d_name;
        if (!seen.insert(titleId).second) continue;
        InstalledApp app;
        app.titleId = titleId;
        app.titleName = readSfoField(sfoPath, "TITLE");
        if (app.titleName.empty()) app.titleName = titleId;
        app.version = readSfoField(sfoPath, "VERSION");
        app.iconPath = metaIconPath(titleId);
        app.lastPlayedTime = 0;
        app.sizeBytes = directorySize(titleDirectory);
        app.category = 0;
        app.isModified = false;
        apps.push_back(app);
    }
    closedir(appRoot);
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
    for (const auto& app : apps) seen.insert(app.titleId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (!id || !*id || !seen.insert(id).second) continue;
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        InstalledApp app;
        app.titleId = id;
        app.titleName = (name && *name) ? name : readSfoField(metaDir(app.titleId) + "/param.sfo", "TITLE");
        if (app.titleName.empty()) app.titleName = app.titleId;
        app.version = readSfoField(metaDir(app.titleId) + "/param.sfo", "VERSION");
        app.category = sqlite3_column_int(stmt, 2);
        app.lastPlayedTime = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
        app.iconPath = metaIconPath(app.titleId);
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
    bool loadedFromDb = false;
    if (sqlite3_open_v2(PATH_APP_DB, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        const char* queries[] = {
            "SELECT titleId, titleName, uiCategory, lastPlayedTime FROM tbl_appinfo ORDER BY lastPlayedTime DESC;",
            "SELECT titleId, titleName, uiCategory, 0 FROM tbl_appinfo ORDER BY titleName COLLATE NOCASE;",
            "SELECT titleId, titleName, 0, 0 FROM appinfo ORDER BY titleName COLLATE NOCASE;"
        };
        for (const char* query : queries) {
            if (loadAppsQuery(db, query, apps)) {
                loadedFromDb = true;
                if (!apps.empty()) break;
            }
        }
        sqlite3_close(db);
    } else {
        LOGE("Could not open app.db; falling back to appmeta discovery");
        if (db) sqlite3_close(db);
    }

    scanAppMetadata(apps);
    std::sort(apps.begin(), apps.end(), [](const InstalledApp& a, const InstalledApp& b) { return a.titleName < b.titleName; });
    LOGI("Loaded %zu installed applications (%s)", apps.size(), loadedFromDb ? "app.db" : "appmeta");
    return !apps.empty();
}

bool AppEditor::updateAppTitle(const std::string& titleId, const std::string& newTitle) {
    if (newTitle.empty() || !backupDatabase()) return false;
    sqlite3* db = nullptr;
    if (sqlite3_open(PATH_APP_DB, &db) != SQLITE_OK) return false;
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(db, "UPDATE tbl_appinfo SET titleName = ? WHERE titleId = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, newTitle.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, titleId.c_str(), -1, SQLITE_TRANSIENT);
        success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    if (success) for (auto& app : apps) if (app.titleId == titleId) { app.titleName = newTitle; app.isModified = true; break; }
    return success;
}

bool AppEditor::updateAppIcon(const std::string& titleId, const std::string& newIconPath) {
    std::string targetMetaDir = metaDir(titleId);
    mkdir(targetMetaDir.c_str(), 0777);
    std::string targetIcon = targetMetaDir + "/icon0.png";
    if (!copyFile(newIconPath, targetIcon)) return false;
    for (auto& app : apps) if (app.titleId == titleId) { app.iconPath = targetIcon; app.isModified = true; break; }
    return true;
}

bool AppEditor::exportAssets(const std::string& titleId, const std::string& destinationDir) {
    mkdir(destinationDir.c_str(), 0777);
    std::string appDir = destinationDir + "/" + titleId;
    mkdir(appDir.c_str(), 0777);
    const std::string source = metaDir(titleId) + "/";
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
    std::sort(apps.begin(), apps.end(), [ascending](const InstalledApp& a, const InstalledApp& b) { return ascending ? a.titleName < b.titleName : a.titleName > b.titleName; });
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
    const uint64_t baseTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    bool success = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_stmt* stmt = nullptr;
    if (success && sqlite3_prepare_v2(db, "UPDATE tbl_appinfo SET lastPlayedTime = ? WHERE titleId = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
        for (size_t i = 0; i < apps.size(); ++i) {
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(baseTime - i * 100));
            sqlite3_bind_text(stmt, 2, apps[i].titleId.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE) success = false;
            sqlite3_reset(stmt);
            apps[i].lastPlayedTime = baseTime - i * 100;
        }
        sqlite3_finalize(stmt);
    } else success = false;
    sqlite3_exec(db, success ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return success;
}

bool AppEditor::softRestartShellCore() {
    LOGI("Triggering SceShellCore soft-restart to refresh XMB...");
    return system("killall -9 SceShellCore") == 0;
}

#include "catalog.h"
#include <fstream>
#include <sstream>
#include <algorithm>

CatalogEngine::CatalogEngine() {
    // Default repository paths
    repositoryUrls.push_back("http://127.0.0.1:8080/games.json");
}

CatalogEngine::~CatalogEngine() {}

static std::string extractJsonField(const std::string& obj, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = obj.find(needle);
    if (pos == std::string::npos) return "";

    size_t colon = obj.find(':', pos + needle.length());
    if (colon == std::string::npos) return "";

    size_t valStart = obj.find_first_not_of(" \t\r\n", colon + 1);
    if (valStart == std::string::npos) return "";

    if (obj[valStart] == '"') {
        size_t valEnd = obj.find('"', valStart + 1);
        if (valEnd != std::string::npos) {
            return obj.substr(valStart + 1, valEnd - valStart - 1);
        }
    } else {
        size_t valEnd = obj.find_first_of(",}\r\n", valStart);
        if (valEnd != std::string::npos) {
            return obj.substr(valStart, valEnd - valStart);
        }
    }
    return "";
}

bool CatalogEngine::parseJsonString(const std::string& jsonStr) {
    size_t cursor = 0;
    int count = 0;

    while ((cursor = jsonStr.find('{', cursor)) != std::string::npos) {
        size_t endObj = jsonStr.find('}', cursor);
        if (endObj == std::string::npos) break;

        std::string objStr = jsonStr.substr(cursor, endObj - cursor + 1);

        CatalogItem item;
        item.titleId  = extractJsonField(objStr, "title_id");
        if (item.titleId.empty()) item.titleId = extractJsonField(objStr, "id");

        item.name     = extractJsonField(objStr, "name");
        if (item.name.empty()) item.name = extractJsonField(objStr, "title");

        item.region   = extractJsonField(objStr, "region");
        item.version  = extractJsonField(objStr, "version");
        item.pkgUrl   = extractJsonField(objStr, "url");
        if (item.pkgUrl.empty()) item.pkgUrl = extractJsonField(objStr, "pkg");

        item.iconUrl  = extractJsonField(objStr, "icon");
        item.type     = extractJsonField(objStr, "type");
        if (item.type.empty()) item.type = "Game";

        std::string sizeStr = extractJsonField(objStr, "size");
        item.sizeBytes = sizeStr.empty() ? 0 : std::stoull(sizeStr);

        if (!item.name.empty() && !item.pkgUrl.empty()) {
            items.push_back(item);
            count++;
        }

        cursor = endObj + 1;
    }

    LOGI("Parsed %d items from catalog JSON", count);
    return count > 0;
}

bool CatalogEngine::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    LOGI("Loading catalog from file: %s", path.c_str());
    return parseJsonString(buffer.str());
}

bool CatalogEngine::loadFromUrl(const std::string& repoUrl) {
    LOGI("Fetching catalog from URL: %s", repoUrl.c_str());
    std::string tmpPath = "/data/AeroPKG/temp_repo.json";
    std::string cmd = "curl -s -o " + tmpPath + " \"" + repoUrl + "\"";
    system(cmd.c_str());
    return loadFromFile(tmpPath);
}

bool CatalogEngine::loadCatalog() {
    items.clear();

    // 1. Try local storage file
    if (loadFromFile(std::string(PATH_AEROPKG_DATA) + "/games.json")) return true;

    // 2. Try USB root files
    if (loadFromFile(std::string(PATH_USB0) + "/games.json")) return true;
    if (loadFromFile(std::string(PATH_USB1) + "/games.json")) return true;

    // 3. Fallback mock entries if no repo is loaded yet
    CatalogItem sample1 = {"CUSA07010", "Horizon Zero Dawn Complete Edition", "US", "01.00", "Game", 45000000000ULL, "http://pkg.example.com/hzd.pkg", ""};
    CatalogItem sample2 = {"CUSA08519", "God of War", "US", "01.00", "Game", 38000000000ULL, "http://pkg.example.com/gow.pkg", ""};
    items.push_back(sample1);
    items.push_back(sample2);

    return true;
}

static std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

std::vector<CatalogItem> CatalogEngine::search(const std::string& query, const std::string& regionFilter) {
    std::vector<CatalogItem> results;
    std::string q = toLower(query);

    for (const auto& item : items) {
        if (!regionFilter.empty() && item.region != regionFilter) {
            continue;
        }

        if (q.empty() ||
            toLower(item.name).find(q) != std::string::npos ||
            toLower(item.titleId).find(q) != std::string::npos) {
            results.push_back(item);
        }
    }
    return results;
}

void CatalogEngine::addRepository(const std::string& url) {
    repositoryUrls.push_back(url);
}

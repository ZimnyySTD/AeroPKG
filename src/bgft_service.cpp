#include "bgft_service.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <set>

BgftService::BgftService() {}
BgftService::~BgftService() { shutdown(); }

bool BgftService::init() {
    if (workerRunning) return true;
    workerRunning = true;
    workerThread = std::thread(&BgftService::workerLoop, this);
    LOGI("BgftService background worker started");
    return true;
}

void BgftService::shutdown() {
    workerRunning = false;
    if (workerThread.joinable()) workerThread.join();
    installingActive = false;
}

static std::string lowerName(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

static void scanUsbDirectory(const std::string& root, int depth, std::vector<DiscoveredPkg>& output, std::set<std::string>& seen) {
    if (depth > 8) return;
    DIR* dir = opendir(root.c_str());
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string fullPath = root + "/" + entry->d_name;
        struct stat st{};
        if (lstat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scanUsbDirectory(fullPath, depth + 1, output, seen);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        std::string lower = lowerName(entry->d_name);
        if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".pkg") continue;
        if (!seen.insert(fullPath).second) continue;

        DiscoveredPkg pkg;
        pkg.filename = entry->d_name;
        pkg.fullPath = fullPath;
        pkg.sizeBytes = static_cast<uint64_t>(st.st_size);
        pkg.isSelected = true;
        pkg.appName = pkg.filename.substr(0, pkg.filename.size() - 4);
        pkg.titleId = "UNKNOWN";
        output.push_back(pkg);
    }
    closedir(dir);
}

void BgftService::scanUsbPackages() {
    discoveredPkgs.clear();
    std::set<std::string> seen;
    const char* roots[] = {PATH_USB0, PATH_USB1, PATH_EXT0, PATH_DATA_PKG};
    for (const char* root : roots) scanUsbDirectory(root, 0, discoveredPkgs, seen);
    std::sort(discoveredPkgs.begin(), discoveredPkgs.end(), [](const DiscoveredPkg& a, const DiscoveredPkg& b) { return a.filename < b.filename; });
    LOGI("USB package scan found %zu package(s)", discoveredPkgs.size());
}

bool BgftService::enqueuePackage(const std::string& pathOrUrl, const std::string& title, bool fromUsb) {
    if (pathOrUrl.empty()) return false;
    std::lock_guard<std::mutex> lock(taskMutex);
    for (const auto& task : tasks) if (task.filePathOrUrl == pathOrUrl && task.status != "Failed") return true;
    tasks.push_back({title, pathOrUrl, fromUsb, 0, "Queued"});
    LOGI("Enqueued PKG: %s (%s)", title.c_str(), pathOrUrl.c_str());
    return true;
}

bool BgftService::enqueueBatchSelected() {
    int count = 0;
    for (const auto& pkg : discoveredPkgs) if (pkg.isSelected && enqueuePackage(pkg.fullPath, pkg.appName, true)) ++count;
    return count > 0;
}

bool BgftService::enqueueAllDiscovered() {
    int count = 0;
    for (const auto& pkg : discoveredPkgs) if (enqueuePackage(pkg.fullPath, pkg.appName, true)) ++count;
    return count > 0;
}

bool BgftService::sendBgftInstall(const std::string& pkgPathOrUrl) {
    if (!pkgPathOrUrl.empty() && pkgPathOrUrl[0] == '/') {
        std::string cmd = "curl --fail --silent --show-error --max-time 30 "
                          "--form 'pkg=@" + pkgPathOrUrl + "' "
                          "http://127.0.0.1:12800/api/install";
        return system(cmd.c_str()) == 0;
    }
    std::string payload = "{\"type\":\"direct\",\"packages\":[\"" + pkgPathOrUrl + "\"]}";
    std::string cmd = "curl --fail --silent --show-error --max-time 30 "
                      "-H 'Content-Type: application/json' "
                      "-X POST http://127.0.0.1:12800/api/install "
                      "-d '" + payload + "'";
    return system(cmd.c_str()) == 0;
}

std::vector<PkgInstallTask> BgftService::getActiveTasks() {
    std::lock_guard<std::mutex> lock(taskMutex);
    return tasks;
}

void BgftService::workerLoop() {
    while (workerRunning) {
        size_t index = 0;
        PkgInstallTask task;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            for (size_t i = 0; i < tasks.size(); ++i) {
                if (tasks[i].status == "Queued") {
                    index = i;
                    task = tasks[i];
                    tasks[i].status = "Installing";
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }
        installingActive = true;
        const bool sent = sendBgftInstall(task.filePathOrUrl);
        std::lock_guard<std::mutex> lock(taskMutex);
        if (index < tasks.size() && tasks[index].filePathOrUrl == task.filePathOrUrl) {
            tasks[index].status = sent ? "Completed" : "Failed";
            tasks[index].progressPercent = sent ? 100 : 0;
        }
        installingActive = false;
    }
}

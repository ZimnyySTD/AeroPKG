#include "bgft_service.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

BgftService::BgftService() {}

BgftService::~BgftService() {
    shutdown();
}

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

static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

void BgftService::scanUsbPackages() {
    discoveredPkgs.clear();
    const char* usbMounts[] = { PATH_USB0, PATH_USB1, "/mnt/ext0" };
    for (const char* mount : usbMounts) {
        DIR* dir = opendir(mount);
        if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string fname = entry->d_name;
            if (!endsWith(fname, ".pkg") && !endsWith(fname, ".PKG")) continue;

            std::string fullPath = std::string(mount) + "/" + fname;
            struct stat st{};
            if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
            DiscoveredPkg pkg;
            pkg.filename = fname;
            pkg.fullPath = fullPath;
            pkg.sizeBytes = static_cast<uint64_t>(st.st_size);
            pkg.isSelected = true;
            pkg.appName = fname.substr(0, fname.find_last_of('.'));
            pkg.titleId = "UNKNOWN";
            discoveredPkgs.push_back(pkg);
        }
        closedir(dir);
    }
    LOGI("USB package scan found %zu package(s)", discoveredPkgs.size());
}

bool BgftService::enqueuePackage(const std::string& pathOrUrl, const std::string& title, bool fromUsb) {
    if (pathOrUrl.empty()) return false;
    std::lock_guard<std::mutex> lock(taskMutex);
    for (const auto& task : tasks) {
        if (task.filePathOrUrl == pathOrUrl && task.status != "Failed") return true;
    }
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
    std::string payload = "{\"type\":\"direct\",\"packages\":[\"" + pkgPathOrUrl + "\"]}";
    std::string cmd = "curl -s -X POST http://127.0.0.1:12800/api/install -d '" + payload + "'";
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

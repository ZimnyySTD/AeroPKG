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
    workerRunning = true;
    workerThread = std::thread(&BgftService::workerLoop, this);
    LOGI("BgftService background worker started");
    return true;
}

void BgftService::shutdown() {
    if (workerRunning) {
        workerRunning = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
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

            if (endsWith(fname, ".pkg") || endsWith(fname, ".PKG")) {
                std::string fullPath = std::string(mount) + "/" + fname;
                struct stat st;
                stat(fullPath.c_str(), &st);

                DiscoveredPkg pkg;
                pkg.filename = fname;
                pkg.fullPath = fullPath;
                pkg.sizeBytes = st.st_size;
                pkg.isSelected = true;

                // Simple title / id heuristic from filename
                pkg.appName = fname.substr(0, fname.find_last_of('.'));
                pkg.titleId = "UNKNOWN";

                discoveredPkgs.push_back(pkg);
                LOGI("Found USB package: %s (%llu MB)", fullPath.c_str(), (unsigned long long)(st.st_size / (1024 * 1024)));
            }
        }
        closedir(dir);
    }
}

bool BgftService::enqueuePackage(const std::string& pathOrUrl, const std::string& title, bool fromUsb) {
    std::lock_guard<std::mutex> lock(taskMutex);
    PkgInstallTask task;
    task.title = title;
    task.filePathOrUrl = pathOrUrl;
    task.isFromUsb = fromUsb;
    task.progressPercent = 0;
    task.status = "Queued";
    tasks.push_back(task);
    LOGI("Enqueued PKG: %s (%s)", title.c_str(), pathOrUrl.c_str());
    return true;
}

bool BgftService::enqueueBatchSelected() {
    int count = 0;
    for (const auto& pkg : discoveredPkgs) {
        if (pkg.isSelected) {
            enqueuePackage(pkg.fullPath, pkg.appName, true);
            count++;
        }
    }
    LOGI("Enqueued %d selected packages from USB", count);
    return count > 0;
}

bool BgftService::enqueueAllDiscovered() {
    for (const auto& pkg : discoveredPkgs) {
        enqueuePackage(pkg.fullPath, pkg.appName, true);
    }
    return !discoveredPkgs.empty();
}

bool BgftService::sendBgftInstall(const std::string& pkgPathOrUrl) {
    LOGI("Triggering BGFT installation for %s", pkgPathOrUrl.c_str());

    // Send HTTP POST to GoldHEN / Remote Package Installer daemon (127.0.0.1:12800/api/install)
    // payload: {"type": "direct", "packages": ["<pathOrUrl>"]}
    std::string payload = "{\"type\":\"direct\",\"packages\":[\"" + pkgPathOrUrl + "\"]}";
    std::string cmd = "curl -s -X POST http://127.0.0.1:12800/api/install -d '" + payload + "' &";
    system(cmd.c_str());
    return true;
}

std::vector<PkgInstallTask> BgftService::getActiveTasks() {
    std::lock_guard<std::mutex> lock(taskMutex);
    return tasks;
}

void BgftService::workerLoop() {
    while (workerRunning) {
        PkgInstallTask* currentTask = nullptr;

        {
            std::lock_guard<std::mutex> lock(taskMutex);
            for (auto& task : tasks) {
                if (task.status == "Queued") {
                    currentTask = &task;
                    break;
                }
            }
        }

        if (currentTask) {
            installingActive = true;
            currentTask->status = "Installing";
            LOGI("Starting install task: %s", currentTask->title.c_str());

            sendBgftInstall(currentTask->filePathOrUrl);

            // Wait brief moment for BGFT registration
            std::this_thread::sleep_for(std::chrono::seconds(2));
            currentTask->status = "Completed";
            currentTask->progressPercent = 100;
            installingActive = false;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

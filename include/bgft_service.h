#pragma once

#include "common.h"
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

struct DiscoveredPkg {
    std::string filename;
    std::string fullPath;
    std::string titleId;
    std::string appName;
    uint64_t sizeBytes;
    bool isSelected;
};

class BgftService {
public:
    BgftService();
    ~BgftService();

    bool init();
    void shutdown();

    // USB Discovery
    void scanUsbPackages();
    std::vector<DiscoveredPkg>& getDiscoveredPkgs() { return discoveredPkgs; }

    // Queue & Installation
    bool enqueuePackage(const std::string& pathOrUrl, const std::string& title, bool fromUsb);
    bool enqueueBatchSelected();
    bool enqueueAllDiscovered();

    // Trigger installation via BGFT / GoldHEN
    bool sendBgftInstall(const std::string& pkgPathOrUrl);

    // Active Queue Inspection
    std::vector<PkgInstallTask> getActiveTasks();
    bool isInstalling() const { return installingActive; }

private:
    std::vector<DiscoveredPkg> discoveredPkgs;
    std::vector<PkgInstallTask> tasks;
    std::mutex taskMutex;

    std::atomic<bool> workerRunning{false};
    std::atomic<bool> installingActive{false};
    std::thread workerThread;

    void workerLoop();
};

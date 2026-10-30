#pragma once

#include "common.h"
#include <vector>
#include <string>

class AppEditor {
public:
    AppEditor();
    ~AppEditor();

    // Query & Loading
    bool refreshInstalledApps();
    const std::vector<InstalledApp>& getInstalledApps() const { return apps; }

    // Backup & Safety
    bool backupDatabase();
    bool restoreDatabase();

    // Customization operations
    bool updateAppTitle(const std::string& titleId, const std::string& newTitle);
    bool updateAppIcon(const std::string& titleId, const std::string& newIconPath);
    bool exportAssets(const std::string& titleId, const std::string& destinationDir);

    // XMB Order & Layout Management
    bool moveAppPosition(int currentIndex, int targetIndex);
    bool sortAppsAlphabetical(bool ascending = true);
    bool sortAppsBySize();
    bool sortAppsByLastPlayed();
    bool applyXmbOrder();

    // System Refresh
    bool softRestartShellCore();

private:
    std::vector<InstalledApp> apps;
    bool executeSql(const std::string& sqlQuery);
    std::string getMetaIconPath(const std::string& titleId);
};

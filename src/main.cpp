#include "common.h"
#include "renderer.h"
#include "input.h"
#include "app_editor.h"
#include "bgft_service.h"
#include "catalog.h"
#include "avatar_mgr.h"
#include "cleaner.h"
#include "web_server.h"

#include <iostream>
#include <string>

enum ViewTab {
    TAB_DASHBOARD = 0,    // Installed games, XMB reorder, icon/title editor
    TAB_USB_INSTALLER,    // Scan USB for PKGs, batch install all
    TAB_FPKGI_CATALOG,    // games.json repo browser, search, download
    TAB_AVATAR_SUITE,     // User profile avatar changer & presets
    TAB_STORAGE_CLEANER,  // Temporary cache & broken download purge
    TAB_COUNT
};

int main(int argc, char* argv[]) {
    LOGI("=============================================");
    LOGI("   AeroPKG v%s (Title ID: %s)", APP_VERSION, APP_TITLE_ID);
    LOGI("   Fast PS4 Package Manager & Dashboard Suite");
    LOGI("=============================================");

    Renderer renderer;
    if (!renderer.init(1920, 1080)) {
        LOGE("Failed to initialize 60 FPS graphics engine");
        return -1;
    }

    InputManager input;
    input.init();

    AppEditor appEditor;
    appEditor.refreshInstalledApps();

    BgftService bgftService;
    bgftService.init();

    CatalogEngine catalog;
    catalog.loadCatalog();

    AvatarManager avatarMgr;
    avatarMgr.init();

    StorageCleaner cleaner;

    WebServer webServer(&appEditor, &bgftService, &avatarMgr);
    webServer.start(WEB_SERVER_PORT);

    ViewTab currentTab = TAB_DASHBOARD;
    int selectedIndex = 0;
    bool isReorderMode = false;
    int reorderSourceIndex = -1;
    std::string statusMessage = "Ready. Web Control: http://" + webServer.getIpAddress() + ":8080";

    bool running = true;
    while (running && !input.shouldQuit()) {
        input.update();

        // Global Tab Navigation: L1 and R1
        if (input.isJustPressed(BTN_L1)) {
            currentTab = (ViewTab)((currentTab - 1 + TAB_COUNT) % TAB_COUNT);
            selectedIndex = 0;
            isReorderMode = false;
        }
        if (input.isJustPressed(BTN_R1)) {
            currentTab = (ViewTab)((currentTab + 1) % TAB_COUNT);
            selectedIndex = 0;
            isReorderMode = false;
        }

        // Handle Tab-Specific Inputs
        if (currentTab == TAB_DASHBOARD) {
            const auto& apps = appEditor.getInstalledApps();
            if (!apps.empty()) {
                if (input.isJustPressed(BTN_UP) && selectedIndex > 0) {
                    if (isReorderMode) {
                        appEditor.moveAppPosition(selectedIndex, selectedIndex - 1);
                    }
                    selectedIndex--;
                }
                if (input.isJustPressed(BTN_DOWN) && selectedIndex < (int)apps.size() - 1) {
                    if (isReorderMode) {
                        appEditor.moveAppPosition(selectedIndex, selectedIndex + 1);
                    }
                    selectedIndex++;
                }

                // Square: Toggle Interactive XMB Move Mode (1st, 2nd, 3rd)
                if (input.isJustPressed(BTN_SQUARE)) {
                    isReorderMode = !isReorderMode;
                    reorderSourceIndex = selectedIndex;
                    statusMessage = isReorderMode ? "Reorder Mode: Use UP/DOWN to move game slot, SQUARE to confirm"
                                                  : "Slot confirmed. Press START/OPTIONS to apply to PS4 Home Screen.";
                }

                // Triangle: Quick-Sort Presets
                if (input.isJustPressed(BTN_TRIANGLE)) {
                    appEditor.sortAppsAlphabetical();
                    statusMessage = "Sorted A-Z. Press OPTIONS to apply to PS4 Home Screen.";
                }

                // Options: Apply XMB Order & Soft-Restart ShellCore
                if (input.isJustPressed(BTN_OPTIONS)) {
                    appEditor.applyXmbOrder();
                    statusMessage = "Applying order to app.db and reloading PS4 ShellCore...";
                    appEditor.softRestartShellCore();
                }
            }
        } else if (currentTab == TAB_USB_INSTALLER) {
            if (input.isJustPressed(BTN_TRIANGLE)) {
                bgftService.scanUsbPackages();
                statusMessage = "Refreshed USB packages.";
            }
            if (input.isJustPressed(BTN_SQUARE)) {
                if (bgftService.enqueueAllDiscovered()) {
                    statusMessage = "Enqueued ALL USB packages for background installation!";
                } else {
                    statusMessage = "No USB packages found to install.";
                }
            }
        } else if (currentTab == TAB_STORAGE_CLEANER) {
            if (input.isJustPressed(BTN_TRIANGLE)) {
                cleaner.scanStorage();
                statusMessage = "Scanned cache & download remnants.";
            }
            if (input.isJustPressed(BTN_SQUARE)) {
                cleaner.purgeAll();
                statusMessage = "Storage purged successfully!";
            }
        }

        // ==========================================
        // Rendering Frame
        // ==========================================
        renderer.beginFrame();

        // 1. Top Header & Tab Bar
        renderer.fillRect(0, 0, 1920, 80, Color::CardBg());
        renderer.drawText("AeroPKG", 40, 24, 32, Color::AccentBlue());

        const char* tabNames[] = {
            "Dashboard & XMB Order",
            "USB PKG Installer",
            "FPKGi Catalog (games.json)",
            "User Avatar Suite",
            "Storage Cleaner"
        };

        int tabX = 260;
        for (int i = 0; i < TAB_COUNT; ++i) {
            Color tabCol = (i == currentTab) ? Color::AccentCyan() : Color::TextMuted();
            renderer.drawText(tabNames[i], tabX, 28, 22, tabCol);
            if (i == currentTab) {
                renderer.fillRect(tabX - 5, 74, renderer.getTextWidth(tabNames[i], 22) + 10, 4, Color::AccentCyan());
            }
            tabX += 290;
        }

        // 2. Main Tab Body
        if (currentTab == TAB_DASHBOARD) {
            const auto& apps = appEditor.getInstalledApps();
            renderer.drawText("Installed Games & Homebrew (Manage Order & Titles)", 40, 110, 26, Color::White());

            int y = 160;
            int startIdx = std::max(0, selectedIndex - 3);
            int endIdx = std::min((int)apps.size(), startIdx + 7);

            for (int i = startIdx; i < endIdx; ++i) {
                bool isSelected = (i == selectedIndex);
                Color cardColor = isSelected ? (isReorderMode ? Color::Warning() : Color::CardSelected()) : Color::CardBg();

                renderer.fillRect(40, y, 1840, 90, cardColor);
                if (isSelected) {
                    renderer.drawRect(40, y, 1840, 90, isReorderMode ? Color::Warning() : Color::AccentBlue());
                }

                // Slot number (1st, 2nd, etc.)
                std::string slotText = "#" + std::to_string(i + 1);
                renderer.drawText(slotText, 60, y + 30, 24, isSelected ? Color::AccentCyan() : Color::TextMuted());

                // Title Icon preview
                renderer.drawImage(apps[i].iconPath, 140, y + 10, 70, 70);

                // Title Name and ID
                renderer.drawText(apps[i].titleName, 230, y + 20, 26, Color::White());
                std::string sub = apps[i].titleId + " | Slot: " + std::to_string(i + 1);
                renderer.drawText(sub, 230, y + 54, 18, Color::TextMuted());

                y += 105;
            }
        } else if (currentTab == TAB_USB_INSTALLER) {
            renderer.drawText("USB Package Batch Installer", 40, 110, 26, Color::White());
            auto& pkgs = bgftService.getDiscoveredPkgs();
            if (pkgs.empty()) {
                renderer.drawText("No USB .pkg files detected in /mnt/usb0 or /mnt/usb1.", 60, 200, 22, Color::TextMuted());
                renderer.drawText("Press TRIANGLE to re-scan USB drives.", 60, 240, 20, Color::AccentCyan());
            } else {
                int y = 170;
                for (size_t i = 0; i < std::min((size_t)8, pkgs.size()); ++i) {
                    renderer.fillRect(40, y, 1840, 70, Color::CardBg());
                    renderer.drawText(pkgs[i].appName, 60, y + 20, 22, Color::White());
                    renderer.drawText(pkgs[i].fullPath, 600, y + 24, 18, Color::TextMuted());
                    y += 85;
                }
            }
        } else if (currentTab == TAB_FPKGI_CATALOG) {
            renderer.drawText("FPKGi Catalog & Repository Browser (games.json)", 40, 110, 26, Color::White());
            const auto& items = catalog.getAllItems();
            int y = 170;
            for (size_t i = 0; i < std::min((size_t)7, items.size()); ++i) {
                renderer.fillRect(40, y, 1840, 80, Color::CardBg());
                renderer.drawText(items[i].name, 60, y + 16, 24, Color::White());
                std::string meta = items[i].titleId + " [" + items[i].region + "] - " + items[i].type;
                renderer.drawText(meta, 60, y + 48, 18, Color::AccentCyan());
                y += 95;
            }
        } else if (currentTab == TAB_AVATAR_SUITE) {
            renderer.drawText("Current User Profile Avatar Suite", 40, 110, 26, Color::White());
            renderer.drawText("User ID: " + avatarMgr.getCurrentUserId(), 60, 170, 22, Color::AccentCyan());
            renderer.drawText("Upload via Web: Open http://" + webServer.getIpAddress() + ":8080 on your phone/PC", 60, 210, 22, Color::White());
            renderer.drawImage(avatarMgr.getCurrentAvatarPath(), 60, 260, 160, 160);
        } else if (currentTab == TAB_STORAGE_CLEANER) {
            renderer.drawText("Storage & Residual Package Cleaner", 40, 110, 26, Color::White());
            std::string rec = "Reclaimable Storage: " + std::to_string(cleaner.getTotalReclaimableBytes() / (1024 * 1024)) + " MB";
            renderer.drawText(rec, 60, 170, 24, Color::Warning());
            renderer.drawText("Press TRIANGLE to Scan, SQUARE to Purge all temporary download remnants", 60, 220, 20, Color::TextMuted());
        }

        // 3. Bottom Status Bar & Controller Guide
        renderer.fillRect(0, 1010, 1920, 70, Color::CardBg());
        renderer.drawText(statusMessage, 40, 1030, 20, Color::AccentCyan());

        std::string controls = "[L1/R1] Tabs   [X] Select   [SQUARE] Move/Batch   [TRIANGLE] Options/Scan   [OPTIONS] Reload XMB";
        renderer.drawText(controls, 1050, 1030, 20, Color::TextMuted());

        renderer.endFrame();
    }

    webServer.stop();
    bgftService.shutdown();
    renderer.shutdown();

    LOGI("AeroPKG shutdown cleanly.");
    return 0;
}

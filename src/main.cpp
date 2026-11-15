#include "common.h"
#include "renderer.h"
#include "input.h"
#include "app_editor.h"
#include "bgft_service.h"
#include "catalog.h"
#include "avatar_mgr.h"
#include "cleaner.h"
#include "web_server.h"

#ifdef __ORBIS__
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#endif

#include <algorithm>
#include <string>

namespace {
enum ViewTab { TAB_HOME = 0, TAB_USB, TAB_CATALOG, TAB_AVATAR, TAB_CLEANER, TAB_COUNT };

const char* tabName(ViewTab tab) {
    static const char* names[] = {"Home", "USB Installer", "Catalog", "Avatar", "Cleaner"};
    return names[tab];
}

std::string formatBytes(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) return std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + " GB";
    if (bytes >= 1024ULL * 1024ULL) return std::to_string(bytes / (1024ULL * 1024ULL)) + " MB";
    if (bytes >= 1024ULL) return std::to_string(bytes / 1024ULL) + " KB";
    return std::to_string(bytes) + " B";
}

void panel(Renderer& renderer, int x, int y, int w, int h, Color fill, bool selected = false) {
    renderer.fillRect(x, y, w, h, fill);
    if (selected) renderer.drawRect(x, y, w, h, Color::AccentCyan());
}

void header(Renderer& renderer, ViewTab active, const std::string& status) {
    renderer.fillRect(0, 0, 1920, 92, Color::CardBg());
    renderer.drawText("AeroPKG", 48, 24, 34, Color::AccentCyan());
    renderer.drawText("PS4 APP MANAGER", 50, 62, 14, Color::TextMuted());
    int x = 430;
    for (int i = 0; i < TAB_COUNT; ++i) {
        ViewTab tab = static_cast<ViewTab>(i);
        Color color = tab == active ? Color::White() : Color::TextMuted();
        renderer.drawText(tabName(tab), x, 34, 22, color);
        if (tab == active) renderer.fillRect(x, 76, renderer.getTextWidth(tabName(tab), 22), 4, Color::AccentCyan());
        x += 285;
    }
    renderer.fillRect(0, 1018, 1920, 62, Color::CardBg());
    renderer.drawText(status, 48, 1038, 19, Color::AccentCyan());
    renderer.drawText("L1/R1 Tabs   UP/DOWN Browse   X Select   SQUARE Action   TRIANGLE Refresh   OPTIONS Apply", 950, 1038, 16, Color::TextMuted());
}
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef __ORBIS__
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_VIDEO_OUT);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
#endif

    Renderer renderer;
    if (!renderer.init(1920, 1080)) return -1;
    InputManager input;
    input.init();

    AppEditor appEditor;
    bool appsLoaded = appEditor.refreshInstalledApps();
    BgftService bgft;
    bgft.init();
    bgft.scanUsbPackages();
    CatalogEngine catalog;
    catalog.loadCatalog();
    AvatarManager avatar;
    avatar.init();
    StorageCleaner cleaner;
    cleaner.scanStorage();
    WebServer webServer(&appEditor, &bgft, &avatar);
    const bool webEnabled = webServer.start();

    ViewTab tab = TAB_HOME;
    int selected = 0;
    bool reorderMode = false;
    bool running = true;
    std::string status = appsLoaded
        ? "Ready - " + std::to_string(appEditor.getInstalledApps().size()) + " app(s), " + std::to_string(bgft.getDiscoveredPkgs().size()) + " package(s), " + formatBytes(cleaner.getSystemFreeBytes()) + " free"
        : "No apps in app.db; scanning app metadata";

    while (running && !input.shouldQuit()) {
        input.update();
        if (input.isJustPressed(BTN_L1)) { tab = static_cast<ViewTab>((tab + TAB_COUNT - 1) % TAB_COUNT); selected = 0; reorderMode = false; }
        if (input.isJustPressed(BTN_R1)) { tab = static_cast<ViewTab>((tab + 1) % TAB_COUNT); selected = 0; reorderMode = false; }

        const auto& apps = appEditor.getInstalledApps();
        const auto& catalogItems = catalog.getAllItems();
        auto& pkgs = bgft.getDiscoveredPkgs();

        if (tab == TAB_HOME) {
            if (input.isJustPressed(BTN_UP) && selected > 0) { if (reorderMode) appEditor.moveAppPosition(selected, selected - 1); --selected; }
            if (input.isJustPressed(BTN_DOWN) && selected + 1 < (int)apps.size()) { if (reorderMode) appEditor.moveAppPosition(selected, selected + 1); ++selected; }
            if (input.isJustPressed(BTN_SQUARE) && !apps.empty()) {
                reorderMode = !reorderMode;
                status = reorderMode ? "Move mode: use UP/DOWN, then SQUARE to finish" : "Order staged - press OPTIONS to apply to XMB";
            }
            if (input.isJustPressed(BTN_TRIANGLE)) {
                appsLoaded = appEditor.refreshInstalledApps();
                selected = 0;
                status = appsLoaded ? "Refreshed installed games and apps" : "No apps found in app.db or /user/appmeta";
            }
            if (input.isJustPressed(BTN_OPTIONS)) {
                status = appEditor.applyXmbOrder() ? "XMB order saved - restarting ShellCore" : "Could not save XMB order";
                if (status.find("restarting") != std::string::npos) appEditor.softRestartShellCore();
            }
            if (input.isJustPressed(BTN_CROSS) && !apps.empty()) status = "Selected: " + apps[selected].titleName + " (" + apps[selected].titleId + ")";
        } else if (tab == TAB_USB) {
            if (input.isJustPressed(BTN_TRIANGLE)) { bgft.scanUsbPackages(); selected = 0; status = "USB scan complete: " + std::to_string(pkgs.size()) + " package(s)"; }
            if (input.isJustPressed(BTN_UP) && selected > 0) --selected;
            if (input.isJustPressed(BTN_DOWN) && selected + 1 < (int)pkgs.size()) ++selected;
            if (input.isJustPressed(BTN_SQUARE)) { status = bgft.enqueueAllDiscovered() ? "All USB packages queued for installation" : "No USB packages found"; }
            if (input.isJustPressed(BTN_CROSS) && selected < (int)pkgs.size()) { bgft.enqueuePackage(pkgs[selected].fullPath, pkgs[selected].appName, true); status = "Queued: " + pkgs[selected].appName; }
        } else if (tab == TAB_CATALOG) {
            if (input.isJustPressed(BTN_UP) && selected > 0) --selected;
            if (input.isJustPressed(BTN_DOWN) && selected + 1 < (int)catalogItems.size()) ++selected;
            if (input.isJustPressed(BTN_TRIANGLE)) { catalog.loadCatalog(); selected = 0; status = catalog.getAllItems().empty() ? "No real games.json found on PS4 or USB" : "Catalog refreshed"; }
            if (input.isJustPressed(BTN_CROSS) && selected < (int)catalogItems.size()) {
                const auto& item = catalogItems[selected];
                bgft.enqueuePackage(item.pkgUrl, item.name, false);
                status = "Queued catalog item: " + item.name;
            }
        } else if (tab == TAB_AVATAR) {
            if (input.isJustPressed(BTN_TRIANGLE)) { auto found = avatar.scanUsbAvatars(); status = "Found " + std::to_string(found.size()) + " avatar image(s) on USB"; }
            if (input.isJustPressed(BTN_SQUARE)) {
                auto presets = avatar.getAvailablePresets();
                if (!presets.empty()) status = avatar.applyAvatar(presets[0].path) ? "Applied avatar preset: " + presets[0].name : "Avatar preset is not installed";
            }
        } else if (tab == TAB_CLEANER) {
            if (input.isJustPressed(BTN_TRIANGLE)) { cleaner.scanStorage(); selected = 0; status = "Storage scan complete: " + formatBytes(cleaner.getTotalReclaimableBytes()) + " reclaimable"; }
            if (input.isJustPressed(BTN_SQUARE)) { status = cleaner.purgeAll() ? "Selected residual files removed" : "Some files could not be removed"; }
        }

        renderer.beginFrame();
        header(renderer, tab, status);

        if (tab == TAB_HOME) {
            renderer.drawText("Installed games and applications", 48, 132, 30, Color::White());
            renderer.drawText(std::to_string(apps.size()) + " items   " + (reorderMode ? "MOVE MODE" : "READY"), 48, 170, 18, reorderMode ? Color::Warning() : Color::TextMuted());
            if (apps.empty()) {
                panel(renderer, 48, 220, 1824, 170, Color::CardBg());
                renderer.drawText("No installed applications were loaded", 88, 265, 26, Color::White());
                renderer.drawText("Press TRIANGLE to rescan app.db and /user/appmeta.", 88, 312, 20, Color::TextMuted());
            } else {
                int start = std::max(0, selected - 4);
                int end = std::min((int)apps.size(), start + 7);
                int y = 220;
                for (int i = start; i < end; ++i) {
                    const auto& app = apps[i];
                    bool chosen = i == selected;
                    panel(renderer, 48, y, 1824, 92, chosen ? Color::CardSelected() : Color::CardBg(), chosen);
                    renderer.drawText(std::to_string(i + 1), 72, y + 31, 20, chosen ? Color::AccentCyan() : Color::TextMuted());
                    renderer.drawImage(app.iconPath, 130, y + 10, 72, 72);
                    renderer.drawText(app.titleName, 230, y + 18, 25, Color::White());
                    renderer.drawText(app.titleId + "   " + (app.category == 0 ? "GAME" : "APP") + (app.version.empty() ? "" : "   " + app.version), 230, y + 55, 17, Color::TextMuted());
                    y += 104;
                }
            }
        } else if (tab == TAB_USB) {
            renderer.drawText("Install packages from USB", 48, 132, 30, Color::White());
            renderer.drawText(std::to_string(pkgs.size()) + " packages found", 48, 170, 18, Color::TextMuted());
            int y = 220;
            for (size_t i = 0; i < std::min((size_t)7, pkgs.size()); ++i) {
                bool chosen = (int)i == selected;
                panel(renderer, 48, y, 1824, 88, chosen ? Color::CardSelected() : Color::CardBg(), chosen);
                renderer.drawText(pkgs[i].appName, 82, y + 18, 24, Color::White());
                renderer.drawText(pkgs[i].fullPath + "   " + formatBytes(pkgs[i].sizeBytes), 82, y + 53, 17, Color::TextMuted());
                y += 100;
            }
            if (pkgs.empty()) renderer.drawText("No .pkg files found. Check /mnt/usb0, /mnt/usb1, /mnt/ext0, or /data/pkg.", 82, 250, 22, Color::TextMuted());
        } else if (tab == TAB_CATALOG) {
            renderer.drawText("FPKGi catalog", 48, 132, 30, Color::White());
            renderer.drawText(std::to_string(catalogItems.size()) + " real catalog entries   X queues the selected package", 48, 170, 18, Color::TextMuted());
            int y = 220;
            for (size_t i = 0; i < std::min((size_t)7, catalogItems.size()); ++i) {
                bool chosen = (int)i == selected;
                panel(renderer, 48, y, 1824, 88, chosen ? Color::CardSelected() : Color::CardBg(), chosen);
                renderer.drawText(catalogItems[i].name, 82, y + 16, 23, Color::White());
                renderer.drawText(catalogItems[i].titleId + "   " + catalogItems[i].region + "   " + catalogItems[i].type, 82, y + 51, 17, Color::TextMuted());
                y += 100;
            }
            if (catalogItems.empty()) renderer.drawText("No games.json found in /data/AeroPKG, USB, extended storage, or /data/pkg.", 82, 250, 22, Color::TextMuted());
        } else if (tab == TAB_AVATAR) {
            renderer.drawText("Avatar suite", 48, 132, 30, Color::White());
            panel(renderer, 48, 210, 440, 520, Color::CardBg(), true);
            renderer.drawImage(avatar.getCurrentAvatarPath(), 118, 275, 300, 300);
            renderer.drawText("User " + avatar.getCurrentUserId(), 130, 615, 21, Color::White());
            renderer.drawText(webEnabled ? "Remote control ready" : "USB and offline mode", 130, 650, 17, Color::TextMuted());
            renderer.drawText("SQUARE applies the first bundled preset", 560, 280, 23, Color::White());
            renderer.drawText("TRIANGLE scans USB for PNG/JPG avatars", 560, 325, 20, Color::TextMuted());
        } else if (tab == TAB_CLEANER) {
            renderer.drawText("Storage cleaner", 48, 132, 30, Color::White());
            panel(renderer, 48, 210, 560, 230, Color::CardBg());
            renderer.drawText(formatBytes(cleaner.getSystemFreeBytes()), 88, 260, 48, Color::AccentCyan());
            renderer.drawText("free system storage", 90, 325, 22, Color::TextMuted());
            renderer.drawText(formatBytes(cleaner.getSystemTotalBytes()) + " total", 90, 365, 18, Color::TextMuted());
            renderer.drawText(formatBytes(cleaner.getTotalReclaimableBytes()) + " reclaimable cache", 90, 395, 18, Color::Warning());
            renderer.drawText("TRIANGLE scans package, download, GoldHEN and notification caches", 700, 270, 22, Color::White());
            renderer.drawText("SQUARE removes the selected residual files", 700, 315, 20, Color::TextMuted());
        }

        renderer.endFrame();
    }

    if (webEnabled) webServer.stop();
    bgft.shutdown();
    renderer.shutdown();
    return 0;
}

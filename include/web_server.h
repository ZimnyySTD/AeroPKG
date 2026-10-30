#pragma once

#include "common.h"
#include <string>
#include <thread>
#include <atomic>

class AppEditor;
class BgftService;
class AvatarManager;

class WebServer {
public:
    WebServer(AppEditor* editor, BgftService* bgft, AvatarManager* avatar);
    ~WebServer();

    bool start(int port = WEB_SERVER_PORT);
    void stop();

    std::string getIpAddress() const;

private:
    AppEditor* appEditor = nullptr;
    BgftService* bgftService = nullptr;
    AvatarManager* avatarMgr = nullptr;

    int serverPort = WEB_SERVER_PORT;
    int serverSocket = -1;
    std::atomic<bool> isRunning{false};
    std::thread serverThread;

    void runServer();
    void handleClient(int clientSocket);
};

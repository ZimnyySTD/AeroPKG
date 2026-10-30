#include "web_server.h"
#include "app_editor.h"
#include "bgft_service.h"
#include "avatar_mgr.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <cstring>
#include <ifaddrs.h>

WebServer::WebServer(AppEditor* editor, BgftService* bgft, AvatarManager* avatar)
    : appEditor(editor), bgftService(bgft), avatarMgr(avatar) {}

WebServer::~WebServer() {
    stop();
}

std::string WebServer::getIpAddress() const {
    std::string ip = "127.0.0.1";
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return ip;

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char host[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                std::string name = ifa->ifa_name;
                if (name.find("eth") != std::string::npos || name.find("wlan") != std::string::npos || name.find("en") != std::string::npos) {
                    ip = host;
                    break;
                }
            }
        }
    }
    freeifaddrs(ifaddr);
    return ip;
}

bool WebServer::start(int port) {
    serverPort = port;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOGE("Failed to create web server socket");
        return false;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(serverPort);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("Failed to bind web server to port %d", serverPort);
        close(serverSocket);
        serverSocket = -1;
        return false;
    }

    if (listen(serverSocket, 10) < 0) {
        LOGE("Failed to listen on web server socket");
        close(serverSocket);
        serverSocket = -1;
        return false;
    }

    isRunning = true;
    serverThread = std::thread(&WebServer::runServer, this);
    LOGI("AeroPKG Web Server active at http://%s:%d", getIpAddress().c_str(), serverPort);
    return true;
}

void WebServer::stop() {
    if (isRunning) {
        isRunning = false;
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
        LOGI("Web server stopped");
    }
}

void WebServer::runServer() {
    while (isRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSocket >= 0) {
            handleClient(clientSocket);
            close(clientSocket);
        } else {
            usleep(10000);
        }
    }
}

static void sendHttpResponse(int sock, const std::string& contentType, const std::string& body, int status = 200) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;

    std::string respStr = response.str();
    send(sock, respStr.c_str(), respStr.length(), 0);
}

void WebServer::handleClient(int sock) {
    char buffer[4096];
    int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) return;
    buffer[bytesRead] = '\0';

    std::string req(buffer);
    std::string method, path;
    std::istringstream stream(req);
    stream >> method >> path;

    if (method == "OPTIONS") {
        sendHttpResponse(sock, "text/plain", "OK");
        return;
    }

    // Endpoint: GET /api/apps
    if (method == "GET" && path == "/api/apps") {
        std::ostringstream json;
        json << "[";
        const auto& apps = appEditor->getInstalledApps();
        for (size_t i = 0; i < apps.size(); ++i) {
            json << "{\"titleId\":\"" << apps[i].titleId << "\","
                 << "\"name\":\"" << apps[i].titleName << "\","
                 << "\"lastPlayed\":" << apps[i].lastPlayedTime << "}";
            if (i + 1 < apps.size()) json << ",";
        }
        json << "]";
        sendHttpResponse(sock, "application/json", json.str());
        return;
    }

    // Endpoint: POST /api/install (Remote Package Sender v2 compatible)
    if (method == "POST" && path == "/api/install") {
        size_t bodyPos = req.find("\r\n\r\n");
        if (bodyPos != std::string::npos) {
            std::string body = req.substr(bodyPos + 4);
            // Example body: {"type":"direct","packages":["http://192.168.1.100/game.pkg"]}
            size_t pkgPos = body.find("http");
            if (pkgPos != std::string::npos) {
                size_t endUrl = body.find_first_of("\"'", pkgPos);
                std::string url = body.substr(pkgPos, endUrl - pkgPos);
                bgftService->enqueuePackage(url, "Remote PKG Install", false);
                sendHttpResponse(sock, "application/json", "{\"status\":\"success\"}");
                return;
            }
        }
        sendHttpResponse(sock, "application/json", "{\"status\":\"queued\"}");
        return;
    }

    // Endpoint: POST /api/restart_shell
    if (method == "POST" && path == "/api/restart_shell") {
        appEditor->softRestartShellCore();
        sendHttpResponse(sock, "application/json", "{\"status\":\"restarting\"}");
        return;
    }

    // Serve HTML Dashboard for browser visits
    if (path == "/" || path == "/index.html") {
        std::ifstream htmlFile("/app0/web/index.html");
        if (htmlFile.is_open()) {
            std::stringstream ss;
            ss << htmlFile.rdbuf();
            sendHttpResponse(sock, "text/html", ss.str());
        } else {
            // Built-in standalone responsive HTML UI
            std::string embeddedHtml = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>AeroPKG Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #12141c; color: #fff; margin: 0; padding: 20px; }
    h1 { color: #0096ff; }
    .card { background: #1c202c; border-radius: 8px; padding: 15px; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
    button { background: #0096ff; color: #fff; border: none; border-radius: 6px; padding: 8px 16px; cursor: pointer; font-weight: bold; }
    button:hover { background: #00b4ff; }
    input[type=text] { background: #2d374e; border: 1px solid #4a5568; color: #fff; padding: 8px; border-radius: 6px; width: 300px; }
  </style>
</head>
<body>
  <h1>AeroPKG - PS4 Remote Control</h1>
  <p>Manage installed games, upload custom icons, and send Remote Packages directly to your PS4.</p>
  <div class="card">
    <div>
      <h3>Remote Package Sender</h3>
      <input type="text" id="pkgUrl" placeholder="http://192.168.1.x/game.pkg">
      <button onclick="sendPkg()">Push to PS4</button>
    </div>
  </div>
  <div class="card">
    <div>
      <h3>Soft-Restart ShellCore</h3>
      <p>Instantly reload XMB icons, custom titles, and order without rebooting.</p>
    </div>
    <button onclick="restartShell()">Restart Shell</button>
  </div>
  <script>
    function sendPkg() {
      const url = document.getElementById('pkgUrl').value;
      fetch('/api/install', { method: 'POST', body: JSON.stringify({ type: 'direct', packages: [url] }) })
        .then(() => alert('Package queued successfully on PS4!'));
    }
    function restartShell() {
      fetch('/api/restart_shell', { method: 'POST' }).then(() => alert('Restart signal sent to PS4!'));
    }
  </script>
</body>
</html>)";
            sendHttpResponse(sock, "text/html", embeddedHtml);
        }
        return;
    }

    sendHttpResponse(sock, "text/plain", "Not Found", 404);
}

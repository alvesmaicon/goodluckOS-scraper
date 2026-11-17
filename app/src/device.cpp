#include "device.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <arpa/inet.h>
#include <glob.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const char* kUsbMode = "/usr/local/bin/usb-mode.sh";

static std::string readWord(const std::string& path) {
    std::ifstream in(path);
    std::string word;
    in >> word;
    return word;
}

NetStatus NetStatus::read() {
    NetStatus s;
    s.usbNetwork = readWord("/etc/player-flags/usb-mode") == "network";
    s.canSwitch = access(kUsbMode, X_OK) == 0;

    // /proc/net/route: Iface Destination Gateway ..., the default route's destination is 00000000
    std::ifstream route("/proc/net/route");
    for (std::string line; std::getline(route, line);) {
        std::istringstream fields(line);
        std::string iface, dest;
        fields >> iface >> dest;
        if (dest == "00000000") {
            s.iface = iface;
            break;
        }
    }
    if (s.iface.empty()) return s;

    struct ifaddrs* list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (struct ifaddrs* a = list; a; a = a->ifa_next) {
            if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || s.iface != a->ifa_name) continue;
            char text[INET_ADDRSTRLEN];
            const struct sockaddr_in* in = (const struct sockaddr_in*)a->ifa_addr;
            if (inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text))) s.address = text;
        }
        freeifaddrs(list);
    }
    return s;
}

void switchUsbToNetwork() {
    const pid_t child = fork();
    if (child == 0) {
        if (fork() == 0) {
            setsid();
            execlp("doas", "doas", "-n", kUsbMode, "network", (char*)nullptr);
            _exit(127);
        }
        _exit(0);
    }
    if (child > 0) waitpid(child, nullptr, 0);
}

namespace screen {

static std::string node;    // the backlight's brightness file
static std::string saved;   // its level before off()

static const std::string& backlight() {
    if (node.empty()) {
        glob_t g;
        if (glob("/sys/class/backlight/*/brightness", 0, nullptr, &g) == 0 && g.gl_pathc) node = g.gl_pathv[0];
        globfree(&g);
    }
    return node;
}

bool off() {
    if (backlight().empty()) return false;
    const std::string level = readWord(node);
    std::ofstream out(node);
    if (!(out << "0" << std::flush)) return false;
    saved = level.empty() || level == "0" ? "5" : level;
    return true;
}

void on() {
    if (saved.empty()) return;
    std::ofstream(node) << saved;
    saved.clear();
}

bool isOff() { return !saved.empty(); }

}  // namespace screen

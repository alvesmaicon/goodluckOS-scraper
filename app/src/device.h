#pragma once
// What the scraper needs from the console: the internet, the USB cable's mode and the backlight.
#include <string>

struct NetStatus {
    std::string iface, address;     // the default route's interface and its IPv4 address
    bool usbNetwork = false;        // the USB cable is a network adapter (usb-mode.sh network)
    bool canSwitch = false;         // usb-mode.sh is there to switch it

    bool online() const { return !iface.empty(); }
    static NetStatus read();
};

void switchUsbToNetwork();          // doas usb-mode.sh network, in the background: the PC sees a replug

// The backlight off while the app keeps running: POWER would freeze the app, and the scraper with it
namespace screen {
bool off();                         // false when the backlight can't be written
void on();
bool isOff();
}

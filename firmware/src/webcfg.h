#pragma once
#include <Arduino.h>

// The configuration web server, in two modes and one implementation.
//
//   begin()   - on the network: http://<ip>/ and http://tigerspool.local/
//   beginAP() - access point + captive portal, entered automatically when
//               there is no usable network. AP "TigerSpool-Setup" at
//               http://192.168.4.1/, with a scan of nearby networks.
//
// The portal is a STATE of this firmware, never a separate binary to flash. A
// product whose recovery path is "flash a different firmware" has already
// failed the person holding it.
//
// It also serves the screen capture used to check the UI remotely:
//   /screen.bmp             the panel contents, right now
//   /screen.bmp?preview=... one of the setup screens, without changing state
//   /screen                 a page that refreshes the capture
namespace webcfg {
    void begin();              // station mode, on the local network
    void beginAP();            // access point + captive portal
    void loop();               // call from the main loop
    bool apActive();
    String url();              // the address to show the user
    const char* apName();
    int  apClients();          // devices currently joined to the setup AP
}

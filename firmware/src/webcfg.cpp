#include "webcfg.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "ui/lvgl_port.h"
#include "ui/screen_setup.h"
#include "net/portal_page.h"
#include "version.h"
#include <ArduinoJson.h>
#include "printer.h"
#include "tigertag_cloud.h"
#include "i18n.h"

// Le canvas offscreen vit dans main.cpp ; /screen.bmp le serialise tel quel.
// Declares au scope GLOBAL : dans le namespace anonyme ci-dessous ils
// designeraient d'autres symboles et l'edition de liens echouerait.
extern LGFX_Sprite canvas;
extern bool        canvasReady;

namespace {
    // ---- traducoes da pagina web (UTF-8, ordem: PT, EN, ES, FR) ----
    enum Wid {
        W_CFGMODE, W_RESCAN, W_STAR_PW, W_NETS_FOUND, W_PICK,
        W_NET, W_PASS, W_KEEP_EMPTY, W_UNCHANGED, W_LANG, W_PRINTERS,
        W_LAN_HINT, W_PRINTER, W_TYPE, W_NAME, W_SERIAL, W_CHECKCODE,
        W_SAVE_RESTART, W_TT_ACCOUNT, W_CONNECTED, W_SYNC_NOW, W_TT_FORGET,
        W_TT_HINT, W_TT_LOGIN, W_RETRY_NET, W_WIPE, W_NONE,
        W_SAVED, W_RESTART_JOIN, W_WIPED, W_RESTARTING, W_RETRY_SAVED,
        W_LOGIN_FAIL, W_ACCT_LINKED, W_SYNCED, W_FAILED, W_ACCT_OFF,
        W_RESTART_SUFFIX,
        W_GOOGLE, W_PAIR_OPEN, W_PAIR_CODE, W_PAIR_WAIT, W_PAIR_DENIED, W_PAIR_EXPIRED,
        W_N
    };
    const char* const WT[W_N][4] = {
        /* W_CFGMODE      */ { "Modo de configuracao - escolhe a rede Wi-Fi e grava.",
                              "Setup mode - pick the Wi-Fi network and save.",
                              "Modo de configuracion - elige la red Wi-Fi y guarda.",
                              "Mode configuration - choisis le reseau Wi-Fi et enregistre." },
        /* W_RESCAN       */ { "procurar de novo", "scan again", "buscar de nuevo", "rechercher a nouveau" },
        /* W_STAR_PW      */ { ". * = com password.", ". * = password-protected.",
                              ". * = con contrasena.", ". * = protege par mot de passe." },
        /* W_NETS_FOUND   */ { "Redes encontradas", "Networks found", "Redes encontradas", "Reseaux trouves" },
        /* W_PICK         */ { "-- escolhe --", "-- pick one --", "-- elige --", "-- choisir --" },
        /* W_NET          */ { "Rede", "Network", "Red", "Reseau" },
        /* W_PASS         */ { "Password", "Password", "Contrasena", "Mot de passe" },
        /* W_KEEP_EMPTY   */ { " (vazio p/ manter)", " (blank to keep)", " (vacio para mantener)", " (vide pour garder)" },
        /* W_UNCHANGED    */ { "(sem alteracao)", "(unchanged)", "(sin cambios)", "(inchange)" },
        /* W_LANG         */ { "Idioma", "Language", "Idioma", "Langue" },
        /* W_PRINTERS     */ { "Impressoras", "Printers", "Impresoras", "Imprimantes" },
        /* W_LAN_HINT     */ { "Ativa o Modo LAN em cada uma. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code do ecra da impressora.",
                              "Enable LAN mode on each. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code from the printer screen.",
                              "Activa el Modo LAN en cada una. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serial + code de la pantalla de la impresora.",
                              "Active le mode LAN sur chacune. K2: WebSocket :9999. FlashForge C5: HTTP :8898. Bambu: MQTT :8883. Serie + code depuis l ecran de l imprimante." },
        /* W_PRINTER      */ { "Impressora", "Printer", "Impresora", "Imprimante" },
        /* W_TYPE         */ { "Tipo", "Type", "Tipo", "Type" },
        /* W_NAME         */ { "Nome", "Name", "Nombre", "Nom" },
        /* W_SERIAL       */ { "Serial (FF / Bambu)", "Serial (FF / Bambu)", "Numero de serie (FF / Bambu)", "Numero de serie (FF / Bambu)" },
        /* W_CHECKCODE    */ { "Check / Access code (FF / Bambu)", "Check / Access code (FF / Bambu)",
                              "Check / Access code (FF / Bambu)", "Check / Access code (FF / Bambu)" },
        /* W_SAVE_RESTART */ { "Guardar e reiniciar", "Save and restart", "Guardar y reiniciar", "Enregistrer et redemarrer" },
        /* W_TT_ACCOUNT   */ { "Conta TigerTag", "TigerTag account", "Cuenta TigerTag", "Compte TigerTag" },
        /* W_CONNECTED    */ { "Ligado: ", "Connected: ", "Conectado: ", "Connecte : " },
        /* W_SYNC_NOW     */ { "Sincronizar maquinas agora", "Sync machines now", "Sincronizar maquinas ahora", "Synchroniser les machines" },
        /* W_TT_FORGET    */ { "Desligar a conta TigerTag", "Disconnect the TigerTag account",
                              "Desconectar la cuenta TigerTag", "Deconnecter le compte TigerTag" },
        /* W_TT_HINT      */ { "Importa as impressoras registadas na tua conta (Firebase). O login e so email/password.",
                              "Imports the printers registered in your account (Firebase). Login is just email/password.",
                              "Importa las impresoras registradas en tu cuenta (Firebase). El acceso es solo email/password.",
                              "Importe les imprimantes enregistrees dans ton compte (Firebase). La connexion est juste email/mot de passe." },
        /* W_TT_LOGIN     */ { "Ligar e importar", "Connect and import", "Conectar e importar", "Connecter et importer" },
        /* W_RETRY_NET    */ { "Tentar rede atual de novo", "Retry current network", "Reintentar la red actual", "Reessayer le reseau actuel" },
        /* W_WIPE         */ { "Apagar tudo", "Wipe everything", "Borrar todo", "Tout effacer" },
        /* W_NONE         */ { "Nenhuma", "None", "Ninguna", "Aucune" },
        /* W_SAVED        */ { "Guardado.", "Saved.", "Guardado.", "Enregistre." },
        /* W_RESTART_JOIN */ { "A reiniciar e a ligar a rede...", "Restarting and joining the network...",
                              "Reiniciando y conectando a la red...", "Redemarrage et connexion au reseau..." },
        /* W_WIPED        */ { "Apagado.", "Wiped.", "Borrado.", "Efface." },
        /* W_RESTARTING   */ { "A reiniciar...", "Restarting...", "Reiniciando...", "Redemarrage..." },
        /* W_RETRY_SAVED  */ { "Nova tentativa na rede guardada.", "Retrying the saved network.",
                              "Reintentando la red guardada.", "Nouvel essai sur le reseau enregistre." },
        /* W_LOGIN_FAIL   */ { "Login falhou", "Login failed", "Fallo de acceso", "Echec de connexion" },
        /* W_ACCT_LINKED  */ { "Conta ligada", "Account linked", "Cuenta conectada", "Compte lie" },
        /* W_SYNCED       */ { "Sincronizado", "Synced", "Sincronizado", "Synchronise" },
        /* W_FAILED       */ { "Falhou", "Failed", "Fallo", "Echoue" },
        /* W_ACCT_OFF     */ { "Conta desligada", "Account disconnected", "Cuenta desconectada", "Compte deconnecte" },
        /* W_RESTART_SUFFIX*/{ " - a reiniciar...", " - restarting...", " - reiniciando...", " - redemarrage..." },
        /* W_GOOGLE       */ { "Entrar com Google", "Sign in with Google",
                              "Iniciar sesion con Google", "Se connecter avec Google" },
        /* W_PAIR_OPEN    */ { "Abre este link (telemovel ou PC) e aprova:",
                              "Open this link (phone or PC) and approve:",
                              "Abre este enlace (movil o PC) y aprueba:",
                              "Ouvre ce lien (telephone ou PC) et approuve :" },
        /* W_PAIR_CODE    */ { "codigo", "code", "codigo", "code" },
        /* W_PAIR_WAIT    */ { "A aguardar aprovacao no telemovel...",
                              "Waiting for approval on your phone...",
                              "Esperando aprobacion en el movil...",
                              "En attente d approbation sur le telephone..." },
        /* W_PAIR_DENIED  */ { "Pedido recusado", "Request denied", "Solicitud rechazada", "Demande refusee" },
        /* W_PAIR_EXPIRED */ { "Codigo expirado - tenta de novo", "Code expired - try again",
                              "Codigo expirado - intenta de nuevo", "Code expire - reessaie" },
    };
    // The web form still carries its own four-column table (PT, EN, ES, FR),
    // inherited from the prototype. The device now has eight languages, so an
    // unmapped index would read past the end of every row.
    //
    // This maps what it can and falls back to English. The real fix is phase 7:
    // serve a static page from LittleFS with proper locale files, the way
    // TigerScale does - see docs/MIGRATION.md.
    const char* wl(Wid id) {
        int col;
        switch (i18n::current()) {
            case LANG_PT:
            case LANG_PT_PT: col = 0; break;
            case LANG_ES:    col = 2; break;
            case LANG_FR:    col = 3; break;
            default:         col = 1; break;   // English
        }
        return WT[id][col];
    }
    void reply(const String& title, const String& msg);   // fwd

    WebServer   server(80);
    DNSServer   dns;
    Preferences p;
    uint32_t    restartAt = 0;
    uint32_t    apTeardownAt = 0;
    bool        apMode    = false;
    String      apScan;                       // <option>s das redes encontradas
    // ------------------------------------------------------------------
    //  Names carry the last four hex digits of the station MAC.
    //
    //  A bare "tigerspool.local" works until there are two of them: mDNS
    //  refuses a duplicate, so the second device silently never claims the
    //  name and becomes unreachable by name.
    //
    //  The setup access point has the same problem and it is worse there.
    //  Two devices in setup mode both broadcasting "TigerSpool-Setup" means
    //  the phone joins one of them at random, and the user configures the
    //  wrong box without ever knowing.
    //
    //  The suffix costs nothing: the QR on the screen carries the SSID, so
    //  nobody types it, and the resolved name is shown on the device and on
    //  the portal's success page for anyone who needs it later.
    //
    //  The STATION MAC, not the AP's - the two differ by one on an ESP32, and
    //  the station's is what a DHCP reservation has to be made against.
    // ------------------------------------------------------------------
    char HOSTNAME_BUF[24];
    char AP_SSID_BUF[28];
    const char* HOSTNAME = HOSTNAME_BUF;
    const char* AP_SSID  = AP_SSID_BUF;

    void buildNames() {
        if (HOSTNAME_BUF[0]) return;                 // built once
        uint8_t mac[6] = {0};
        WiFi.macAddress(mac);                        // station interface
        snprintf(HOSTNAME_BUF, sizeof(HOSTNAME_BUF), "tigerspool-%02x%02x", mac[4], mac[5]);
        snprintf(AP_SSID_BUF,  sizeof(AP_SSID_BUF),  "TigerSpool-Setup-%02X%02X", mac[4], mac[5]);
    }
    const IPAddress AP_IP(192, 168, 4, 1);
    const char* PTYPES[] = { "Nenhuma", "Creality K2", "FlashForge Creator 5 Pro",
                             "Bambu Lab (A1/A2/P1/X1)", "Snapmaker (Moonraker)" };
    const int   NPTYPES  = 5;
    // Mirrors enum Lang exactly: the form writes this index straight into NVS,
    // so a shorter list here would silently store the wrong language.
    const char* LANGS[]  = { "English", "Francais", "Deutsch", "Espanol",
                             "Italiano", "Polski", "Portugues (BR)", "Portugues (PT)" };

    String esc(const String& s) {
        String o; o.reserve(s.length() + 8);
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
            else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;"; else o += c;
        }
        return o;
    }

    // ------------------------------------------------------------------
    //  Screen capture: /screen.bmp and /screen (a page that refreshes it)
    //
    //  Tout le dessin passe par le sprite offscreen 'canvas' avant pushSprite(),
    //  donc son buffer EST le framebuffer. On le sert en BMP 24 bits : pas de
    //  compression a embarquer, et tous les navigateurs le lisent. 240*3 = 720
    //  octets par ligne, multiple de 4, donc aucun padding a gerer.
    //
    //  Le handler tourne dans la meme boucle que le dessin (WebServer::
    //  handleClient est appele depuis loop()), donc pas de course sur le buffer.
    // ------------------------------------------------------------------
    void le32(uint8_t* p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

    void handleShot() {
        if (!canvasReady) { server.send(503, "text/plain", "no canvas"); return; }

        // The shadow copy is not maintained frame by frame - that would put a
        // PSRAM write in the render path for a feature used a few times a day.
        // Ask for it, force a full repaint so every pixel passes through
        // flush_cb, and pump LVGL until the frame has been drawn.
        // Diagnostic: /screen.bmp?preview=lang|wifi|pair|pairfail renders one of
        // the setup screens for the capture and nothing else. The state machine
        // redraws on its next pass, so the device is not left showing it.
        //
        // This exists because the first-boot screens are, by definition, only
        // reachable on a device that has not been set up - which is exactly the
        // state a developer cannot get a networked screenshot out of.
        String preview = server.hasArg("preview") ? server.arg("preview") : String();
        if      (preview == "lang") screen_setup::showLanguage(true);
        else if (preview == "wifi") screen_setup::showWifi("TigerSpool-Setup", 0);
        else if (preview == "pair") screen_setup::showPairing(
                     "https://tigersystem.io/pair?c=K7QF3M2P", "K7QF-3M2P", 587);
        else if (preview == "pairfail") screen_setup::showPairFailed("Code expired");
        else if (preview == "account") screen_setup::showAccountIntro();

        lvgl_port::requestCapture(true);
        lv_obj_invalidate(lv_scr_act());
        for (uint32_t t0 = millis(); millis() - t0 < 400; ) { lv_timer_handler(); delay(5); }
        lvgl_port::requestCapture(false);

        const int W = 240, H = 320;
        const uint32_t rowBytes  = (uint32_t)W * 3;
        const uint32_t dataSize  = rowBytes * H;

        uint8_t hdr[54] = {0};
        hdr[0] = 'B'; hdr[1] = 'M';
        le32(hdr + 2,  54 + dataSize);
        le32(hdr + 10, 54);
        le32(hdr + 14, 40);
        le32(hdr + 18, (uint32_t)W);
        le32(hdr + 22, (uint32_t)H);      // positif = stocke de bas en haut
        hdr[26] = 1;
        hdr[28] = 24;
        le32(hdr + 34, dataSize);

        server.setContentLength(54 + dataSize);
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "image/bmp", "");
        server.sendContent((const char*)hdr, 54);

        static uint8_t row[720];
        for (int y = H - 1; y >= 0; y--) {          // BMP: derniere ligne en premier
            uint8_t* o = row;
            for (int x = 0; x < W; x++) {
                uint32_t c = canvas.readPixel(x, y);    // RGB565 -> RGB888
                uint8_t r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
                *o++ = b | (b >> 5);                // BMP est en BGR
                *o++ = g | (g >> 6);
                *o++ = r | (r >> 5);
            }
            server.sendContent((const char*)row, rowBytes);
        }
        server.sendContent("", 0);
    }

    void handleShotPage() {
        String p = F("<!doctype html><meta charset=utf-8><title>TigerSpool screen</title>"
          "<style>body{margin:0;background:#111;color:#888;font:13px system-ui;"
          "display:flex;flex-direction:column;align-items:center;gap:10px;padding:16px}"
          "img{width:240px;height:320px;image-rendering:pixelated;border-radius:8px;"
          "border:1px solid #333}b{color:#ddd}</style>"
          "<img id=s><div>live &middot; <b id=n>0</b> frames &middot; <span id=e></span></div>"
          "<script>let n=0;function t(){const i=document.getElementById('s');"
          "i.onload=()=>{document.getElementById('n').textContent=++n;setTimeout(t,600)};"
          "i.onerror=e=>{document.getElementById('e').textContent='erreur';setTimeout(t,2000)};"
          "i.src='/screen.bmp?'+Date.now()}t();</script>");
        server.send(200, "text/html", p);
    }

    // ------------------------------------------------------------------
    //  The setup portal: one page, and three small endpoints behind it.
    //
    //  The page is served from PROGMEM with three placeholders filled in. It
    //  opens in the language chosen on the device, so someone who picked
    //  Portugues on the screen does not meet an English page on their phone.
    // ------------------------------------------------------------------
    const char* LANG_CODES[] = { "en", "fr", "de", "es", "it", "pl", "pt", "ptpt" };

    void handlePortal() {
        buildNames();
        String page = FPSTR(PORTAL_HTML);
        page.replace("%SSID%", AP_SSID);
        page.replace("%FW%",   TIGERSPOOL_FW_VERSION);
        int l = (int)i18n::current();
        page.replace("%LANG%", LANG_CODES[(l >= 0 && l < (int)LANG_N) ? l : 0]);
        server.send(200, "text/html", page);
    }

    // Networks, strongest first and deduplicated. The signal is reported in dBm
    // and the page turns it into arcs - the mapping belongs with the drawing,
    // not here.
    void handleApiScan() {
        WiFi.mode(WIFI_AP_STA);
        WiFi.scanDelete();
        int n = WiFi.scanNetworks(false, true);
        if (n < 0) n = 0;

        int idx[32], m = n > 32 ? 32 : n;
        for (int i = 0; i < m; i++) idx[i] = i;
        for (int i = 0; i < m; i++)
            for (int j = i + 1; j < m; j++)
                if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }

        JsonDocument doc;
        JsonArray arr = doc["nets"].to<JsonArray>();
        String seen = "\n";
        for (int k = 0; k < m; k++) {
            String ssid = WiFi.SSID(idx[k]);
            if (!ssid.length() || seen.indexOf("\n" + ssid + "\n") >= 0) continue;
            seen += ssid + "\n";
            JsonObject o = arr.add<JsonObject>();
            o["s"] = ssid;
            o["r"] = WiFi.RSSI(idx[k]);
            o["k"] = WiFi.encryptionType(idx[k]) != WIFI_AUTH_OPEN;
        }
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);

        // Back to AP-only: leaving the station interface scanning makes the
        // radio hop channels and the phone falls off the setup network.
        if (apMode) WiFi.mode(WIFI_AP);
    }

    // Join, verify, and only then report - no reboot.
    //
    // The prototype saved and restarted, which drops the phone and reopens the
    // portal with no explanation when the password was wrong. Here the access
    // point stays up through the attempt, so a failure is reported while the
    // user is still looking at the field they typed it into.
    //
    // Honest caveat: an ESP32 shares one radio between AP and station, and the
    // access point follows the station's channel. If the home network is on a
    // different channel the phone can drop mid-attempt and never see this
    // response. That is why the device's own screen shows the same result - the
    // page is the nice path, the screen is the one that cannot fail.
    void handleApiJoin() {
        JsonDocument in;
        if (deserializeJson(in, server.arg("plain"))) {
            server.send(400, "application/json", "{\"ok\":false}");
            return;
        }
        String ssid = in["ssid"] | "";
        String pass = in["pass"] | "";
        if (ssid.isEmpty()) { server.send(400, "application/json", "{\"ok\":false}"); return; }

        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 18000) delay(120);

        JsonDocument out;
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect(true);
            if (apMode) WiFi.mode(WIFI_AP);
            out["ok"] = false;
            String j; serializeJson(out, j);
            server.send(200, "application/json", j);
            Serial.printf("[webcfg] join '%s' failed\n", ssid.c_str());
            return;
        }

        Preferences w;
        w.begin("tigerspool", false);
        w.putString("ssid", ssid);
        w.putString("pass", pass);
        w.end();

        out["ok"]   = true;
        out["host"] = String(HOSTNAME) + ".local";
        out["ip"]   = WiFi.localIP().toString();
        // The STATION MAC. It differs from the access point's by one, and a DHCP
        // reservation made against the wrong one silently never fires.
        out["mac"]  = WiFi.macAddress();
        String j; serializeJson(out, j);
        server.send(200, "application/json", j);

        Serial.printf("[webcfg] joined '%s' as %s (%s)\n",
                      ssid.c_str(), out["ip"].as<String>().c_str(), out["mac"].as<String>().c_str());

        // Give the phone a few seconds to render the result before the access
        // point disappears from under it.
        apTeardownAt = millis() + 6000;
    }

    void handleApiLang() {
        String l = server.hasArg("l") ? server.arg("l") : String();
        for (int i = 0; i < (int)LANG_N; i++)
            if (l == LANG_CODES[i]) { i18n::set((Lang)i); break; }
        server.send(200, "application/json", "{\"ok\":true}");
    }

    void doScan() {
        // Scanning needs the station interface, so go AP+STA for it and back to
        // AP-only afterwards: leaving STA active makes the radio hop channels and
        // the phone drops off the setup network mid-form.
        WiFi.mode(WIFI_AP_STA);
        WiFi.scanDelete();
        int n = WiFi.scanNetworks(false, true);
        Serial.printf("[webcfg] scan: %d redes\n", n);
        int idx[24], m = (n > 24) ? 24 : (n < 0 ? 0 : n);
        for (int i = 0; i < m; i++) idx[i] = i;
        for (int i = 0; i < m; i++)
            for (int j = i + 1; j < m; j++)
                if (WiFi.RSSI(idx[j]) > WiFi.RSSI(idx[i])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        String o, seen = "\n";
        for (int k = 0; k < m; k++) {
            String raw = WiFi.SSID(idx[k]);
            if (!raw.length() || seen.indexOf("\n" + raw + "\n") >= 0) continue;
            seen += raw + "\n";
            bool lock = (WiFi.encryptionType(idx[k]) != WIFI_AUTH_OPEN);
            o += "<option value=\"" + esc(raw) + "\">" + esc(raw) + "  " + WiFi.RSSI(idx[k]) + "dBm" + (lock ? " *" : "") + "</option>";
        }
        WiFi.scanDelete();
        apScan = o;
        if (apMode) { WiFi.mode(WIFI_AP); delay(50); }   // AP-only = estavel
    }

    struct Row { int type; String name, host, sn, cc; };
    void load(String& ssid, String& pass, int& lang, Row r[MAX_PRINTERS]) {
        p.begin("tigerspool", true);
        ssid = p.getString("ssid", "");
        pass = p.getString("pass", "");
        lang = p.getInt("lang", -1);
        for (int i = 0; i < MAX_PRINTERS; i++) {
            char k[6];
            snprintf(k, sizeof(k), "p%dt", i); r[i].type = p.getInt(k, 0);
            snprintf(k, sizeof(k), "p%dn", i); r[i].name = p.getString(k, "");
            snprintf(k, sizeof(k), "p%dh", i); r[i].host = p.getString(k, "");
            snprintf(k, sizeof(k), "p%ds", i); r[i].sn   = p.getString(k, "");
            snprintf(k, sizeof(k), "p%dc", i); r[i].cc   = p.getString(k, "");
        }
        p.end();
    }

    String page() {
        String ssid, pass; int lang; Row r[MAX_PRINTERS];
        load(ssid, pass, lang, r);

        String h; h.reserve(5200);
        h += F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
               "<title>TigerTag Bridge</title><style>"
               "body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:20px}"
               ".card{max-width:460px;margin:auto;background:#1c1c1c;border:1px solid #333;border-radius:12px;padding:22px}"
               "h1{font-size:1.15rem;margin:.1rem 0 .3rem}h2{font-size:.95rem;color:#bbb;margin:1.5rem 0 .3rem;border-top:1px solid #333;padding-top:1rem}"
               "label{display:block;margin:.7rem 0 .2rem;font-size:.85rem;color:#bbb}"
               "input,select{width:100%;box-sizing:border-box;padding:9px;border-radius:8px;border:1px solid #444;background:#111;color:#eee;font-size:.95rem}"
               "button{margin-top:1.4rem;width:100%;padding:12px;border:0;border-radius:8px;background:#2d7;color:#012;font-weight:700;font-size:1rem}"
               "a{color:#2d7}.hint{color:#888;font-size:.78rem;margin-top:.25rem}"
               "fieldset{border:1px solid #333;border-radius:8px;margin:.6rem 0;padding:.4rem .8rem .8rem}"
               "legend{color:#999;font-size:.8rem;padding:0 .4rem}"
               "</style></head><body><div class=card>"
               "<h1>TigerTag Bridge</h1>");
        if (apMode) {
            h += F("<div class=hint>"); h += wl(W_CFGMODE); h += F("</div>");
        }
        else {
            h += F("<div class=hint>IP "); h += WiFi.localIP().toString();
            h += F(" &middot; <a href=\"http://tigerspool.local/\">tigerspool.local</a></div>");
        }
        h += F("<form method=POST action=/save>"
               "<h2>Wi-Fi</h2>");
        if (apMode) {
            h += F("<label>"); h += wl(W_NETS_FOUND);
            h += F("</label><select id=sel onchange=\"document.getElementById('ssid').value=this.value\">"
                   "<option value=\"\">"); h += wl(W_PICK); h += F("</option>");
            h += apScan;
            h += F("</select><div class=hint><a href=\"/?rescan=1\">"); h += wl(W_RESCAN);
            h += F("</a>"); h += wl(W_STAR_PW); h += F("</div>");
        }
        h += F("<label>"); h += wl(W_NET); h += F("</label><input id=ssid name=ssid value=\"");
        h += esc(ssid);
        h += F("\"><label>"); h += wl(W_PASS);
        if (!apMode) h += wl(W_KEEP_EMPTY);
        h += F("</label><input name=pass type=password autocomplete=off");
        if (!apMode) { h += F(" placeholder=\""); h += wl(W_UNCHANGED); h += F("\""); }
        h += F("><h2>"); h += wl(W_LANG); h += F("</h2><select name=lang>");
        for (int i = 0; i < (int)LANG_N; i++) { h += "<option value=\""; h += i; h += "\""; if (lang == i) h += " selected"; h += ">"; h += LANGS[i]; h += "</option>"; }
        h += F("</select><h2>"); h += wl(W_PRINTERS);
        h += F("</h2><div class=hint>"); h += wl(W_LAN_HINT); h += F("</div>");
        for (int i = 0; i < MAX_PRINTERS; i++) {
            h += "<fieldset><legend>"; h += wl(W_PRINTER); h += ' '; h += (i + 1); h += "</legend>";
            h += "<label>"; h += wl(W_TYPE); h += "</label><select name=p"; h += i; h += "t>";
            for (int t = 0; t < NPTYPES; t++) { h += "<option value=\""; h += t; h += "\""; if (r[i].type == t) h += " selected"; h += ">"; h += (t == 0 ? wl(W_NONE) : PTYPES[t]); h += "</option>"; }
            h += "</select>";
            h += "<label>"; h += wl(W_NAME); h += "</label><input name=p"; h += i; h += "n value=\""; h += esc(r[i].name); h += "\">";
            h += "<label>IP</label><input name=p"; h += i; h += "h inputmode=decimal placeholder=\"192.168.1.50\" value=\""; h += esc(r[i].host); h += "\">";
            h += "<label>"; h += wl(W_SERIAL); h += "</label><input name=p"; h += i; h += "s value=\""; h += esc(r[i].sn); h += "\">";
            h += "<label>"; h += wl(W_CHECKCODE); h += "</label><input name=p"; h += i; h += "c value=\""; h += esc(r[i].cc); h += "\">";
            h += "</fieldset>";
        }
        h += F("<button type=submit>"); h += wl(W_SAVE_RESTART); h += F("</button></form>");

        // ---- Conta TigerTag (importa as maquinas da conta) ----
        h += F("<h2>"); h += wl(W_TT_ACCOUNT); h += F("</h2>");
        if (ttcloud::haveSession()) {
            h += F("<div class=hint>"); h += wl(W_CONNECTED); h += esc(ttcloud::email());
            h += F("<br>"); h += esc(ttcloud::lastResult());
            h += F("</div><form method=POST action=/tt-sync style=margin-top:.6rem>"
                   "<button type=submit>"); h += wl(W_SYNC_NOW); h += F("</button></form>"
                   "<p class=hint><a href=/tt-forget>"); h += wl(W_TT_FORGET); h += F("</a></p>");
        } else {
            h += F("<div class=hint>"); h += wl(W_TT_HINT); h += F("</div>"
                   "<form method=POST action=/tt-login>"
                   "<label>Email</label><input name=ttmail type=email autocomplete=off>"
                   "<label>"); h += wl(W_PASS); h += F("</label><input name=ttpass type=password autocomplete=off>"
                   "<button type=submit>"); h += wl(W_TT_LOGIN); h += F("</button></form>"
                   "<form method=POST action=/tt-gstart style=margin-top:.4rem>"
                   "<button type=submit style=background:#4285f4;color:#fff>");
            h += wl(W_GOOGLE); h += F("</button></form>");
        }

        h += F("<p class=hint><a href=/retry>"); h += wl(W_RETRY_NET);
        h += F("</a> &middot; <a href=/reset>"); h += wl(W_WIPE); h += F("</a></p>"
               "</div></body></html>");
        return h;
    }

    void handleRoot() {
        if (apMode && server.hasArg("rescan")) doScan();
        server.send(200, "text/html", page());
    }

    void handleSave() {
        p.begin("tigerspool", false);
        if (server.hasArg("ssid")) { String s = server.arg("ssid"); s.trim(); p.putString("ssid", s); }
        if (server.hasArg("pass") && (apMode || server.arg("pass").length())) p.putString("pass", server.arg("pass"));
        if (server.hasArg("lang")) p.putInt("lang", server.arg("lang").toInt());
        for (int i = 0; i < MAX_PRINTERS; i++) {
            char a[4], k[6];
            snprintf(a, sizeof(a), "p%dt", i); snprintf(k, sizeof(k), "p%dt", i); p.putInt(k, server.arg(a).toInt());
            snprintf(a, sizeof(a), "p%dn", i); snprintf(k, sizeof(k), "p%dn", i); { String v = server.arg(a); v.trim();
                if (v.isEmpty() && server.arg(String("p") + i + "t").toInt() != 0) v = PTYPES[server.arg(String("p") + i + "t").toInt()];
                p.putString(k, v); }
            snprintf(a, sizeof(a), "p%dh", i); snprintf(k, sizeof(k), "p%dh", i); { String v = server.arg(a); v.trim(); p.putString(k, v); }
            snprintf(a, sizeof(a), "p%ds", i); snprintf(k, sizeof(k), "p%ds", i); { String v = server.arg(a); v.trim(); p.putString(k, v); }
            snprintf(a, sizeof(a), "p%dc", i); snprintf(k, sizeof(k), "p%dc", i); { String v = server.arg(a); v.trim(); p.putString(k, v); }
        }
        p.remove("k2ip");
        p.end();
        reply(wl(W_SAVED), wl(W_RESTART_JOIN));
        restartAt = millis() + 1400;
    }

    // Factory reset: the device must come back exactly as it left the flasher.
    //
    // Clearing only "tigerspool" is not that. The account session lives in its
    // own namespace, and the prototype's namespaces are still on the chip - the
    // migration in main.cpp would read them on the next boot and put the Wi-Fi
    // credentials and the printers straight back. A reset that undoes itself is
    // worse than no reset, because the user believes the device was wiped.
    void handleReset() {
        const char* namespaces[] = { "tigerspool", "tsaccount", "k2cfg", "ttcfg" };
        for (const char* ns : namespaces) {
            Preferences w;
            if (w.begin(ns, false)) { w.clear(); w.end(); }
        }
        Serial.println("[config] factory reset - all namespaces cleared");
        reply(wl(W_WIPED), wl(W_RESTARTING));
        restartAt = millis() + 1200;
    }

    void handleRetry() {
        reply(wl(W_RESTARTING), wl(W_RETRY_SAVED));
        restartAt = millis() + 1000;
    }

    void reply(const String& title, const String& msg) {
        String h = F("<!doctype html><meta charset=utf-8><meta http-equiv=refresh content=\"5;url=/\">"
                     "<body style='font-family:system-ui;background:#111;color:#eee;padding:24px'><h2>");
        h += esc(title); h += F("</h2><p>"); h += esc(msg); h += F("</p></body>");
        server.send(200, "text/html", h);
    }

    void handleTtLogin() {
        String mail = server.arg("ttmail"); mail.trim();
        String pass = server.arg("ttpass");
        String err;
        if (!ttcloud::signIn(mail, pass, err)) { reply(wl(W_LOGIN_FAIL), err); return; }
        String s; ttcloud::syncNow(s);
        reply(wl(W_ACCT_LINKED), s + wl(W_RESTART_SUFFIX));
        restartAt = millis() + 1600;
    }
    void handleTtSync() {
        String s;
        bool ok = ttcloud::syncNow(s);
        reply(ok ? wl(W_SYNCED) : wl(W_FAILED), s + wl(W_RESTART_SUFFIX));
        restartAt = millis() + 1600;
    }
    void handleTtForget() {
        ttcloud::forget();
        reply(wl(W_ACCT_OFF), wl(W_RESTARTING));
        restartAt = millis() + 1200;
    }

    // --- login Google (fluxo de pareamento por link) ---
    String g_pairTok, g_pairUrl, g_pairCode;
    int    g_pairIv = 5;

    // pagina de espera: mostra o link + codigo e recarrega em /tt-gpoll
    void pairWaitPage(const String& extra) {
        String h = F("<!doctype html><html><head><meta charset=utf-8>"
                     "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
                     "<meta http-equiv=refresh content=\"");
        h += g_pairIv; h += F(";url=/tt-gpoll\">"
                     "<style>body{font-family:system-ui;background:#111;color:#eee;padding:22px;text-align:center}"
                     "a{color:#4285f4;font-size:1.1rem;word-break:break-all}"
                     ".c{font-size:1.6rem;letter-spacing:3px;margin:1rem 0}</style></head><body>");
        h += F("<p>"); h += wl(W_PAIR_OPEN); h += F("</p><p><a href=\""); h += esc(g_pairUrl);
        h += F("\" target=_blank>"); h += esc(g_pairUrl); h += F("</a></p>");
        if (g_pairCode.length()) { h += F("<p class=hint>"); h += wl(W_PAIR_CODE);
            h += F("</p><p class=c>"); h += esc(g_pairCode); h += F("</p>"); }
        h += F("<p>"); h += wl(W_PAIR_WAIT); h += F("</p>");
        if (extra.length()) { h += F("<p class=hint>"); h += esc(extra); h += F("</p>"); }
        h += F("</body></html>");
        server.send(200, "text/html", h);
    }

    void handleTtGStart() {
        String err;
        if (!ttcloud::pairStart(g_pairCode, g_pairUrl, g_pairTok, g_pairIv, err)) {
            reply(wl(W_FAILED), err); return;
        }
        if (g_pairIv < 3) g_pairIv = 3;
        pairWaitPage("");
    }

    void handleTtGPoll() {
        if (g_pairTok.isEmpty()) { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); return; }
        String ct, em, err;
        int st = ttcloud::pairPoll(g_pairTok, ct, em, err);
        if (st == 1) {
            g_pairTok = "";
            if (!ttcloud::signInWithCustomToken(ct, em, err)) { reply(wl(W_LOGIN_FAIL), err); return; }
            String s; ttcloud::syncNow(s);
            reply(wl(W_ACCT_LINKED), s + wl(W_RESTART_SUFFIX));
            restartAt = millis() + 1600;
        } else if (st == 2) {
            g_pairTok = ""; reply(wl(W_PAIR_DENIED), wl(W_RESTARTING)); restartAt = millis() + 1500;
        } else if (st == 3) {
            g_pairTok = ""; reply(wl(W_PAIR_EXPIRED), wl(W_RESTARTING)); restartAt = millis() + 1500;
        } else {
            pairWaitPage(st < 0 ? err : String());   // 0 = pendente, <0 = falha transitoria
        }
    }

    void handleCaptive() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    }

    void routes(bool captive) {
        // In AP mode the root IS the setup portal. The legacy form stays on the
        // local network, where printers and the account are configured.
        server.on("/", apMode ? handlePortal : handleRoot);
        server.on("/api/scan", handleApiScan);
        server.on("/api/join", HTTP_POST, handleApiJoin);
        server.on("/api/lang", handleApiLang);
        server.on("/screen.bmp", handleShot);      // raw panel capture
        server.on("/screen", handleShotPage);      // page qui la rafraichit
        server.on("/save", HTTP_POST, handleSave);
        server.on("/reset", handleReset);
        server.on("/retry", handleRetry);
        server.on("/tt-login", HTTP_POST, handleTtLogin);
        server.on("/tt-gstart", HTTP_POST, handleTtGStart);
        server.on("/tt-gpoll", handleTtGPoll);
        server.on("/tt-sync", HTTP_POST, handleTtSync);
        server.on("/tt-forget", handleTtForget);
        if (captive) {
            server.on("/generate_204", handleCaptive);
            server.on("/gen_204", handleCaptive);
            server.on("/ncsi.txt", handleCaptive);
            server.on("/connecttest.txt", handleCaptive);
            server.on("/hotspot-detect.html", handleCaptive);
            server.on("/canonical.html", handleCaptive);
            server.onNotFound(handleCaptive);
        } else {
            server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
        }
    }
}

void webcfg::begin() {
    apMode = false;
    buildNames();
    if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    routes(false);
    server.begin();
    Serial.printf("[webcfg] http://%s  http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
}

void webcfg::beginAP() {
    apMode = true;

    // Stop the station interface retrying an association: it makes the radio
    // hop channels, and the access point appears to drop every few seconds.
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);          // para + apaga credenciais da STA
    delay(100);

    WiFi.mode(WIFI_AP_STA);               // AP+STA only for the initial scan
    WiFi.setSleep(false);                 // AP sem modem-sleep = ligacoes estaveis
    buildNames();
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, nullptr, 1, 0, 4);   // canal 1 fixo, max 4 clientes
    delay(300);

    doScan();                            // scans, then returns to plain AP

    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", AP_IP);
    routes(true);
    server.begin();
    Serial.printf("[webcfg] AP '%s' (canal 1)  http://192.168.4.1/\n", AP_SSID);
}

void webcfg::loop() {
    if (apMode) dns.processNextRequest();
    server.handleClient();
    if (restartAt && millis() >= restartAt) { delay(50); ESP.restart(); }

    // The access point comes down only after the phone has had time to see the
    // result. Nothing reboots: the device is already on the network.
    if (apTeardownAt && millis() >= apTeardownAt) {
        apTeardownAt = 0;
        apMode = false;
        dns.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        Serial.println("[webcfg] setup access point down, station up");
    }
}

bool webcfg::apActive()   { return apMode; }
const char* webcfg::apName() { buildNames(); return AP_SSID; }
int webcfg::apClients()    { return apMode ? WiFi.softAPgetStationNum() : 0; }
String webcfg::url() { buildNames(); return apMode ? String("http://192.168.4.1")
                                                : String("http://") + HOSTNAME + ".local"; }

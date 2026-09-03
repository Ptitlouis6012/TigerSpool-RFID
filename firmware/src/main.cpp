// ============================================================================
//  cfs_ui  -  multi-impressora: escolher impressora -> grelha de slots ->
//             ler TigerTag -> enviar material+cor
//
//  Estados: LANG (1a vez) -> WIFI -> PRINTER -> GRID -> SCAN -> REVIEW -> RESULT
//  Config em NVS "tigerspool": ssid/pass/lang + p0..p3 (tipo/nome/host/sn/cc) + selectedPrinter.
//  Impressoras adicionadas/editadas no tools/wifi_portal.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LovyanGFX.hpp>
#include "LGFX_ESP32_S3_Touch_LCD_2.h"
#include "config.h"
#include "i18n.h"
#include "reader.h"
#include "printer.h"
#include "backend_creality.h"
#include "backend_ff.h"
#include "backend_bambu.h"
#include "backend_snapmaker.h"
#include "webcfg.h"
#include "tigertag_cloud.h"
#include "ui/lvgl_port.h"
#include "ui/screen_home.h"

// ---- cores -------------------------------------------------------------
#define C_BG    0x0000
#define C_HDR   0x2104
#define C_TXT   0xFFFF
#define C_DIM   0x8410
#define C_SEL   0xFFE0
#define C_OKG   0x2606
#define C_ERR   0xF800
#define C_BTNR  0x9000
#define C_BTNG  0x0480
#define C_BTNB  0x1A4B

LGFX        lcd;
LGFX_Sprite canvas(&lcd);
bool        canvasReady = false;

Preferences nvs;
String  wifiSsid, wifiPass;
PrinterCfg printers[MAX_PRINTERS];
int     selectedPrinter = 0;

PrinterBackend* backend = nullptr;
CrealityBackend            crealityBackend;
FlashForgeC5Backend  flashForgeBackend;
BambuBackend         bambuBackend;
SnapmakerBackend     snapmakerBackend;
bool webStarted = false;

enum State { ST_LANG, ST_WIFI, ST_AP, ST_PRINTER, ST_GRID, ST_SCAN, ST_REVIEW, ST_RESULT };
State   state = ST_LANG;
bool    nfcReady = false;
uint32_t nfcLastTry = 0;
int     selSlot = -1;
int     gridPage = 0;                 // paginacao da grelha (backends com muitos slots)
static const int GRID_PP = 6;         // slots por pagina (2 col x 3 lin)
TagInfo tag;
bool    sendOk = false;
String  resultMsg;
uint32_t stateSince = 0;

static int printerCount() {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (printers[i].type != PT_NONE) n++;
    return n;
}
static const char* typeTag(PrinterType t) {
    return t == PT_CREALITY ? "K2" : t == PT_FF_C5 ? "FF C5" : t == PT_BAMBU ? "Bambu"
         : t == PT_SNAPMAKER ? "Snap" : "--";
}

// ---- sondagem "esta online?" (TCP connect a porta de controlo) ----------
// K2 = ws :9999 | FlashForge C5 = HTTP :8898 | Bambu = MQTT :8883
// Guarda o instante da ultima resposta OK; uma sondagem falhada NAO esconde
// logo a impressora (a ligacao TCP falha por congestao/ARP frio a toda a hora).
static uint32_t pLastSeen[MAX_PRINTERS] = { 0 };
static uint32_t pProbeAt = 0;
static int      pProbeIdx = 0;
static const uint32_t ONLINE_TTL_MS = 25000;   // "online" ate 25 s sem resposta

static uint16_t ctrlPort(PrinterType t) {
    switch (t) {
        case PT_FF_C5: return 8898;
        case PT_BAMBU: return 8883;
        case PT_SNAPMAKER: return 7125;
        default:       return 9999;       // K2
    }
}
static bool probeOne(const PrinterCfg& p) {
    if (p.type == PT_NONE || p.host.isEmpty()) return false;
    WiFiClient c;
    bool ok = c.connect(p.host.c_str(), ctrlPort(p.type), 900);
    c.stop();
    return ok;
}
static bool isOnline(int i) {
    return printers[i].type != PT_NONE && pLastSeen[i] != 0
        && (millis() - pLastSeen[i] < ONLINE_TTL_MS);
}
static int onlineCount();                   // fwd
static void probeTick() {                   // 1 impressora por chamada, round-robin
    // se nada responde (rede errada / impressoras desligadas) abranda muito -
    // senao o log enche de "connect() ... errno 104" a cada segundo
    uint32_t gap = (onlineCount() > 0) ? 1200 : 6000;
    if (millis() - pProbeAt < gap) return;
    pProbeAt = millis();
    for (int k = 0; k < MAX_PRINTERS; k++) {
        int i = (pProbeIdx + k) % MAX_PRINTERS;
        if (printers[i].type == PT_NONE) continue;
        if (probeOne(printers[i])) pLastSeen[i] = millis() ? millis() : 1;
        pProbeIdx = (i + 1) % MAX_PRINTERS;
        return;
    }
}
static int onlineCount() {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (isOnline(i)) n++;
    return n;
}
// mostra SEMPRE as impressoras configuradas; o "online" e so um indicador
// (ponto verde / rotulo offline). A sondagem TCP nao e fiavel o suficiente
// para esconder nada - K2/FF com Modo LAN off dao RST na porta.
static bool printerVisible(int i) {
    return printers[i].type != PT_NONE;
}

// ---- descoberta de Creality na LAN (auto-corrige IPs errados) -----------
// As K2 nao anunciam mDNS; o Studio Manager varre a subrede a procura da
// porta 9999 + handshake WebSocket. Fazemos o mesmo, sem bloquear: umas
// quantas IPs por chamada. Casa a impressora pelo deviceSn; se nao houver
// serial, casa 1:1 (uma K2 offline <-> uma K2 nova encontrada).
namespace disc {
    enum { IDLE, SWEEP, RECONCILE } st = IDLE;
    uint32_t lastRun = 0;
    int      cur = 1;
    IPAddress base;
    struct Found { uint8_t oct; String sn; };
    Found  found[10];
    int    nFound = 0;

    // Handshake WS minimo + pede printerInfo; devolve o deviceSn se for uma K2.
    bool probe(IPAddress ip, String& sn) {
        WiFiClient c;
        if (!c.connect(ip, 9999, 150)) { c.stop(); return false; }
        c.print(F("GET / HTTP/1.1\r\nHost: k\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n"));
        String buf; uint32_t t0 = millis();
        while (millis() - t0 < 250 && buf.indexOf("\r\n\r\n") < 0) {
            while (c.available()) buf += (char)c.read();
            delay(5);
        }
        if (buf.indexOf(" 101 ") < 0 && buf.indexOf("101 Switching") < 0) { c.stop(); return false; }
        // frame de texto WS mascarado com o pedido
        const char* req = "{\"method\":\"get\",\"params\":{\"printerInfo\":1}}";
        uint8_t rl = strlen(req), hdr[6] = { 0x81, (uint8_t)(0x80 | rl), 0x00, 0x00, 0x00, 0x00 };
        c.write(hdr, 6);
        for (uint8_t i = 0; i < rl; i++) { uint8_t x = req[i]; c.write(&x, 1); }
        // le ~500 ms de resposta e procura deviceSn / hostname no meio das frames
        buf = ""; t0 = millis();
        while (millis() - t0 < 500 && buf.length() < 1800) {
            while (c.available()) buf += (char)c.read();
            delay(5);
        }
        c.stop();
        bool isK2 = buf.indexOf("hostname") >= 0 || buf.indexOf("deviceSn") >= 0
                 || buf.indexOf("modelVersion") >= 0;
        int a = buf.indexOf("\"deviceSn\":\"");
        if (a >= 0) { a += 12; int b = buf.indexOf('"', a); if (b > a) sn = buf.substring(a, b); }
        return isK2;
    }

    void reconcile() {
        bool used[10] = { false };
        bool changed = false;
        nvs.begin("tigerspool", false);
        // 1) casar pelo serial
        for (int i = 0; i < MAX_PRINTERS; i++) {
            if (printers[i].type != PT_CREALITY || printers[i].sn.isEmpty() || isOnline(i)) continue;
            for (int j = 0; j < nFound; j++) {
                if (used[j] || found[j].sn.isEmpty() || found[j].sn != printers[i].sn) continue;
                IPAddress ip = base; ip[3] = found[j].oct;
                String s = ip.toString();
                if (s != printers[i].host) {
                    Serial.printf("[discovery] %s: IP %s -> %s (serial)\n", printers[i].name.c_str(),
                                  printers[i].host.c_str(), s.c_str());
                    printers[i].host = s; char k[6]; snprintf(k, sizeof(k), "p%dh", i);
                    nvs.putString(k, s); changed = true;
                }
                used[j] = true; pLastSeen[i] = 0;
            }
        }
        // 2) casar 1:1 (exatamente uma K2 offline sem match <-> uma encontrada livre)
        int io = -1, jo = -1, no = 0, nj = 0;
        for (int i = 0; i < MAX_PRINTERS; i++)
            if (printers[i].type == PT_CREALITY && !isOnline(i)) { no++; io = i; }
        for (int j = 0; j < nFound; j++) if (!used[j]) { nj++; jo = j; }
        if (no == 1 && nj == 1) {
            IPAddress ip = base; ip[3] = found[jo].oct;
            String s = ip.toString();
            if (s != printers[io].host) {
                Serial.printf("[discovery] %s: IP %s -> %s (1:1)\n", printers[io].name.c_str(),
                              printers[io].host.c_str(), s.c_str());
                printers[io].host = s; char k[6]; snprintf(k, sizeof(k), "p%dh", io);
                nvs.putString(k, s); changed = true; pLastSeen[io] = 0;
            }
        }
        nvs.end();
        Serial.printf("[discovery] fim: %d K2 na LAN, %s\n", nFound, changed ? "IPs corrigidos" : "sem alteracoes");
    }

    void tick() {
        if (st == IDLE) {
            if (!WiFi.isConnected()) return;
            if (lastRun && millis() - lastRun < 180000) return;       // no max 1x / 3 min
            bool anyOff = false;
            for (int i = 0; i < MAX_PRINTERS; i++)
                if (printers[i].type == PT_CREALITY && !isOnline(i)) anyOff = true;
            if (!anyOff) return;
            base = WiFi.localIP(); nFound = 0; cur = 1; st = SWEEP;
            lastRun = millis();
            Serial.printf("[discovery] varrer %d.%d.%d.1-254 :9999...\n", base[0], base[1], base[2]);
        }
        if (st == SWEEP) {
            for (int k = 0; k < 4 && cur <= 254; k++, cur++) {
                IPAddress ip = base; ip[3] = cur;
                if (ip == WiFi.localIP()) continue;
                String sn;
                if (probe(ip, sn) && nFound < 10) {
                    found[nFound++] = { (uint8_t)cur, sn };
                    Serial.printf("[discovery] K2 @ %s sn=%s\n", ip.toString().c_str(), sn.c_str());
                }
            }
            if (cur > 254) st = RECONCILE;
        }
        if (st == RECONCILE) { reconcile(); st = IDLE; }
    }
}

// ---- touch --------------------------------------------------------------
bool     touchWas = false;
int      tapX = -1, tapY = -1;
bool     tapPending = false;
uint32_t tapCooldownUntil = 0;

static void pollTouch() {
    int32_t x, y;
    bool now = lcd.getTouch(&x, &y);
    if (now && !touchWas && millis() > tapCooldownUntil) {
        tapX = x; tapY = y; tapPending = true;
        tapCooldownUntil = millis() + 220;
    }
    touchWas = now;
}
static bool takeTap(int& x, int& y) {
    if (!tapPending) return false;
    x = tapX; y = tapY; tapPending = false; return true;
}

// ---- desenho -----------------------------------------------------------
static void textC(const String& s, int cx, int y, uint8_t size, uint16_t col) {
    canvas.setTextSize(size); canvas.setTextColor(col);
    canvas.setCursor(cx - canvas.textWidth(s) / 2, y);
    canvas.print(s);
}
struct Btn { int x, y, w, h; };
static void drawBtn(const Btn& b, const String& label, uint16_t bg) {
    canvas.fillRoundRect(b.x, b.y, b.w, b.h, 6, bg);
    textC(label, b.x + b.w / 2, b.y + b.h / 2 - 8, 2, C_TXT);
}
static bool inBtn(const Btn& b, int x, int y) {
    return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}
static Btn rowBtn(int i) { return Btn{ 20, 70 + i * 52, 200, 44 }; }
static void cellRect(int i, int& x, int& y, int& w, int& h) {
    w = 117; h = 90;
    x = (i % 2) ? 121 : 2;
    y = 26 + (i / 2) * 94;
}
// botao "voltar" no cabecalho (presente nos ecrans depois da escolha da impressora)
static bool backShown() {
    return state == ST_GRID || state == ST_SCAN || state == ST_REVIEW || state == ST_RESULT;
}
// zona de toque generosa (o chip e pequeno mas a barra toda a esquerda volta)
static bool inBack(int x, int y) { return backShown() && x < 92 && y < 30; }

static void header(const String& right) {
    canvas.fillRect(0, 0, SCR_W, 22, C_HDR);
    canvas.setTextSize(1);
    int nameX = 4;
    if (backShown()) {
        canvas.fillRoundRect(2, 1, 52, 20, 4, C_BTNG);       // chip maior
        canvas.setTextColor(C_BG);
        canvas.setCursor(8, 7);
        canvas.print(i18n::T(S_BACK));
        nameX = 62;
    }
    canvas.fillCircle(SCR_W - 8,  11, 4, (backend && backend->connected()) ? C_OKG : C_ERR);
    canvas.fillCircle(SCR_W - 20, 11, 4, nfcReady ? C_OKG : C_ERR);
    int rw = canvas.textWidth(right);
    int rightX = SCR_W - 30 - rw;
    canvas.setTextColor(C_DIM);
    canvas.setCursor(rightX, 7);
    canvas.print(right);
    // nome, truncado para nao bater no IP
    String nm = (state == ST_GRID) ? printers[selectedPrinter].name : String("CFS");
    int avail = rightX - nameX - 4;
    while (nm.length() > 1 && canvas.textWidth(nm) > avail) nm.remove(nm.length() - 1);
    canvas.setTextColor(C_TXT);
    canvas.setCursor(nameX, 7);
    canvas.print(nm);
}

static void drawLang() {
    canvas.fillScreen(C_BG);
    textC(i18n::T(S_CHOOSE_LANG), SCR_W / 2, 34, 2, C_SEL);
    for (int i = 0; i < LANG_N; i++) drawBtn(rowBtn(i), i18n::name((Lang)i), C_BTNB);
    canvas.pushSprite(0, 0);
}

// lista de impressoras: paginada (MAX_PRINTERS pode ser 8)
static const int PR_PP = 4;              // linhas por pagina
static int prPage = 0;
static Btn prRowBtn(int k) { return Btn{ 20, 52 + k * 50, 200, 44 }; }

// indices das impressoras visiveis, por ordem
static int visiblePrinters(int idx[MAX_PRINTERS]) {
    int n = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) if (printerVisible(i)) idx[n++] = i;
    return n;
}
static int prPages() {
    int idx[MAX_PRINTERS]; int n = visiblePrinters(idx);
    int p = (n + PR_PP - 1) / PR_PP;
    return p < 1 ? 1 : p;
}

static void drawPrinter() {
    canvas.fillScreen(C_BG);
    textC(i18n::T(S_PRINTER), SCR_W / 2, 30, 2, C_SEL);
    int idx[MAX_PRINTERS]; int n = visiblePrinters(idx);
    if (printerCount() == 0) {
        textC(i18n::T(S_NO_PRINTERS), SCR_W / 2, 110, 2, C_TXT);
        textC(i18n::T(ttcloud::haveSession() ? S_TT_LINKED : S_ADD_WEB), SCR_W / 2, 140, 1, C_DIM);
    } else {
        int nPages = prPages();
        if (prPage >= nPages) prPage = 0;
        int base = prPage * PR_PP;
        for (int k = 0; k < PR_PP && base + k < n; k++) {
            int i = idx[base + k];
            Btn b = prRowBtn(k);
            drawBtn(b, printers[i].name, i == selectedPrinter ? C_BTNG : C_BTNB);
            textC(typeTag(printers[i].type), b.x + b.w - 24, b.y + b.h / 2 - 4, 1, C_DIM);
            canvas.fillCircle(b.x + b.w - 10, b.y + b.h / 2, 4, isOnline(i) ? C_OKG : C_ERR);
        }
        if (nPages > 1) {
            char bt[24]; snprintf(bt, sizeof(bt), "<  %d/%d  >", prPage + 1, nPages);
            textC(bt, SCR_W / 2, 258, 1, C_SEL);
        }
    }
    if (resultMsg.length() && prPages() <= 1)
        textC(resultMsg, SCR_W / 2, SCR_H - 42, 1, C_OKG);
    if (ttcloud::haveSession())
        textC("TigerTag: " + ttcloud::email(), SCR_W / 2, SCR_H - 28, 1, C_DIM);
    // sync em curso: ponto discreto no canto, o ecra continua utilizavel
    if (ttcloud::asyncBusy())
        canvas.fillCircle(SCR_W - 8, 8, 3, ((millis() / 400) % 2) ? C_SEL : C_HDR);
    textC(String(i18n::T(S_CONFIG_WEB)) + " tigerspool.local", SCR_W / 2, SCR_H - 14, 1, C_DIM);
    canvas.pushSprite(0, 0);
}

static int gridPages() {
    int nc = backend ? backend->slotCount() : 0;
    int p = (nc + GRID_PP - 1) / GRID_PP;
    return p < 1 ? 1 : p;
}

static void drawGrid() {
    canvas.fillScreen(C_BG);
    header(WiFi.localIP().toString());
    int nc = backend ? backend->slotCount() : 0;
    int nPages = gridPages();
    if (gridPage >= nPages) gridPage = 0;
    int base = gridPage * GRID_PP;
    for (int k = 0; k < GRID_PP && base + k < nc; k++) {
        int gi = base + k;
        int x, y, w, h; cellRect(k, x, y, w, h);
        const SlotState& s = backend->slot(gi);
        int cx = x + w / 2, cy = y + 30;
        uint16_t fill = s.known ? canvas.color565(s.r, s.g, s.b) : 0x2945;
        canvas.fillCircle(cx, cy, 24, fill);
        canvas.drawCircle(cx, cy, 24, C_TXT);
        if (s.selected) canvas.fillTriangle(cx - 5, y + 2, cx + 5, y + 2, cx, y + 9, C_SEL);
        textC(backend->slotLabel(gi), cx, y + 58, 2, C_TXT);
        textC(s.known ? s.type : String("--"), cx, y + 76, 1, C_DIM);
        if (gi == selSlot) canvas.drawRoundRect(x, y, w, h, 6, C_SEL);
    }
    if (nPages > 1) {
        char b[20]; snprintf(b, sizeof(b), "<  %d/%d  >", gridPage + 1, nPages);
        textC(b, SCR_W / 2, SCR_H - 12, 1, C_SEL);
    } else {
        textC(i18n::T(S_TOUCH_SLOT), SCR_W / 2, SCR_H - 14, 1, C_DIM);
    }
    canvas.pushSprite(0, 0);
}

static const Btn B_CANCEL = { 8, 268, 104, 44 };
static const Btn B_SEND   = { 128, 268, 104, 44 };
static const Btn B_FULL   = { 8, 268, 224, 44 };

static void drawScan() {
    canvas.fillScreen(C_BG);
    header(backend ? i18n::T(backend->connected() ? S_ONLINE : S_OFFLINE) : "");
    textC(String(i18n::T(S_SLOT)) + " " + (selSlot >= 0 && backend ? backend->slotLabel(selSlot) : "?"),
          SCR_W / 2, 34, 2, C_SEL);

    int cx = SCR_W / 2, cy = 118;
    static const SlotState EMPTY;
    const SlotState& s = (backend && selSlot >= 0) ? backend->slot(selSlot) : EMPTY;
    canvas.fillCircle(cx, cy, 40, s.known ? canvas.color565(s.r, s.g, s.b) : 0x2945);
    canvas.drawCircle(cx, cy, 40, C_TXT);
    int ph = (millis() / 100) % 8;
    for (int k = 0; k < 8; k++) {
        float a = k * PI / 4;
        canvas.fillCircle(cx + cos(a) * 54, cy + sin(a) * 54, 3, (k == ph) ? C_SEL : C_DIM);
    }
    textC(i18n::T(S_BRING_TAG), SCR_W / 2, 186, 2, C_TXT);
    textC(i18n::T(S_TO_READER), SCR_W / 2, 210, 2, C_TXT);
    if (resultMsg.length()) textC(resultMsg, SCR_W / 2, 242, 1, C_ERR);
    drawBtn(B_FULL, i18n::T(S_CANCEL), C_BTNR);
    canvas.pushSprite(0, 0);
}

static void drawReview() {
    canvas.fillScreen(C_BG);
    header(String("-> ") + (selSlot >= 0 && backend ? backend->slotLabel(selSlot) : "?"));
    canvas.fillRect(20, 28, SCR_W - 40, 70, canvas.color565(tag.r, tag.g, tag.b));
    canvas.drawRect(19, 27, SCR_W - 38, 72, C_TXT);
    textC(tag.material, SCR_W / 2, 112, 3, C_TXT);
    textC(tag.brand, SCR_W / 2, 146, 2, 0x07FF);
    char hx[10]; snprintf(hx, sizeof(hx), "#%02X%02X%02X", tag.r, tag.g, tag.b);
    textC(hx, SCR_W / 2, 172, 2, C_TXT);
    char t[44];
    snprintf(t, sizeof(t), "%s %u-%u C", i18n::T(S_NOZZLE), tag.nozMin, tag.nozMax);
    textC(t, SCR_W / 2, 198, 1, C_DIM);
    snprintf(t, sizeof(t), "%s %u-%u C", i18n::T(S_BED), tag.bedMin, tag.bedMax);
    textC(t, SCR_W / 2, 214, 1, C_DIM);
    snprintf(t, sizeof(t), i18n::T(S_SEND_TO), selSlot >= 0 && backend ? backend->slotLabel(selSlot) : "?");
    textC(t, SCR_W / 2, 238, 1, C_TXT);
    drawBtn(B_CANCEL, i18n::T(S_NO), C_BTNR);
    drawBtn(B_SEND, i18n::T(S_SEND), C_BTNG);
    canvas.pushSprite(0, 0);
}

static void drawResult() {
    canvas.fillScreen(C_BG);
    header(backend ? i18n::T(backend->connected() ? S_ONLINE : S_OFFLINE) : "");
    textC(sendOk ? i18n::T(S_OK) : i18n::T(S_ERR), SCR_W / 2, 108, 3, sendOk ? C_OKG : C_ERR);
    textC(resultMsg, SCR_W / 2, 158, 1, C_TXT);
    textC(i18n::T(S_TAP_BACK), SCR_W / 2, SCR_H - 16, 1, C_DIM);
    canvas.pushSprite(0, 0);
}

static void drawWifi(const String& msg) {
    canvas.fillScreen(C_BG);
    header("");
    textC("Wi-Fi", SCR_W / 2, 120, 3, C_SEL);
    textC(msg, SCR_W / 2, 168, 1, C_TXT);
    canvas.pushSprite(0, 0);
}

// ecra do modo AP de configuracao (aberto quando a ligacao Wi-Fi falha)
static void drawAP() {
    canvas.fillScreen(C_BG);
    header("");
    textC(i18n::T(S_AP_TITLE), SCR_W / 2, 24, 2, C_SEL);
    textC(i18n::T(S_WIFI_FAIL), SCR_W / 2, 58, 1, C_ERR);
    textC(i18n::T(S_AP_JOIN), SCR_W / 2, 96, 1, C_DIM);
    textC(webcfg::apName(), SCR_W / 2, 118, 2, C_TXT);
    textC(i18n::T(S_AP_OPEN), SCR_W / 2, 158, 1, C_DIM);
    textC("192.168.4.1", SCR_W / 2, 180, 2, C_TXT);
    textC(i18n::T(S_AP_CHOOSE), SCR_W / 2, 210, 1, C_DIM);
    int cl = webcfg::apClients();
    char b[28];
    if (cl) snprintf(b, sizeof(b), i18n::T(S_AP_CLIENTS), cl);
    else    snprintf(b, sizeof(b), "%s", i18n::T(S_AP_WAITING));
    textC(b, SCR_W / 2, SCR_H - 16, 1, cl ? C_OKG : C_DIM);
    canvas.pushSprite(0, 0);
}

// ---- NVS ---------------------------------------------------------------
// One-time migration from the prototype's namespaces.
//
// The prototype stored everything under "k2cfg" (and the account under
// "ttcfg"), names that belong to a Creality-only ancestor. Renaming them was
// right; dropping the data was not. Without this, every existing device reboots
// into the setup portal and its owner has to type a Wi-Fi password again to
// recover a device that was working - which is exactly the friction this
// product exists to remove.
//
// Runs once: the copy is persisted, so the old namespace is read at most one
// more time in the life of a device.
static void migrateLegacyConfig() {
    Preferences dst;
    dst.begin("tigerspool", true);
    bool alreadyDone = dst.getString("ssid", "").length() || dst.getBool("migrated", false);
    dst.end();
    if (alreadyDone) return;

    Preferences src;
    if (!src.begin("k2cfg", true)) return;
    String ssid = src.getString("ssid", "");
    if (ssid.isEmpty()) { src.end(); return; }

    dst.begin("tigerspool", false);
    dst.putString("ssid", ssid);
    dst.putString("pass", src.getString("pass", ""));
    dst.putInt("lang",    src.getInt("lang", 0));
    dst.putInt("printerIdx", src.getInt("psel", 0));
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char k[6];
        snprintf(k, sizeof(k), "p%dt", i); dst.putInt(k, src.getInt(k, 0));
        snprintf(k, sizeof(k), "p%dn", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%dh", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%ds", i); dst.putString(k, src.getString(k, ""));
        snprintf(k, sizeof(k), "p%dc", i); dst.putString(k, src.getString(k, ""));
    }
    dst.putBool("migrated", true);
    dst.end();
    src.end();

    // The account session lived in its own namespace and moves with it.
    Preferences oldAcc, newAcc;
    if (oldAcc.begin("ttcfg", true)) {
        String refresh = oldAcc.getString("refresh", "");
        if (refresh.length()) {
            newAcc.begin("tsaccount", false);
            newAcc.putString("refresh", refresh);
            newAcc.putString("email", oldAcc.getString("email", ""));
            newAcc.putString("uid",   oldAcc.getString("uid", ""));
            newAcc.end();
        }
        oldAcc.end();
    }
    Serial.printf("[config] migrated k2cfg -> tigerspool (network '%s')\n", ssid.c_str());
}

static void loadCfg() {
    nvs.begin("tigerspool", true);
    wifiSsid = nvs.getString("ssid", "");
    wifiPass = nvs.getString("pass", "");
    selectedPrinter     = nvs.getInt("printerIdx", 0);
    for (int i = 0; i < MAX_PRINTERS; i++) {
        char k[6];
        snprintf(k, sizeof(k), "p%dt", i); printers[i].type = (PrinterType)nvs.getInt(k, 0);
        snprintf(k, sizeof(k), "p%dn", i); printers[i].name = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%dh", i); printers[i].host = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%ds", i); printers[i].sn   = nvs.getString(k, "");
        snprintf(k, sizeof(k), "p%dc", i); printers[i].cc   = nvs.getString(k, "");
    }
    String oldK2 = nvs.getString("k2ip", "");
    nvs.end();

    // migracao do formato antigo (k2ip unico) -> impressora 0, e PERSISTE
    if (printers[0].type == PT_NONE && oldK2.length()) {
        printers[0].type = PT_CREALITY; printers[0].name = "K2"; printers[0].host = oldK2;
        nvs.begin("tigerspool", false);
        nvs.putInt("p0t", PT_CREALITY);
        nvs.putString("p0n", "K2");
        nvs.putString("p0h", oldK2);
        nvs.remove("k2ip");
        nvs.end();
        Serial.println("[config] migrado k2ip -> p0 (K2)");
    }
    if (selectedPrinter < 0 || selectedPrinter >= MAX_PRINTERS || printers[selectedPrinter].type == PT_NONE) {
        selectedPrinter = 0;
        for (int i = 0; i < MAX_PRINTERS; i++) if (printers[i].type != PT_NONE) { selectedPrinter = i; break; }
    }
}
static void saveSel(int i) {
    nvs.begin("tigerspool", false);
    nvs.putInt("printerIdx", i);
    nvs.end();
}

// ---- Wi-Fi / arranque ------------------------------------------------------
static const uint32_t WIFI_TIMEOUT_MS = 30000;   // 30 s por tentativa; se falhar -> portal AP

static bool wifiConnect() {
    if (wifiSsid.isEmpty()) { drawWifi(i18n::T(S_NO_NETWORK)); return false; }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
        int left = (int)((WIFI_TIMEOUT_MS - (millis() - t0)) / 1000) + 1;
        drawWifi(String(i18n::T(S_CONNECTING)) + " " + wifiSsid + "  " + left + "s");
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] timeout 30s sem ligacao");
        drawWifi(i18n::T(S_WIFI_FAIL));
        return false;
    }
    Serial.printf("[wifi] OK %s\n", WiFi.localIP().toString().c_str());
    return true;
}
static void onWifiUp() {
    if (!webStarted) { webcfg::begin(); webStarted = true; }
}
static void startConfigAP() {
    webcfg::beginAP();
    state = ST_AP;
    stateSince = millis();
}
static void goAfterLang() {
    if (wifiConnect()) { onWifiUp(); state = ST_PRINTER; stateSince = millis(); }
    else startConfigAP();                 // falhou -> abre o AP para escolher outra rede
}
static void backToPrinters() {
    screen_home::leave();          // force a full LVGL repaint on re-entry
    if (backend) { backend->stop(); backend = nullptr; }
    selSlot = -1; gridPage = 0;
    for (int i = 0; i < MAX_PRINTERS; i++) pLastSeen[i] = 0;   // re-sondar do zero
    // abre na pagina que contem a impressora activa
    int idx[MAX_PRINTERS], n = visiblePrinters(idx), pos = 0;
    for (int i = 0; i < n; i++) if (idx[i] == selectedPrinter) pos = i;
    prPage = pos / PR_PP;
    state = ST_PRINTER;
    stateSince = millis();
}
static void selectPrinter(int i) {
    if (i < 0 || i >= MAX_PRINTERS || printers[i].type == PT_NONE) return;
    if (backend) backend->stop();
    selectedPrinter = i; saveSel(i);
    switch (printers[i].type) {
        case PT_FF_C5:     backend = &flashForgeBackend;    break;
        case PT_BAMBU:     backend = &bambuBackend; break;
        case PT_SNAPMAKER: backend = &snapmakerBackend;  break;
        default:           backend = &crealityBackend;    break;
    }
    backend->begin(printers[i]);
    selSlot = -1; gridPage = 0; resultMsg = "";
    state = ST_GRID; stateSince = millis();
    Serial.printf("[ui] impressora %d '%s' (%s)\n", i, printers[i].name.c_str(), typeTag(printers[i].type));
}

// ---- setup / loop -----------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(150);
    lcd.init();
    lcd.setRotation(SCR_ROTATION);
    lcd.setBrightness(200);
    canvas.setPsram(true);
    canvas.setColorDepth(16);
    canvasReady = (canvas.createSprite(SCR_W, SCR_H) != nullptr);

    // LVGL owns the printer list. The remaining screens are still raw-drawn and
    // are being ported one at a time - see docs/MIGRATION.md.
    lvgl_port::begin();

    i18n::begin();
    migrateLegacyConfig();     // must run before anything reads the new namespace
    loadCfg();
    ttcloud::begin();

    nfcReady = reader::begin();
    if (!nfcReady) Serial.printf("[reader] %s\n", reader::lastError().c_str());

    if (i18n::chosen()) goAfterLang();
    else { state = ST_LANG; stateSince = millis(); }
}

void loop() {
    if (backend) backend->loop();
    if (webStarted || webcfg::apActive()) webcfg::loop();
    pollTouch();
    int tx, ty;

    if (!nfcReady && millis() - nfcLastTry > 2000) {
        nfcLastTry = millis();
        nfcReady = reader::begin();
        if (nfcReady) Serial.println("[reader] PN532 OK (retry)");
    }

    switch (state) {

    case ST_LANG:
        if (takeTap(tx, ty))
            for (int i = 0; i < LANG_N; i++)
                if (inBtn(rowBtn(i), tx, ty)) { i18n::set((Lang)i); goAfterLang(); break; }
        drawLang();
        break;

    case ST_WIFI:
        startConfigAP();                      // falha de Wi-Fi -> portal AP
        return;

    case ST_AP: {
        static int lastCl = -99; static uint32_t lastDraw = 0;
        int cl = webcfg::apClients();
        if (cl != lastCl || millis() - lastDraw > 2000) { drawAP(); lastCl = cl; lastDraw = millis(); }
        delay(120);
        return;
    }

    case ST_PRINTER: {
        // Re-sync com a conta TigerTag EM TAREFA SEPARADA. O ecra principal e
        // aquele onde o utilizador volta a toda a hora: nunca deve esperar pela
        // rede. A lista vem sempre do NVS (loadCfg no arranque); a sync so a
        // actualiza se algo mudou, e recarrega-se aqui, na boucle UI, para nao
        // haver corrida sobre printers[].
        if (ttcloud::due() && !ttcloud::asyncBusy()) ttcloud::startAsyncSync();
        {
            String s;
            if (ttcloud::asyncTake(s)) {
                if (ttcloud::consumeChanged()) {
                    loadCfg(); resultMsg = s;
                    for (int i = 0; i < MAX_PRINTERS; i++) pLastSeen[i] = 0;
                }
            }
        }
        probeTick();                 // background online/offline indicator
        disc::tick();                // LAN sweep when a Creality is unreachable

        {
            bool online[MAX_PRINTERS];
            for (int i = 0; i < MAX_PRINTERS; i++) online[i] = isOnline(i);
            screen_home::show(printers, MAX_PRINTERS, selectedPrinter,
                              online, ttcloud::asyncBusy());
        }
        lvgl_port::loop();

        {
            int tapped = screen_home::takeTappedPrinter();
            if (tapped >= 0) { screen_home::leave(); selectPrinter(tapped); }
            else if (screen_home::takeSettingsTap())
                Serial.println("[ui] settings tapped - screen not ported yet");
        }
        break;
    }

    case ST_GRID: {
        if (takeTap(tx, ty)) {
            if (inBack(tx, ty)) { backToPrinters(); break; }         // voltar -> escolher impressora
            int nc = backend ? backend->slotCount() : 0;
            int nPages = gridPages();
            if (nPages > 1 && ty >= SCR_H - 24) {                    // barra de paginacao
                gridPage = (tx < SCR_W / 2) ? (gridPage + nPages - 1) % nPages
                                            : (gridPage + 1) % nPages;
                drawGrid();
                break;
            }
            int base = gridPage * GRID_PP;
            for (int k = 0; k < GRID_PP && base + k < nc; k++) {
                int x, y, w, h; cellRect(k, x, y, w, h);
                if (tx >= x && tx < x + w && ty >= y && ty < y + h) {
                    selSlot = base + k; resultMsg = ""; state = ST_SCAN; stateSince = millis();
                    break;
                }
            }
        }
        drawGrid();
        break;
    }

    case ST_SCAN: {
        if (takeTap(tx, ty) && (inBack(tx, ty) || inBtn(B_FULL, tx, ty))) { state = ST_GRID; selSlot = -1; break; }
        if (reader::present()) {
            if (reader::read(tag) && tag.ok) { state = ST_REVIEW; stateSince = millis(); }
            else resultMsg = reader::lastError();
        }
        drawScan();
        break;
    }

    case ST_REVIEW: {
        if (takeTap(tx, ty)) {
            if (inBack(tx, ty) || inBtn(B_CANCEL, tx, ty)) { state = ST_GRID; selSlot = -1; }
            else if (inBtn(B_SEND, tx, ty)) {
                sendOk = backend && backend->connected() && backend->assign(selSlot, tag);
                char m[48];
                if (sendOk) snprintf(m, sizeof(m), i18n::T(S_UPDATED), backend->slotLabel(selSlot));
                else        snprintf(m, sizeof(m), "%s",
                                     (backend && backend->connected()) ? i18n::T(S_SEND_FAIL) : i18n::T(S_PRINTER_OFF));
                resultMsg = m;
                state = ST_RESULT; stateSince = millis();
            }
        }
        drawReview();
        break;
    }

    case ST_RESULT:
        if (takeTap(tx, ty) || millis() - stateSince > 2500) { state = ST_GRID; selSlot = -1; }
        drawResult();
        break;
    }

    delay(15);
}

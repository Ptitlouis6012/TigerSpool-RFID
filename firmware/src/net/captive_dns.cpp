#include "captive_dns.h"
#include <WiFiUdp.h>

namespace captive_dns {
namespace {

WiFiUDP  s_udp;
IPAddress s_ip;
bool     s_up = false;

constexpr uint16_t PORT       = 53;
constexpr size_t   HEADER     = 12;
constexpr size_t   MAX_PACKET = 512;   // a UDP DNS message never exceeds this
constexpr uint16_t TYPE_A     = 1;
constexpr uint16_t CLASS_IN   = 1;
constexpr uint32_t TTL_S      = 60;

// A name is a run of length-prefixed labels ending in a zero byte. Returns the
// offset just past it, or 0 if the packet runs out first or uses compression -
// a question section has no reason to be compressed, and refusing to guess is
// better than reading past the buffer.
size_t skipName(const uint8_t* p, size_t len, size_t at) {
    while (at < len) {
        uint8_t n = p[at];
        if (n == 0)          return at + 1;
        if ((n & 0xC0) != 0) return 0;      // compression pointer
        at += 1 + n;
    }
    return 0;
}

void put16(uint8_t* p, uint16_t v) { p[0] = v >> 8;   p[1] = v & 0xFF; }
void put32(uint8_t* p, uint32_t v) { p[0] = v >> 24;  p[1] = (v >> 16) & 0xFF;
                                     p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF; }

}  // namespace

void begin(const IPAddress& ip) {
    s_ip = ip;
    s_up = s_udp.begin(PORT);
    Serial.printf("[dns] captive resolver on %s: %s\n",
                  ip.toString().c_str(), s_up ? "up" : "FAILED to bind :53");
}

void end() { if (s_up) { s_udp.stop(); s_up = false; } }

void loop() {
    if (!s_up) return;

    int len = s_udp.parsePacket();
    if (len <= 0) return;
    if (len < (int)HEADER || len > (int)MAX_PACKET) { s_udp.flush(); return; }

    uint8_t buf[MAX_PACKET];
    int got = s_udp.read(buf, sizeof(buf));
    if (got < (int)HEADER) return;

    const bool     isResponse = (buf[2] & 0x80) != 0;
    const uint8_t  opcode     = (buf[2] >> 3) & 0x0F;
    const uint16_t qdcount    = (buf[4] << 8) | buf[5];
    if (isResponse || opcode != 0 || qdcount != 1) return;   // not ours to answer

    size_t qend = skipName(buf, got, HEADER);
    if (!qend || qend + 4 > (size_t)got) return;
    const uint16_t qtype  = (buf[qend] << 8) | buf[qend + 1];
    const uint16_t qclass = (buf[qend + 2] << 8) | buf[qend + 3];
    const size_t   qlen   = qend + 4 - HEADER;                // name + type + class

    // Answer only what we can answer truthfully. Anything that is not an
    // Internet A record gets an empty NOERROR - the name exists, it has no
    // record of that type - which is what lets a client stop waiting and use
    // the address it already has.
    const bool answer = (qtype == TYPE_A && qclass == CLASS_IN);

    uint8_t out[MAX_PACKET];
    size_t  n = 0;

    memcpy(out, buf, HEADER + qlen);                 // id, then the question back
    n = HEADER + qlen;

    out[2] = 0x84 | (buf[2] & 0x01);                 // response, authoritative, keep RD
    out[3] = 0x00;                                   // no recursion available, NOERROR
    put16(out + 6, answer ? 1 : 0);                  // ANCOUNT
    put16(out + 8, 0);                               // NSCOUNT
    put16(out + 10, 0);                              // ARCOUNT

    if (answer) {
        out[n++] = 0xC0; out[n++] = (uint8_t)HEADER; // pointer back to the name
        put16(out + n, TYPE_A);   n += 2;
        put16(out + n, CLASS_IN); n += 2;
        put32(out + n, TTL_S);    n += 4;
        put16(out + n, 4);        n += 2;            // RDLENGTH
        out[n++] = s_ip[0]; out[n++] = s_ip[1];
        out[n++] = s_ip[2]; out[n++] = s_ip[3];
    }

    s_udp.beginPacket(s_udp.remoteIP(), s_udp.remotePort());
    s_udp.write(out, n);
    s_udp.endPacket();
}

}  // namespace captive_dns

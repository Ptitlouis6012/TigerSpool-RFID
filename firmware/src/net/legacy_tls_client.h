#pragma once
#include <WiFiClientSecure.h>

// A WiFiClientSecure for a server whose certificates are signed with SHA-1,
// verified against ONE given CA and presenting a client certificate.
//
// WHY IT EXISTS. Anycubic's cloud broker has a certificate from Anycubic's own
// "AC Root CA", signed sha1WithRSAEncryption, and it wants mutual TLS. The
// Arduino client can do neither half of that together: with a CA it verifies
// under mbedTLS's default profile, which rejects SHA-1 signatures outright; and
// with setInsecure() it skips the client certificate as well as the check. This
// class performs the same handshake with a profile that also accepts SHA-1 -
// the chain is still checked, against that CA and nothing else - and then hands
// the session back to WiFiClientSecure, whose reads, writes and stop() are
// unchanged. It is used for that broker only: every other connection keeps the
// default profile and the root store (net/tls.h).
class LegacyTlsClient : public WiFiClientSecure {
public:
    // PEM strings; they must outlive the connection.
    void setIdentity(const char* caPem, const char* certPem, const char* keyPem) {
        ca_ = caPem; crt_ = certPem; key_ = keyPem;
    }

    int connect(const char* host, uint16_t port) override { return open(host, port); }
    int connect(const char* host, uint16_t port, int32_t) override { return open(host, port); }
    int connect(IPAddress ip, uint16_t port) override { return open(ip.toString().c_str(), port); }
    int connect(IPAddress ip, uint16_t port, int32_t) override { return open(ip.toString().c_str(), port); }

private:
    int open(const char* host, uint16_t port);
    int fail(int err);

    const char* ca_ = nullptr;
    const char* crt_ = nullptr;
    const char* key_ = nullptr;
};

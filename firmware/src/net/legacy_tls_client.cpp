#include "legacy_tls_client.h"
#include <WiFi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>

namespace {

// mbedTLS's default profile, plus SHA-1 - nothing else is relaxed: the same
// RSA and curve families, and RSA keys of 2048 bits and up.
const mbedtls_x509_crt_profile SHA1_PROFILE = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF,      // any public key algorithm
    0xFFFFFFF,      // any curve
    2048,
};

}  // namespace

int LegacyTlsClient::fail(int err) {
    _lastError = err;
    stop();                 // frees whatever was set up, as WiFiClientSecure does
    return 0;
}

int LegacyTlsClient::open(const char* host, uint16_t port) {
    if (!ca_ || !crt_ || !key_) return 0;
    stop();

    IPAddress ip;
    if (!WiFi.hostByName(host, ip)) return 0;

    sslclient_context* c = sslclient;
    const unsigned long hsTimeout = c->handshake_timeout;
    ssl_init(c);
    c->handshake_timeout = hsTimeout;

    // TCP, non-blocking so the connect is bounded, as the Arduino client does.
    c->socket = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (c->socket < 0) return fail(-1);
    fcntl(c->socket, F_SETFL, fcntl(c->socket, F_GETFL, 0) | O_NONBLOCK);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = (uint32_t)ip;
    lwip_connect(c->socket, (struct sockaddr*)&addr, sizeof(addr));
    fd_set wr; FD_ZERO(&wr); FD_SET(c->socket, &wr);
    struct timeval tv = { 5, 0 };
    if (select(c->socket + 1, nullptr, &wr, nullptr, &tv) <= 0) return fail(-1);
    int soErr = 0; socklen_t len = sizeof(soErr);
    getsockopt(c->socket, SOL_SOCKET, SO_ERROR, &soErr, &len);
    if (soErr) return fail(-1);
    c->socket_timeout = 5000;
    lwip_setsockopt(c->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(c->socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    const int on = 1;
    lwip_setsockopt(c->socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    lwip_setsockopt(c->socket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    int ret;
    mbedtls_entropy_init(&c->entropy_ctx);
    if ((ret = mbedtls_ctr_drbg_seed(&c->drbg_ctx, mbedtls_entropy_func, &c->entropy_ctx,
                                     (const uint8_t*)"legacy-tls", 10)) != 0) return fail(ret);
    if ((ret = mbedtls_ssl_config_defaults(&c->ssl_conf, MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) return fail(ret);

    // The server: verified, against this CA only, under the SHA-1 profile.
    mbedtls_x509_crt_init(&c->ca_cert);
    if ((ret = mbedtls_x509_crt_parse(&c->ca_cert, (const uint8_t*)ca_, strlen(ca_) + 1)) != 0)
        return fail(ret);
    mbedtls_ssl_conf_authmode(&c->ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&c->ssl_conf, &c->ca_cert, nullptr);
    mbedtls_ssl_conf_cert_profile(&c->ssl_conf, &SHA1_PROFILE);

    // This device: the client certificate the server asks for.
    mbedtls_x509_crt_init(&c->client_cert);
    mbedtls_pk_init(&c->client_key);
    if ((ret = mbedtls_x509_crt_parse(&c->client_cert, (const uint8_t*)crt_, strlen(crt_) + 1)) != 0)
        return fail(ret);
    if ((ret = mbedtls_pk_parse_key(&c->client_key, (const uint8_t*)key_, strlen(key_) + 1,
                                    nullptr, 0)) != 0) return fail(ret);
    if ((ret = mbedtls_ssl_conf_own_cert(&c->ssl_conf, &c->client_cert, &c->client_key)) != 0)
        return fail(ret);

    if ((ret = mbedtls_ssl_set_hostname(&c->ssl_ctx, host)) != 0) return fail(ret);
    mbedtls_ssl_conf_rng(&c->ssl_conf, mbedtls_ctr_drbg_random, &c->drbg_ctx);
    if ((ret = mbedtls_ssl_setup(&c->ssl_ctx, &c->ssl_conf)) != 0) return fail(ret);
    mbedtls_ssl_set_bio(&c->ssl_ctx, &c->socket, mbedtls_net_send, mbedtls_net_recv, nullptr);

    const uint32_t t0 = millis();
    while ((ret = mbedtls_ssl_handshake(&c->ssl_ctx)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) return fail(ret);
        if (millis() - t0 > c->handshake_timeout) return fail(-1);
        vTaskDelay(2);
    }
    if (mbedtls_ssl_get_verify_result(&c->ssl_ctx) != 0) return fail(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);

    _lastError = 0;
    _connected = true;
    return 1;
}

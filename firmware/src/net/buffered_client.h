#pragma once
#include <Arduino.h>
#include <Client.h>

// A read buffer in front of a Client, because PubSubClient reads ONE BYTE AT A
// TIME.
//
// Measured on the bench, with an X1C reporting once a second: a single report
// is about 30 KB, and PubSubClient pulls it through `read()` byte by byte. Over
// TLS each of those calls goes into mbedtls, and the whole report cost 74 ms of
// a loop that also has to draw the screen. The screen was blind for 8.9 of
// every 75 seconds, in gaps of up to 1.3 s, and that is what an interface that
// "freezes" actually is.
//
// Nothing about the protocol needs fixing; the reads do. This pulls whatever
// the socket already has into a local buffer with one call, and answers
// PubSubClient's byte-at-a-time reads out of RAM.
//
// It is a Client, so it substitutes anywhere one is taken - MQTT over TLS, MQTT
// in the clear, a socket read by any other library with the same habit.
template <size_t N>
class BufferedClient : public Client {
public:
    explicit BufferedClient(Client& inner) : in_(inner) {}

    // ---- the buffer ------------------------------------------------------
    int available() override {
        if (n_ > i_) return (int)(n_ - i_) + in_.available();
        return in_.available();
    }

    int read() override {
        if (i_ >= n_ && !fill()) return -1;
        return buf_[i_++];
    }

    int read(uint8_t* dst, size_t len) override {
        size_t done = 0;
        while (done < len) {
            if (i_ >= n_ && !fill()) break;
            const size_t take = min(len - done, n_ - i_);
            memcpy(dst + done, buf_ + i_, take);
            i_   += take;
            done += take;
        }
        return (int)done;
    }

    int peek() override {
        if (i_ >= n_ && !fill()) return -1;
        return buf_[i_];
    }

    void flush() override { in_.flush(); }

    // ---- everything else is the inner client -----------------------------
    int connect(IPAddress ip, uint16_t port) override { drop(); return in_.connect(ip, port); }
    int connect(const char* host, uint16_t port) override { drop(); return in_.connect(host, port); }
    size_t write(uint8_t b) override { return in_.write(b); }
    size_t write(const uint8_t* b, size_t n) override { return in_.write(b, n); }
    void stop() override { drop(); in_.stop(); }
    uint8_t connected() override { return (i_ < n_) ? 1 : in_.connected(); }
    operator bool() override { return (bool)in_; }

private:
    // One read of whatever is there. Never blocks for bytes that have not
    // arrived: `available()` is asked first, so this only ever moves what the
    // socket already holds.
    bool fill() {
        i_ = n_ = 0;
        const int have = in_.available();
        if (have <= 0) {
            // Nothing buffered below either - fall back to a single read, so a
            // caller that is prepared to block still can.
            const int one = in_.read();
            if (one < 0) return false;
            buf_[0] = (uint8_t)one;
            n_ = 1;
            return true;
        }
        const size_t want = (size_t)have > N ? N : (size_t)have;
        const int got = in_.read(buf_, want);
        if (got <= 0) return false;
        n_ = (size_t)got;
        return true;
    }
    void drop() { i_ = n_ = 0; }

    Client& in_;
    uint8_t buf_[N];
    size_t  i_ = 0, n_ = 0;
};

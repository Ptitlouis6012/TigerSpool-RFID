#pragma once

// How many arcs of the Wi-Fi wave to light, from a signal that never sits still.
//
// Two problems, one answer, and both are about what a person sees rather than
// about radio.
//
// THE SCALE sits 10 dB below where a textbook would put it, deliberately. The
// ESP32's receiver reads low, and by an amount that is not constant between
// boards: two of our units, five centimetres apart, reported -47 and -60 dBm on
// the same access point. A scale built for the numbers a phone shows therefore
// tells a user their network is failing while the device works perfectly. The
// Wi-Fi screen still prints the dBm beside the word, so nothing is hidden from
// anyone diagnosing a real problem - the scale decides the adjective.
//
// THE FLICKER is the second. A signal wandering across a threshold - -70, -71,
// -70 - moved the wave every second, and a user watching an icon twitch reads
// it as a device in trouble. So the reading is smoothed, and the level has to
// be EARNED: it takes 3 dB past a boundary to gain an arc and 3 dB back to lose
// one, which is more than the wobble and less than a real change.
//
// Header-only and free of LVGL on purpose: the arithmetic is what can be got
// wrong, and this way it can be run and checked on a computer.

namespace signal_level {

// Where each level begins, in dBm: level 1 at -90, level 2 at -80, level 3 at
// -70. Level 0 is everything below the first.
inline int floorFor(int level) {
    switch (level) {
        case 3:  return -70;
        case 2:  return -80;
        case 1:  return -90;
        default: return -1000;
    }
}

// The instantaneous answer, with no memory: what the portal's own copy of this
// arithmetic does, and the starting point for the smoothed one.
inline int fromRssi(int rssi) {
    if (rssi >= -70) return 3;
    if (rssi >= -80) return 2;
    if (rssi >= -90) return 1;
    return 0;
}

// How far past a boundary the signal must go before the wave changes.
inline constexpr int MARGIN_DB = 3;

// How often the average is allowed to move, and the jump that bypasses it.
inline constexpr unsigned SAMPLE_MS = 1000;
// A change this large is not noise - somebody moved the box, or the device
// changed access point - and waiting five seconds to show it would be its own
// kind of wrong.
inline constexpr int JUMP_DB = 15;

// The state the smoothing keeps between readings. One radio, so one of these.
//
// Time-based, NOT per call: the screens ask for this on every pass of the main
// loop, which is tens of times a second, and an average that moved on every
// call would converge before the wobble it exists to absorb had happened. One
// sample a second, whoever asks and however often.
struct Smoother {
    float    avg    = 0.0f;   // 0 means "nothing seen yet"
    int      level  = -1;     // -1 means "not decided yet"
    unsigned lastAt = 0;      // millis of the last sample taken

    // `rssi` in dBm, or 0 when there is no connection at all. `nowMs` is
    // millis(); it is passed in rather than read here so this stays testable on
    // a computer.
    int update(int rssi, unsigned nowMs) {
        if (rssi == 0) { avg = 0.0f; level = -1; lastAt = 0; return 0; }

        const bool jumped = avg != 0.0f && (rssi - avg > JUMP_DB || avg - rssi > JUMP_DB);
        if (avg != 0.0f && !jumped && lastAt && nowMs - lastAt < SAMPLE_MS)
            return level < 0 ? fromRssi((int)avg) : level;
        lastAt = nowMs;

        // Four fifths of the old value: the reading moves a decibel or two
        // every second on a still desk, and this turns that into a number that
        // only moves when the signal does. A real jump replaces it outright.
        if (avg == 0.0f || jumped) avg = (float)rssi;
        else                       avg = avg * 0.8f + (float)rssi * 0.2f;

        if (level < 0) { level = fromRssi((int)avg); return level; }

        // Up one arc only once the smoothed reading is MARGIN past the floor of
        // the next level, and down one only once it is MARGIN below the floor
        // of this one. Between the two nothing happens, which is the point.
        if (level < 3 && avg >= (float)(floorFor(level + 1) + MARGIN_DB)) level++;
        else if (level > 0 && avg <= (float)(floorFor(level) - MARGIN_DB))  level--;
        return level;
    }
};

}  // namespace signal_level

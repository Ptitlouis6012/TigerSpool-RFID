#pragma once
#include "printer.h"

// Bambu Lab A1 / A1 mini / A2L / P1 / X1 / H2D over the LAN.
//
// MQTT on port 8883 with TLS. Authentication is the printer's own access code
// from its screen (LAN mode must be on), with user "bblp", and the serial
// number forms the topic: device/<serial>/{report,request}.
//
// Slots are discovered, not assumed. The `pushall` report carries
// print.ams.ams[], and the count runs from five (external spool plus one AMS
// Lite) to seventeen (external plus four units of four). Until the first report
// arrives an AMS Lite is assumed. Labels are "A".."D" with a single unit and
// "1A".."4D" with several.
//
// The report is large - a fully loaded X1 sends around 50 KB - so the MQTT
// receive buffer is sized for it and the JSON is parsed through a filter.
class BambuBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    int  slotCount() override;                      // dynamic, 5..17
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
};

#pragma once
#include "printer.h"

// Bambu Lab A1 / A1 mini / A2L / P1 / X1 / H2D - protocolo LAN por MQTT (porta
// 8883, TLS). Auth: user "bblp", pass = access code do ecra da impressora
// (Modo LAN). Precisa tambem do serial (topico device/<id>/{report,request}).
//
// Slots: descobertos do relatorio `pushall` (print.ams.ams[]). Ext (spool
// externo) sempre; depois 0..4 unidades x 4 tabuleiros. Ate ao 1o relatorio
// assume-se AMS Lite (Ext + A..D). Rotulos: "A".."D" com 1 unidade, "1A".."4D"
// com varias.
class BambuBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    int  slotCount() override;                      // dinamico (1..17)
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
};

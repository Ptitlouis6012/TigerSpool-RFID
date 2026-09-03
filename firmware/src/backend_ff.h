#pragma once
#include "printer.h"

// FlashForge Creator 5 / 5 Pro  -  API HTTP REST na porta 8898.
// Auth CheckCode (serialNumber + checkCode em cada pedido). Modo LAN ativo.
class FlashForgeC5Backend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    bool connected() override;
    int  slotCount() override { return 4; }        // estacao de 4 slots (T1..T4)
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
private:
    void tryAuth();
};

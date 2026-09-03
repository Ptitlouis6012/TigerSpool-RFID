#pragma once
#include "printer.h"

// Snapmaker (Artisan / J1 / J1s / U1) - Moonraker (Klipper) por WebSocket:
//   ws://<ip>:7125/websocket , JSON-RPC 2.0 , sem autenticacao.
// 4 slots = 4 extrusores (CONFIG_EXTRUDER 0..3). Atribuir filamento:
//   printer.gcode.script -> "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=n
//     VENDOR=.. FILAMENT_TYPE=.. FILAMENT_SUBTYPE= FILAMENT_COLOR_RGBA=RRGGBBFF"
// Estado dos slots: objecto print_task_config (filament_color_rgba[],
//   filament_type[], filament_vendor[]).  Fonte: TigerTag-Studio-Manager
//   renderer/printers/snapmaker/PROTOCOL.md.
class SnapmakerBackend : public PrinterBackend {
public:
    void begin(const PrinterCfg& cfg) override;
    void loop() override;
    void stop() override;
    bool connected() override;
    int  slotCount() override { return 4; }
    const char* slotLabel(int i) override;
    const SlotState& slot(int i) override;
    bool assign(int i, const TagInfo& t) override;
    String status() override;
    void refresh() override;
};

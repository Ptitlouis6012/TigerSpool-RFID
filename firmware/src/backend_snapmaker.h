#pragma once
#include "printer.h"

// Snapmaker (Artisan / J1 / J1s / U1) - Moonraker (Klipper) over WebSocket:
//   ws://<ip>:7125/websocket , JSON-RPC 2.0 , no authentication.
// 4 slots = 4 extruders (CONFIG_EXTRUDER 0..3). Assigning filament:
//   printer.gcode.script -> "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=n
//     VENDOR=.. FILAMENT_TYPE=.. FILAMENT_SUBTYPE= FILAMENT_COLOR_RGBA=RRGGBBFF"
// Slot state: the print_task_config object (filament_color_rgba[],
//   filament_type[], filament_vendor[]).  Source: TigerTag-Studio-Manager
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

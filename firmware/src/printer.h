#pragma once
#include <Arduino.h>
#include "reader.h"

#define MAX_PRINTERS 8

// tipo de backend
enum PrinterType : uint8_t { PT_NONE = 0, PT_CREALITY = 1, PT_FF_C5 = 2, PT_BAMBU = 3, PT_SNAPMAKER = 4 };

// config de uma impressora (vem do NVS, escrito pelo portal web)
struct PrinterCfg {
    PrinterType type = PT_NONE;
    String name;        // rotulo mostrado no ecra principal
    String host;        // IP
    String sn;          // FlashForge: serial number  | Bambu: serial number
    String cc;          // FlashForge: check code     | Bambu: access code
};

// estado conhecido de um slot (lido da impressora)
struct SlotState {
    bool    known = false;
    String  type;                    // "PLA", "PETG"...
    uint8_t r = 90, g = 90, b = 90;  // cor (cinza se desconhecida)
    uint8_t percent = 0;
    bool    selected = false;        // carregado / ativo
};

// interface comum aos backends (K2, FlashForge C5, ...)
class PrinterBackend {
public:
    virtual ~PrinterBackend() {}
    virtual void begin(const PrinterCfg& cfg) = 0;
    virtual void loop() = 0;
    virtual void stop() {}                          // fecha ligacoes (liberta heap)
    virtual bool connected() = 0;
    virtual int  slotCount() = 0;
    virtual const char* slotLabel(int i) = 0;      // "Suporte"/"1A".. ou "T1".."T4"
    virtual const SlotState& slot(int i) = 0;
    virtual bool assign(int i, const TagInfo& t) = 0;   // envia material+cor da tag
    virtual String status() = 0;
    virtual void refresh() = 0;                     // volta a pedir o estado dos slots
};

#pragma once
#include <Arduino.h>

enum Lang : uint8_t { LANG_PT = 0, LANG_EN, LANG_ES, LANG_FR, LANG_N };

enum StrId : uint8_t {
    S_TOUCH_SLOT = 0,   // "toca num slot"
    S_SLOT,             // "Slot"
    S_BRING_TAG,        // "Aproxima a TigerTag"
    S_TO_READER,        // "ao leitor"
    S_CANCEL,           // "CANCELAR"
    S_NO,               // "NAO"
    S_SEND,             // "ENVIAR"
    S_SEND_TO,          // "Enviar para %s ?"
    S_NOZZLE,           // "Bico"
    S_BED,              // "Cama"
    S_OK,               // "OK"
    S_ERR,              // "ERRO"
    S_TAP_BACK,         // "toca para voltar"
    S_CONNECTING,       // "a ligar a"
    S_WIFI_FAIL,        // "falha de ligacao"
    S_NO_NETWORK,       // "sem rede configurada"
    S_CONFIG_HINT,      // "Config: flash tools/wifi_portal"
    S_UPDATED,          // "%s atualizado"
    S_PRINTER_OFF,           // "impressora offline"
    S_SEND_FAIL,        // "falha no envio"
    S_HOLDER,           // "Suporte"
    S_CHOOSE_LANG,      // "Escolhe o idioma"
    S_READ_UNSTABLE,    // "leitura instavel"
    S_BLANK_TAG,        // "tag em branco"
    S_PRINTER,          // "Impressora"
    S_NO_PRINTERS,      // "Sem impressoras."
    S_TT_LINKED,        // "conta TigerTag ligada"
    S_ADD_WEB,          // "adiciona no browser"
    S_CONFIG_WEB,       // "config:"
    S_AP_TITLE,         // "CONFIG WI-FI"
    S_AP_JOIN,          // "Liga o telemovel a rede:"
    S_AP_OPEN,          // "Depois abre no browser:"
    S_AP_CHOOSE,        // "e escolhe outra rede."
    S_AP_WAITING,       // "a aguardar ligacao"
    S_AP_CLIENTS,       // "%d ligado(s)"
    S_TT_IMPORTING,     // "a importar maquinas..."
    S_TT_ACCOUNT,       // "Conta TigerTag"
    S_ONLINE,           // "ligado"
    S_OFFLINE,          // "offline"
    S_BACK,             // "< back"
    S_FIND_PRINTERS,    // "a procurar impressoras..."
    S_NO_ONLINE,        // "sem impressoras online"
    S_COUNT
};

namespace i18n {
    void  begin();                 // carrega do NVS
    bool  chosen();                // ja foi escolhido alguma vez?
    void  set(Lang l);             // grava no NVS
    Lang  current();
    const char* T(StrId id);       // string na lingua atual
    const char* name(Lang l);      // "Portugues" / "English" / ...
}

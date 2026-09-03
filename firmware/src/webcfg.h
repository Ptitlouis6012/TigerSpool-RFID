#pragma once
#include <Arduino.h>

// Servidor web de configuracao.
//  - begin()   : modo normal (ligado a rede)  ->  http://<ip>/  , http://tigerspool.local/
//  - beginAP() : modo AP + portal captivo (usado quando a ligacao Wi-Fi falha)
//                AP "TigerSpool-Setup"  ->  http://192.168.4.1/  (com lista de redes)
// Edita Wi-Fi, idioma e as 4 impressoras; grava no NVS "tigerspool" e reinicia.
namespace webcfg {
    void begin();              // servidor em modo STA (rede ligada)
    void beginAP();            // servidor em modo AP + portal captivo
    void loop();               // chamar no loop() principal
    bool apActive();           // true se o AP de config esta ligado
    String url();              // endereco a mostrar ao utilizador
    const char* apName();      // nome do AP
    int  apClients();          // nr de dispositivos ligados ao AP
}

#include "i18n.h"
#include <Preferences.h>

// Sem acentos: a fonte por omissao do LovyanGFX so tem ASCII.
// Uma linha por string, as 4 linguas juntas (ordem: PT, EN, ES, FR).
struct L4 { const char* s[LANG_N]; };

static const L4 STR[S_COUNT] = {
  /* S_TOUCH_SLOT   */ {{ "toca num slot", "tap a slot", "toca una ranura", "touchez un emplacement" }},
  /* S_SLOT         */ {{ "Slot", "Slot", "Ranura", "Empl." }},
  /* S_BRING_TAG    */ {{ "Aproxima a TigerTag", "Bring the TigerTag", "Acerca la TigerTag", "Approchez la TigerTag" }},
  /* S_TO_READER    */ {{ "ao leitor", "to the reader", "al lector", "du lecteur" }},
  /* S_CANCEL       */ {{ "CANCELAR", "CANCEL", "CANCELAR", "ANNULER" }},
  /* S_NO           */ {{ "NAO", "NO", "NO", "NON" }},
  /* S_SEND         */ {{ "ENVIAR", "SEND", "ENVIAR", "ENVOYER" }},
  /* S_SEND_TO      */ {{ "Enviar para %s ?", "Send to %s ?", "Enviar a %s ?", "Envoyer vers %s ?" }},
  /* S_NOZZLE       */ {{ "Bico", "Nozzle", "Boquilla", "Buse" }},
  /* S_BED          */ {{ "Cama", "Bed", "Cama", "Lit" }},
  /* S_OK           */ {{ "OK", "OK", "OK", "OK" }},
  /* S_ERR          */ {{ "ERRO", "ERROR", "ERROR", "ERREUR" }},
  /* S_TAP_BACK     */ {{ "toca para voltar", "tap to go back", "toca para volver", "touchez pour revenir" }},
  /* S_CONNECTING   */ {{ "a ligar a", "connecting to", "conectando a", "connexion a" }},
  /* S_WIFI_FAIL    */ {{ "falha de ligacao", "connection failed", "fallo de conexion", "echec de connexion" }},
  /* S_NO_NETWORK   */ {{ "sem rede configurada", "no network configured", "sin red configurada", "aucun reseau configure" }},
  /* S_CONFIG_HINT  */ {{ "Config: flash tools/wifi_portal", "Config: flash tools/wifi_portal", "Config: flash tools/wifi_portal", "Config: flash tools/wifi_portal" }},
  /* S_UPDATED      */ {{ "%s atualizado", "%s updated", "%s actualizado", "%s mis a jour" }},
  /* S_PRINTER_OFF       */ {{ "impressora offline", "printer offline", "impresora desconectada", "imprimante hors ligne" }},
  /* S_SEND_FAIL    */ {{ "falha no envio", "send failed", "fallo al enviar", "echec de l envoi" }},
  /* S_HOLDER       */ {{ "Suporte", "Spool", "Soporte", "Support" }},
  /* S_CHOOSE_LANG  */ {{ "Escolhe o idioma", "Choose language", "Elige el idioma", "Choisissez la langue" }},
  /* S_READ_UNSTABLE*/ {{ "leitura instavel", "unstable read", "lectura inestable", "lecture instable" }},
  /* S_BLANK_TAG    */ {{ "tag em branco", "blank tag", "etiqueta vacia", "tag vierge" }},
  /* S_PRINTER      */ {{ "Impressora", "Printer", "Impresora", "Imprimante" }},
  /* S_NO_PRINTERS  */ {{ "Sem impressoras.", "No printers.", "Sin impresoras.", "Aucune imprimante." }},
  /* S_TT_LINKED    */ {{ "conta TigerTag ligada", "TigerTag account linked", "cuenta TigerTag conectada", "compte TigerTag lie" }},
  /* S_ADD_WEB      */ {{ "adiciona no browser", "add via browser", "anade en el navegador", "ajoute via le navigateur" }},
  /* S_CONFIG_WEB   */ {{ "config:", "config:", "config:", "config:" }},
  /* S_AP_TITLE     */ {{ "CONFIG WI-FI", "WI-FI SETUP", "CONFIG WI-FI", "CONFIG WI-FI" }},
  /* S_AP_JOIN      */ {{ "Liga o telemovel a rede:", "Join this network:", "Conecta a la red:", "Rejoins ce reseau :" }},
  /* S_AP_OPEN      */ {{ "Depois abre no browser:", "Then open in a browser:", "Luego abre en el navegador:", "Puis ouvre dans un navigateur :" }},
  /* S_AP_CHOOSE    */ {{ "e escolhe outra rede.", "and pick another network.", "y elige otra red.", "et choisis un autre reseau." }},
  /* S_AP_WAITING   */ {{ "a aguardar ligacao", "waiting for a device", "esperando conexion", "en attente d une connexion" }},
  /* S_AP_CLIENTS   */ {{ "%d ligado(s)", "%d connected", "%d conectado(s)", "%d connecte(s)" }},
  /* S_TT_IMPORTING */ {{ "a importar maquinas...", "importing machines...", "importando maquinas...", "import des machines..." }},
  /* S_TT_ACCOUNT   */ {{ "Conta TigerTag", "TigerTag account", "Cuenta TigerTag", "Compte TigerTag" }},
  /* S_ONLINE       */ {{ "ligado", "online", "conectado", "en ligne" }},
  /* S_OFFLINE      */ {{ "offline", "offline", "desconectado", "hors ligne" }},
  /* S_BACK         */ {{ "< voltar", "< back", "< volver", "< retour" }},
  /* S_FIND_PRINTERS*/ {{ "a procurar impressoras...", "finding printers...", "buscando impresoras...", "recherche des imprimantes..." }},
  /* S_NO_ONLINE    */ {{ "sem impressoras online", "no printers online", "sin impresoras en linea", "aucune imprimante en ligne" }},
};

static const char* LNAME[LANG_N] = { "Portugues", "English", "Espanol", "Francais" };

namespace {
    Preferences p;
    Lang g_lang = LANG_PT;
    bool g_chosen = false;
}

void i18n::begin() {
    p.begin("tigerspool", true);
    int v = p.getInt("lang", -1);
    p.end();
    if (v >= 0 && v < LANG_N) { g_lang = (Lang)v; g_chosen = true; }
}
bool i18n::chosen()      { return g_chosen; }
Lang i18n::current()     { return g_lang; }
const char* i18n::name(Lang l) { return LNAME[l < LANG_N ? l : 0]; }
const char* i18n::T(StrId id)  { return STR[id < S_COUNT ? id : 0].s[g_lang]; }

void i18n::set(Lang l) {
    g_lang = l; g_chosen = true;
    p.begin("tigerspool", false);
    p.putInt("lang", (int)l);
    p.end();
}

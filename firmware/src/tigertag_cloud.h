#pragma once
#include <Arduino.h>

// Liga a conta Firebase da TigerTag e importa as impressoras do utilizador
// (users/{uid}/printers/{brand}/devices) para o NVS "tigerspool" (p0..p3).
// Login so pelo portal web; re-sync periodico quando o dispositivo esta idle.
namespace ttcloud {
    void   begin();                       // carrega sessao do NVS "tsaccount"
    bool   haveSession();
    String email();
    String lastResult();

    // login email/password -> guarda refreshToken/uid/email no NVS
    bool   signIn(const String& mail, const String& pass, String& err);
    void   forget();                      // apaga a sessao

    // login com conta Google (contas sem password) - fluxo de pareamento via
    // cloud TigerTag, igual ao Tiger-Scale-V3 (docs/ACCOUNT-PAIRING.md):
    //  1) pairStart()  -> code + verifyUrl (mostrar ao utilizador) + pollToken
    //  2) pairPoll(pollToken) repetido ate 1 (approved) -> customToken + email
    //  3) signInWithCustomToken() -> guarda a sessao (uid vem do JWT, nao do body)
    bool   pairStart(String& code, String& verifyUrl, String& pollToken,
                     int& intervalS, String& err);
    int    pairPoll(const String& pollToken, String& customToken,
                    String& emailOut, String& err);   // <0 erro, 0 pendente, 1 ok, 2 recusado, 3 expirado
    bool   signInWithCustomToken(const String& customToken, const String& emailHint,
                                 String& err);

    bool   due();                         // ha uma sincronizacao pendente?
    bool   syncNow(String& summary);      // le o Firestore e escreve p0..p3 (bloqueante ~10s)
    bool   consumeChanged();              // true (uma vez) se o ultimo sync alterou a config

    // sync em tarefa separada - o ecra principal nunca espera pela rede
    bool   startAsyncSync();              // false se ja houver uma a correr
    bool   asyncBusy();
    bool   asyncTake(String& summary);    // true uma vez quando termina
}

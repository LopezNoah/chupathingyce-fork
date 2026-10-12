/* port: local bot-match setup overlay (bot_lobby.c). */
#ifndef HALO_BOT_LOBBY_H
#define HALO_BOT_LOBBY_H
#include "cseries/cseries.h"
struct halo_ui_pointer;
boolean bot_lobby_active(void);
void bot_lobby_open(void);
void bot_lobby_process(void);
void bot_lobby_render(void);
void bot_lobby_pointer(struct halo_ui_pointer const *pointer);
#endif

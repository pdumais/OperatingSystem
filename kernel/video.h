#pragma once

#include "includes/kernel/types.h"

typedef struct
{
    char backBuffer[4096];
    bool    active;
} Screen;

void video_init();
Screen* video_create_screen();
char* video_get_buffer(Screen* scr);
void video_update_cursor(Screen* scr, bool on, uint16_t position);
void video_change_active_screen(Screen* oldscr, Screen* newscr);
void video_get_dimensions(Screen* src, uint32_t* width, uint32_t* height);

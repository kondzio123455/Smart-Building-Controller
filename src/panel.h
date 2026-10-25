#ifndef PANEL_H
#define PANEL_H

#include <stdbool.h>
#include "building.h"

/* The control panel: everything that used to crowd the main screen lives here. */
void panel_toggle(void);
bool panel_visible(void);
bool panel_contains(int x, int y);       /* is this screen point over the open panel? */
void panel_update(Building *b);          /* every frame: key and mouse input */
void panel_draw(const Building *b);      /* after the game screen; draws only when open */

#endif
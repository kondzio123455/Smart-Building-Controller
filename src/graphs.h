#ifndef GRAPHS_H
#define GRAPHS_H

#include <stdbool.h>
#include "building.h"

void graphs_update(const Building *b, float dt);   /* every frame, after building_update */
void graphs_draw(const Building *b);               /* after ui_draw; draws only when open */
void graphs_toggle(void);                          /* open/close the dashboard */
bool graphs_visible(void);

#endif

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdbool.h>
#include "building.h"

/* The building overview: a floor plan with live readings, alarms, energy and the thermostat schedule. */
void dashboard_toggle(void);
void dashboard_show(bool show);
bool dashboard_visible(void);
void dashboard_update(Building *b);       /* every frame: the B key and mouse clicks */
void dashboard_draw(const Building *b);

#endif

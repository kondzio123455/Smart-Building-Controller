#include <string.h>
#include "building.h"

static void add_room(Building *b, const char *name, float temp)
{
    if (b->roomCount >= MAX_ROOMS) return;

    Room *r = &b->rooms[b->roomCount];
    memset(r, 0, sizeof(Room));

    r->id = b->roomCount;
    strncpy(r->name, name, ROOM_NAME_LEN - 1);

    r->sensors.temperature = temp;
    r->sensors.targetTemp  = 21.0f;
    r->sensors.lightLevel  = 0.5f;

    r->actuators.lightsOn   = true;
    r->actuators.doorLocked = true;
    r->actuators.hvacOn     = true;

    r->alert = ALERT_NONE;
    b->roomCount++;
}

void building_init(Building *b)
{
    memset(b, 0, sizeof(Building));

    add_room(b, "Lobby",       20.5f);
    add_room(b, "Office",      22.0f);
    add_room(b, "Server Room", 24.0f);
    add_room(b, "Kitchen",     23.0f);

    b->selectedRoom = 0;
}

void room_evaluate_alert(Room *r)
{
    if (r->sensors.smokeDetected) {
        r->alert = ALERT_CRITICAL;
    } else if (r->sensors.temperature > 28.0f || r->sensors.temperature < 15.0f) {
        r->alert = ALERT_WARNING;
    } else {
        r->alert = ALERT_NONE;
    }
}

void building_update(Building *b, float dt)
{
    (void)dt; /* unused until we add temperature drift and motion events */

    for (int i = 0; i < b->roomCount; i++) {
        room_evaluate_alert(&b->rooms[i]);
    }
}

void room_set_lights(Room *r, bool on)
{
    r->actuators.lightsOn = on;
}

void room_set_lock(Room *r, bool locked)
{
    r->actuators.doorLocked = locked;
}

void room_set_target_temp(Room *r, float temp)
{
    r->sensors.targetTemp = temp;
}
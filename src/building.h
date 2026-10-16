#ifndef BUILDING_H
#define BUILDING_H

#include <stdbool.h>

#define MAX_ROOMS        8
#define ROOM_NAME_LEN    32

/* Night runs 12 AM to 6 AM. Lower SECONDS_PER_HOUR to test quickly. */
#define NIGHT_HOURS      6
#define SECONDS_PER_HOUR 90.0f

typedef struct {
    float temperature;
    float targetTemp;
    bool  motionDetected;
    float secondsSinceMotion;
    bool  doorOpen;
    bool  smokeDetected;
    float lightLevel;
} SensorData;

typedef struct {
    bool lightsOn;
    bool doorLocked;
    bool hvacOn;
} Actuators;

typedef enum {
    ALERT_NONE,
    ALERT_WARNING,
    ALERT_CRITICAL
} AlertLevel;

typedef enum {
    GAME_PLAYING,
    GAME_OVER,
    GAME_WON
} GameState;

typedef struct {
    int        id;
    char       name[ROOM_NAME_LEN];
    SensorData sensors;
    Actuators  actuators;
    AlertLevel alert;
} Room;

typedef struct {
    Room      rooms[MAX_ROOMS];
    int       roomCount;
    int       selectedRoom;
    GameState state;
    float     nightTime;
} Building;

void building_init(Building *b);
void building_update(Building *b, float dt);
void room_evaluate_alert(Room *r);
int  building_hour(const Building *b);
void game_lose(Building *b);

void room_set_lights(Room *r, bool on);
void room_set_lock(Room *r, bool locked);
void room_set_target_temp(Room *r, float temp);

void ui_handle_input(Building *b);
void ui_draw(const Building *b);

#endif /* BUILDING_H */
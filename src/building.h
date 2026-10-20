#ifndef BUILDING_H
#define BUILDING_H

#include <stdbool.h>

#define MAX_ROOMS         8
#define ROOM_NAME_LEN     32
#define MAX_INTRUDERS     5
#define INTRUDER_NAME_LEN 16

/* The Office is where you sit. An intruder getting in here ends the night. */
#define GOAL_ROOM         0

/* Night runs 12 AM to 6 AM. Lower SECONDS_PER_HOUR to test quickly. */
#define NIGHT_HOURS       6
#define SECONDS_PER_HOUR  90.0f

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

typedef enum {
    INTRUDER_DRIFTER,   /* walks toward the Office, one room at a time */
    INTRUDER_SHADE,     /* freezes while a camera is on it */
    INTRUDER_CRAWLER,   /* uses the vents: can skip a room and its locked door */
    INTRUDER_GLITCH,    /* kills the lights in whatever room it is in */
    INTRUDER_LEECH      /* drains extra power while it is loose */
} IntruderType;

typedef struct {
    IntruderType type;
    char  name[INTRUDER_NAME_LEN];
    int   room;         /* index into Building.rooms */
    int   home;         /* where it starts, and where it retreats to when blocked */
    int   startHour;    /* wakes up at this hour of the night */
    bool  active;
    float timer;        /* seconds until its next move attempt */
    float interval;     /* seconds between move attempts */
    int   aggression;   /* 0-20: chance out of 20 that an attempt succeeds */
} Intruder;

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

    Intruder  intruders[MAX_INTRUDERS];
    int       intruderCount;
    float     power;        /* 100.0 down to 0.0 */
    bool      powerOut;
    int       killer;       /* index of the intruder that got in, or -1 */
} Building;

void  building_init(Building *b);
void  building_update(Building *b, float dt);
void  room_evaluate_alert(Room *r);
int   building_hour(const Building *b);
float building_power_usage(const Building *b);   /* power lost per second */
void  game_lose(Building *b);

void room_set_lights(Room *r, bool on);
void room_set_lock(Room *r, bool locked);
void room_set_target_temp(Room *r, float temp);

void ui_handle_input(Building *b);
void ui_draw(const Building *b);

#endif /* BUILDING_H */
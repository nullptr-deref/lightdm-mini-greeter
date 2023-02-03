#ifndef SYSTEM_H
#define SYSTEM_H

enum {
    BAT_PRESENT = 0,
    NO_BAT
};

typedef struct BatteryInfo_ {
    char *status;
    float percentage;
} BatteryInfo;


BatteryInfo *initialize_battery_info(void);
void destroy_battery_info(BatteryInfo *bat_info);
int get_battery_info(BatteryInfo *bat_info);

#endif

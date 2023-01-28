#ifndef SYSTEM_H
#define SYSTEM_H

enum {
    BAT_PRESENT = 0,
    NO_BAT
};

typedef struct BatteryInfo_ {
    char *status;
    float percentage;
} Battery;

void initialize_battery_info(BatteryInfo *bat_info);
void destroy_battery_info(BatteryInfo *bat_info);
int get_battery_info(BatteryState *bat_info);
int battery_present();

#endif

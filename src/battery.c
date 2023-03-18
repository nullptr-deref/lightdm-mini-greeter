#include "battery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <sys/types.h>
#include <dirent.h>

#ifndef BAT_DIR
#define BAT_DIR "/sys/class/power_supply/BAT0/"
#endif
#ifndef BAT_STATUS
#define BAT_STATUS "/sys/class/power_supply/BAT0/status"
#endif
#ifndef BAT_CHARGE_NOW_FILE
#define BAT_CHARGE_NOW_FILE "/sys/class/power_supply/BAT0/charge_now"
#endif
#ifndef BAT_CHARGE_FULL_FILE
#define BAT_CHARGE_FULL_FILE "/sys/class/power_supply/BAT0/charge_full"
#endif

const unsigned int MAX_BAT_STATUS_LEN = 12; // Maximum string len + '\0'

static int battery_present(void);

/** Initialize battery info struct. */
BatteryInfo *initialize_battery_info() {
    BatteryInfo *bat_info = malloc(sizeof(BatteryInfo));
    bat_info->status = NULL;

    return bat_info;
}

/** Clear and destroy battery data. Pair of
 * initialize_battery_info(BatteryInfo *).
 */
void destroy_battery_info(BatteryInfo *bat_info) {
    if (bat_info->status != NULL)
        free(bat_info->status);

    free(bat_info);
}

/* Retrieves current battery state if present.
 * Returns 0 on success or -1 if battery is missing.
 */
int get_battery_info(BatteryInfo *bat_info) {
    if (battery_present() != BAT_PRESENT)
        return -1;

    int curr_charge, full_charge;
    FILE *charge_now_file = fopen(BAT_CHARGE_NOW_FILE, "r");
    fscanf(charge_now_file, "%d", &curr_charge);
    FILE *charge_full_file = fopen(BAT_CHARGE_FULL_FILE, "r");
    fscanf(charge_full_file, "%d", &full_charge);
    FILE *status_file = fopen(BAT_STATUS, "r");
    gchar curr_status[MAX_BAT_STATUS_LEN];
    fscanf(status_file, "%s", curr_status);
    fclose(charge_now_file);
    fclose(charge_full_file);
    fclose(status_file);

    bat_info->percentage = (float)curr_charge / full_charge * 100;
    bat_info->status = g_strdup(curr_status);

    return 0;
}

/* Determine whether battery is present.
 * Returns BAT_PRESENT if so or NO_BAT if no battery detected.
 */
int battery_present() {
    DIR *bat_dir = opendir(BAT_DIR);
    if (bat_dir == NULL) {
        return NO_BAT;
    }
    closedir(bat_dir);

    return BAT_PRESENT;
}

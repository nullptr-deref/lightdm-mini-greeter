#ifndef APP_H
#define APP_H

#include <lightdm.h>

#include "battery.h"
#include "config.h"
#include "focus_ring.h"
#include "ui.h"


typedef struct App_ {
    Config *config;
    LightDMGreeter *greeter;
    BatteryInfo *battery_info;
    UI *ui;
    FocusRing *session_ring;

    // Signal Handler ID for the `handle_password` callback
    gulong password_callback_id;
} App;


App *initialize_app(int argc, char **argv);
void destroy_app(App *app);

/* Config Member Accessors */
#define APP_LOGIN_USER(app)             (app)->config->login_user
#define APP_CONFIG(app)                 (app)->config

/* UI Member Accessors */
#define APP_BACKGROUND_WINDOWS(app)     (app)->ui->background_windows
#define APP_MONITOR_COUNT(app)            (app)->ui->monitor_count
#define APP_MAIN_WINDOW(app)              (app)->ui->main_window
#define APP_PASSWORD_INPUT(app)           (app)->ui->password_input
#define APP_FEEDBACK_LABEL(app)           (app)->ui->feedback_label
#define APP_TIME_LABEL(app)               (app)->ui->time_label
#define APP_BATTERY_INFO_WINDOW(app)      (app)->ui->battery_info_window
#define APP_BATTERY_PERCENTAGE_LABEL(app) (app)->ui->bat_percentage_label
#define APP_BATTERY_STATUS_LABEL(app)     (app)->ui->bat_status_label

#endif

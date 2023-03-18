/* Functions related to the GUI. */
#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <lightdm.h>

#include "callbacks.h"
#include "ui.h"
#include "utils.h"


static UI *new_ui(void);
static void setup_background_windows(Config *config, UI *ui);
static GtkWindow *new_background_window(GdkMonitor *monitor);
static void set_window_to_monitor_size(GdkMonitor *monitor, GtkWindow *window);
static void hide_mouse_cursor(GtkWidget *window, gpointer user_data);
static void move_mouse_to_background_window(void);
static void setup_main_window(Config *config, UI *ui);
static void place_main_window(GtkWidget *main_window, gpointer user_data);
static void create_and_attach_layout_container(UI *ui);
static void create_and_attach_sys_info_label(Config *config, UI *ui);
static void create_and_attach_password_field(Config *config, UI *ui);
static void create_and_attach_feedback_label(UI *ui);
static void create_and_attach_battery_info_label(Config *config, UI *ui);
static void attach_config_colors_to_screen(Config *config);
static void adjust_main_window_screen_space(Config *config, UI *ui);
static void setup_battery_info_window(Config *config, UI *ui);
static void place_battery_info_window(GtkWidget *bat_wnd, gpointer user_data);


const int BAT_POSITIONS_COUNT = 6;
const char *battery_window_positions[] = {
    "topleft",
    "top",
    "topright",
    "bottomleft",
    "bottom",
    "bottomright"
};
enum BatteryInfoWindowPositions {
    Topleft = 0,
    Top = 1,
    Topright = 2,
    Bottomleft = 3,
    Bottom = 4,
    Bottomright = 5
};

/* Initialize the Main Window & it's Children */
UI *initialize_ui(Config *config)
{
    UI *ui = new_ui();

    setup_background_windows(config, ui);
    move_mouse_to_background_window();
    setup_main_window(config, ui);
    adjust_main_window_screen_space(config, ui);
    create_and_attach_layout_container(ui);
    create_and_attach_sys_info_label(config, ui);
    create_and_attach_password_field(config, ui);
    create_and_attach_feedback_label(ui);
    attach_config_colors_to_screen(config);
    if (strcmp(config->battery_info_position, "main-window") != 0) {
        setup_battery_info_window(config, ui);
    }
    create_and_attach_battery_info_label(config, ui);

    return ui;
}


/* Create a new UI with all values initialized to NULL */
static UI *new_ui(void)
{
    UI *ui = malloc(sizeof(UI));
    if (ui == NULL) {
        g_error("Could not allocate memory for UI");
    }
    ui->background_windows = NULL;
    ui->monitor_count = 0;
    ui->main_window = NULL;
    ui->battery_info_window = NULL;
    ui->layout_container = NULL;
    ui->password_label = NULL;
    ui->password_input = NULL;
    ui->feedback_label = NULL;
    ui->bat_percentage_label = NULL;
    ui->bat_status_label = NULL;
    ui->battery_info_container = NULL;

    return ui;
}


/* Create a Background Window for Every Monitor */
static void setup_background_windows(Config *config, UI *ui)
{
    GdkDisplay *display = gdk_display_get_default();
    ui->monitor_count = gdk_display_get_n_monitors(display);
    ui->background_windows = malloc((uint) ui->monitor_count * sizeof (GtkWindow *));
    for (int m = 0; m < ui->monitor_count; m++) {
        GdkMonitor *monitor = gdk_display_get_monitor(display, m);
        if (monitor == NULL) {
            break;
        }

        GtkWindow *background_window = new_background_window(monitor);
        ui->background_windows[m] = background_window;

        gboolean show_background_image =
            (gdk_monitor_is_primary(monitor) || config->show_image_on_all_monitors) &&
            (strcmp(config->background_image, "\"\"") != 0);
        if (show_background_image) {
            GtkStyleContext *style_context =
                gtk_widget_get_style_context(GTK_WIDGET(background_window));
            gtk_style_context_add_class(style_context, "with-image");
        }
    }
}


/* Create & Configure a Background Window for a Monitor */
static GtkWindow *new_background_window(GdkMonitor *monitor)
{
    GtkWindow *background_window = GTK_WINDOW(gtk_window_new(
        GTK_WINDOW_TOPLEVEL));
    gtk_window_set_type_hint(background_window, GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_keep_below(background_window, TRUE);
    gtk_widget_set_name(GTK_WIDGET(background_window), "background");

    // Set Window Size to Monitor Size
    set_window_to_monitor_size(monitor, background_window);

    g_signal_connect(background_window, "realize", G_CALLBACK(hide_mouse_cursor),
                     NULL);
    // TODO: is this needed?
    g_signal_connect(background_window, "destroy", G_CALLBACK(gtk_main_quit),
                     NULL);

    return background_window;
}


/* Set the Window's Minimum Size to the Default Screen's Size */
static void set_window_to_monitor_size(GdkMonitor *monitor, GtkWindow *window)
{
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    gtk_widget_set_size_request(
        GTK_WIDGET(window),
        geometry.width,
        geometry.height
    );
    gtk_window_move(window, geometry.x, geometry.y);
    gtk_window_set_resizable(window, FALSE);
}


/* Hide the mouse cursor when it is hovered over the given widget.
 *
 * Note: This has no effect when used with a GtkEntry widget.
 */
static void hide_mouse_cursor(GtkWidget *widget, gpointer user_data)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkCursor *blank_cursor = gdk_cursor_new_for_display(display, GDK_BLANK_CURSOR);
    GdkWindow *window = gtk_widget_get_window(widget);
    if (window != NULL) {
        gdk_window_set_cursor(window, blank_cursor);
    }
}


/* Move the mouse cursor to the upper-left corner of the primary screen.
 *
 * This is necessary for hiding the mouse cursor because we cannot hide the
 * mouse cursor when it is hovered over the GtkEntry password input. Instead,
 * we hide the cursor when it is over the background windows and then move the
 * mouse to the corner of the screen where it should hover over the background
 * window or main window instead.
 */
static void move_mouse_to_background_window(void)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkDevice *mouse = gdk_seat_get_pointer(gdk_display_get_default_seat(display));
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gdk_device_warp(mouse, screen, 0, 0);
}


/* Create & Configure the Main Window */
static void setup_main_window(Config *config, UI *ui)
{
    GtkWindow *main_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

    gtk_container_set_border_width(GTK_CONTAINER(main_window), config->layout_spacing);
    gtk_widget_set_name(GTK_WIDGET(main_window), "main");

    g_signal_connect(main_window, "show", G_CALLBACK(place_main_window), NULL);
    g_signal_connect(main_window, "realize", G_CALLBACK(hide_mouse_cursor), NULL);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    ui->main_window = main_window;
}


/* Move the Main Window to the Center of the Primary Monitor
 *
 * This is done after the main window is shown(via the "show" signal) so that
 * the width of the window is properly calculated. Otherwise the returned size
 * will not include the size of the password label text.
 */
static void place_main_window(GtkWidget *main_window, gpointer user_data)
{
    // Get the Geometry of the Primary Monitor
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle primary_monitor_geometry;
    gdk_monitor_get_geometry(primary_monitor, &primary_monitor_geometry);

    // Get the Geometry of the Window
    gint window_width, window_height;
    gtk_window_get_size(GTK_WINDOW(main_window), &window_width, &window_height);

    gtk_window_move(
        GTK_WINDOW(main_window),
        primary_monitor_geometry.x + primary_monitor_geometry.width / 2 - window_width / 2,
        primary_monitor_geometry.y + primary_monitor_geometry.height / 2 - window_height / 2);
}


/* Set main window size to cover specified screen space */
static void adjust_main_window_screen_space(Config *config, UI *ui)
{
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle primary_monitor_geometry;
    gdk_monitor_get_geometry(primary_monitor, &primary_monitor_geometry);

    const float EPS = 10e-6;
    gint initial_width, initial_height;
    gtk_widget_get_preferred_width(GTK_WIDGET(ui->main_window), &initial_width, NULL);
    gtk_widget_get_preferred_height(GTK_WIDGET(ui->main_window), &initial_height, NULL);
    const gfloat initial_screen_space_h = (gfloat)initial_width / primary_monitor_geometry.width;
    const gfloat initial_screen_space_v = (gfloat)initial_height / primary_monitor_geometry.height;
    gint width = initial_width,
         height = initial_height;
    if (fabs(config->screen_space_h - initial_screen_space_h) > EPS
            && config->screen_space_h > initial_screen_space_h) {
        width = (gint)(config->screen_space_h * primary_monitor_geometry.width);
    }
    if (fabs(config->screen_space_v - initial_screen_space_v) > EPS
            && config->screen_space_v > initial_screen_space_v) {
        height = (gint)(config->screen_space_v * primary_monitor_geometry.height);
    }
    gtk_window_set_default_size(GTK_WINDOW(ui->main_window), width, height);
}


/* Add a Layout Container for All Displayed Widgets */
static void create_and_attach_layout_container(UI *ui)
{
    ui->layout_container = GTK_GRID(gtk_grid_new());
    gtk_grid_set_column_spacing(ui->layout_container, 5);
    gtk_grid_set_row_spacing(ui->layout_container, 5);

    gtk_container_add(GTK_CONTAINER(ui->main_window),
                      GTK_WIDGET(ui->layout_container));
}

/* Create a container for the system information & current time.
 *
 * Set the system information text by querying LightDM & the Config, but leave
 * the time blank & let the timer update it.
 */
static void create_and_attach_sys_info_label(Config *config, UI *ui)
{
    if (!config->show_sys_info) {
        return;
    }
    // container for system info & time
    ui->info_container = GTK_GRID(gtk_grid_new());
    gtk_grid_set_column_spacing(ui->info_container, 0);
    gtk_grid_set_row_spacing(ui->info_container, 5);
    gtk_widget_set_name(GTK_WIDGET(ui->info_container), "info");

    // system info: <user>@<hostname>
    const gchar *hostname = lightdm_get_hostname();
    gchar *output_string;
    int output_string_length = asprintf(&output_string, "%s@%s",
                                        config->login_user, hostname);
    if (output_string_length >= 0) {
        ui->sys_info_label = gtk_label_new(output_string);
    } else {
        g_warning("Could not allocate memory for system info string.");
        ui->sys_info_label = gtk_label_new("");
    }
    gtk_label_set_xalign(GTK_LABEL(ui->sys_info_label), 0.0f);
    gtk_widget_set_name(GTK_WIDGET(ui->sys_info_label), "sys-info");

    // time: filled out by callback
    ui->time_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ui->time_label), 1.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(ui->time_label), TRUE);
    gtk_widget_set_name(GTK_WIDGET(ui->time_label), "time-info");

    // attach labels to info container, attach info container to layout.
    gtk_grid_attach(
        ui->info_container, GTK_WIDGET(ui->sys_info_label), 0, 0, 1, 1);
    gtk_grid_attach(
        ui->info_container, GTK_WIDGET(ui->time_label), 1, 0, 1, 1);
    gtk_grid_attach(
        ui->layout_container, GTK_WIDGET(ui->info_container), 0, 0, 2, 1);
}


/* Add a label & entry field for the user's password.
 *
 * If the `show_password_label` member of `config` is FALSE,
 * `ui->password_label` is left as NULL.
 */
static void create_and_attach_password_field(Config *config, UI *ui)
{
    ui->password_input = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ui->password_input), FALSE);
    if (config->password_char != NULL) {
        gtk_entry_set_invisible_char(GTK_ENTRY(ui->password_input), *config->password_char);
    }
    gtk_entry_set_alignment(GTK_ENTRY(ui->password_input),
                            config->password_alignment);
    // TODO: The width is usually a little shorter than we specify. Is there a
    // way to force this exact character width?
    // Maybe use 2 GtkBoxes instead of a GtkGrid?
    gtk_entry_set_width_chars(GTK_ENTRY(ui->password_input),
                              config->password_input_width);
    gtk_widget_set_name(GTK_WIDGET(ui->password_input), "password");
    const gint top = config->show_sys_info ? 1 : 0;
    gtk_grid_attach(ui->layout_container, ui->password_input, 1, top, 1, 1);

    if (config->show_password_label) {
        ui->password_label = gtk_label_new(config->password_label_text);
        gtk_label_set_justify(GTK_LABEL(ui->password_label), GTK_JUSTIFY_RIGHT);
        gtk_grid_attach_next_to(ui->layout_container, ui->password_label,
                                ui->password_input, GTK_POS_LEFT, 1, 1);
    }
}

void setup_battery_info_window(Config *config, UI *ui) {
    GtkWindow *battery_info_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

    gtk_widget_set_name(GTK_WIDGET(battery_info_window), "battery");

    g_signal_connect(battery_info_window, "show", G_CALLBACK(place_battery_info_window), config);
    g_signal_connect(battery_info_window, "realize", G_CALLBACK(hide_mouse_cursor), NULL);
    g_signal_connect(battery_info_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    ui->battery_info_window = battery_info_window;
}


void place_battery_info_window(GtkWidget *bat_wnd, gpointer user_data) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle primary_monitor_geometry;
    gdk_monitor_get_geometry(primary_monitor, &primary_monitor_geometry);

    gint window_width, window_height;
    gtk_window_get_size(GTK_WINDOW(bat_wnd), &window_width, &window_height);

    int position_selector = Topright;
    for (int i = 0; i < BAT_POSITIONS_COUNT; i++) {
        if (strcmp(battery_window_positions[i], ((Config*)user_data)->battery_info_position) == 0) {
            position_selector = i;
            break;
        }
    }
    const int bat_window_margin = 10; // margin in pixels from screen border
    switch (position_selector) {
        case Topleft:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + bat_window_margin,
                primary_monitor_geometry.y + bat_window_margin);
            break;
        case Top:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + primary_monitor_geometry.width / 2 - window_width / 2,
                primary_monitor_geometry.y + bat_window_margin);
            break;
        case Topright:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + primary_monitor_geometry.width - window_width - bat_window_margin,
                primary_monitor_geometry.y + bat_window_margin);
            break;
        case Bottomleft:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + bat_window_margin,
                primary_monitor_geometry.y + primary_monitor_geometry.height - window_height - bat_window_margin);
            break;
        case Bottom:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + primary_monitor_geometry.width / 2 + window_width / 2,
                primary_monitor_geometry.y + primary_monitor_geometry.height - window_height - bat_window_margin);
            break;
        case Bottomright:
            gtk_window_move(
                GTK_WINDOW(bat_wnd),
                primary_monitor_geometry.x + primary_monitor_geometry.width - window_width - bat_window_margin,
                primary_monitor_geometry.y + primary_monitor_geometry.height - window_height - bat_window_margin);
    }
}


/* Add a label for feedback to the user */
static void create_and_attach_feedback_label(UI *ui)
{
    ui->feedback_label = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(ui->feedback_label), GTK_JUSTIFY_CENTER);
    gtk_widget_set_no_show_all(ui->feedback_label, TRUE);
    gtk_widget_set_name(GTK_WIDGET(ui->feedback_label), "error");

    GtkWidget *attachment_point;
    gint width;
    if (ui->password_label == NULL) {
        attachment_point = ui->password_input;
        width = 1;
    } else {
        attachment_point = ui->password_label;
        width = 2;
    }

    gtk_grid_attach_next_to(ui->layout_container, ui->feedback_label,
                            attachment_point, GTK_POS_BOTTOM, width, 1);
}

static void create_and_attach_battery_info_label(Config *config, UI *ui)
{
    ui->battery_info_container = GTK_GRID(gtk_grid_new());
    gtk_grid_set_column_spacing(ui->battery_info_container, 0);
    gtk_grid_set_row_spacing(ui->battery_info_container, 5);
    gtk_widget_set_name(GTK_WIDGET(ui->battery_info_container), "battery-info");

    ui->bat_status_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ui->bat_status_label), 1.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(ui->bat_status_label), TRUE);
    gtk_widget_set_name(GTK_WIDGET(ui->bat_status_label), "battery-status");

    ui->bat_percentage_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ui->bat_percentage_label), 0.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(ui->bat_percentage_label), TRUE);
    gtk_widget_set_name(GTK_WIDGET(ui->bat_percentage_label), "battery-percentage");

    gtk_grid_attach(
        ui->battery_info_container, GTK_WIDGET(ui->bat_status_label), 0, 0, 1, 1);
    gtk_grid_attach(
        ui->battery_info_container, GTK_WIDGET(ui->bat_percentage_label), 1, 0, 1, 1);
    if (config->show_battery_info) {
        gtk_container_add(GTK_CONTAINER(ui->battery_info_window),
                          GTK_WIDGET(ui->battery_info_container));
    }
    else {
        // TODO: handle placement in the main window
    }
}

/* Attach a style provider to the screen, using color options from config */
static void attach_config_colors_to_screen(Config *config)
{
    GtkCssProvider* provider = gtk_css_provider_new();

    GdkRGBA *caret_color;
    if (config->show_input_cursor) {
        caret_color = config->password_color;
    } else {
        caret_color = config->password_background_color;
    }

    char *css;
    int css_string_length = asprintf(&css,
        "* {\n"
            "font-family: %s;\n"
            "font-size: %s;\n"
            "font-weight: %s;\n"
            "font-style: %s;\n"
        "}\n"
        "label {\n"
            "color: %s;\n"
        "}\n"
        "label#error {\n"
            "color: %s;\n"
        "}\n"
        "#background {\n"
            "background-color: %s;\n"
        "}\n"
        "#background.with-image {\n"
            "background-image: image(url(%s), %s);\n"
            "background-repeat: no-repeat;\n"
            "background-size: %s;\n"
            "background-position: center;\n"
        "}\n"
        "#main, #password {\n"
            "border-width: %s;\n"
            "border-color: %s;\n"
            "border-style: solid;\n"
        "}\n"
        "#main, #battery {\n"
            "background-color: %s;\n"
        "}\n"
        "#password {\n"
            "color: %s;\n"
            "caret-color: %s;\n"
            "background-color: %s;\n"
            "border-width: %s;\n"
            "border-color: %s;\n"
            "border-radius: %s;\n"
            "background-image: none;\n"
            "box-shadow: none;\n"
            "border-image-width: 0;\n"
        "}\n"
        "#info {\n"
            "margin: %s;\n"
        "}\n"
        "#info label {\n"
            "font-family: %s;\n"
            "font-size: %s;\n"
            "color: %s;\n"
        "}\n"

        // *
        , config->font
        , config->font_size
        , config->font_weight
        , config->font_style
        // label
        , gdk_rgba_to_string(config->text_color)
        // label#error
        , gdk_rgba_to_string(config->error_color)
        // #background
        , gdk_rgba_to_string(config->background_color)
        // #background.image-background
        , config->background_image
        , gdk_rgba_to_string(config->background_color)
        , config->background_image_size
        // #main, #password
        , config->border_width
        , gdk_rgba_to_string(config->border_color)
        // #main
        , gdk_rgba_to_string(config->window_color)
        // #password
        , gdk_rgba_to_string(config->password_color)
        , gdk_rgba_to_string(caret_color)
        , gdk_rgba_to_string(config->password_background_color)
        , config->password_border_width
        , gdk_rgba_to_string(config->password_border_color)
        , config->password_border_radius
        // #info
        , config->sys_info_margin
        // #info label
        , config->sys_info_font
        , config->sys_info_font_size
        , gdk_rgba_to_string(config->sys_info_color)
    );

    if (css_string_length >= 0) {
        gtk_css_provider_load_from_data(provider, css, -1, NULL);

        GdkScreen *screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    }


    g_object_unref(provider);
}

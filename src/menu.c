/*
 * Morpheuz Sleep Monitor
 *
 * Copyright (c) 2013-2015 James Fowler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "pebble.h"
#include "morpheuz.h"
#include "language.h"
#include "analogue.h"

#define NUM_MENU_SECTIONS 1
#define NUM_MENU_ICONS 2

static Window *window;
static MenuLayer *menu_layer;
static GBitmap *menu_icons[NUM_MENU_ICONS];

static uint8_t ignore_state = 0;
static uint8_t weekend_state = 0;
static uint8_t inverse_state = 0;
static uint8_t analogue_state = 0;
static uint8_t power_nap_state = 0;
static uint8_t auto_reset_state = 0;
static uint8_t original_auto_reset_state = 0;
static bool alarm_on = false;
static char menu_text[15];
static int16_t selected_row;
static bool menu_live = false;
static bool menu_act;

extern char date_text[16];
static char failure_text[] = NO_FAILURE;

#ifndef PBL_COLOR
static void menu_invert();
#endif

static void menu_analogue();
static void menu_resend();
static void hide_menu(void *data);

// Invoke a menu item
typedef void (*MorphMenuAction)(void);

// Define a menu item
typedef struct {
  char *title;
  char *subtitle;
  uint8_t *state;
  MorphMenuAction action;
} MenuDef;

// Define the menu
#ifdef PBL_COLOR
  #define OPT_WAKEUP 7
  #define OPT_PRESETS 5
static MenuDef menu_def[] = { {MENU_SNOOZE, MENU_SNOOZE_DES, NULL, snooze_alarm},
  { MENU_CANCEL, MENU_CANCEL_DES, NULL, cancel_alarm},
  { MENU_IGNORE, MENU_IGNORE_DES, &ignore_state, set_ignore_on_current_time_segment},
  { MENU_RESET, MENU_RESET_DES, NULL, reset_sleep_period},
  { MENU_SMART_ALARM, MENU_SMART_ALARM_DES, NULL, show_set_alarm},
  { MENU_PRESET, MENU_PRESET_DES, NULL, show_preset_menu},
  { MENU_WEEKEND, MENU_WEEKEND_DES, &weekend_state, toggle_weekend_mode},
  { MENU_AUTO_RESET, MENU_AUTO_RESET_DES_OFF, &auto_reset_state, wakeup_toggle},
  { MENU_POWER_NAP, MENU_POWER_NAP_DES, &power_nap_state, toggle_power_nap},
  { MENU_ANALOGUE, MENU_ANALOGUE_DES, &analogue_state, menu_analogue},
  { MENU_RESEND, MENU_RESEND_DES, NULL, menu_resend},
  { MENU_QUIT, MENU_QUIT_DES, NULL, close_morpheuz}};
#else
    #define OPT_WAKEUP 6
static MenuDef menu_def[] = { { MENU_SNOOZE, MENU_SNOOZE_DES, NULL, snooze_alarm }, 
  { MENU_CANCEL, MENU_CANCEL_DES, NULL, cancel_alarm }, 
  { MENU_IGNORE, MENU_IGNORE_DES, &ignore_state, set_ignore_on_current_time_segment }, 
  { MENU_RESET, MENU_RESET_DES, NULL, reset_sleep_period }, 
  { MENU_SMART_ALARM, MENU_SMART_ALARM_DES, NULL, show_set_alarm }, 
  { MENU_WEEKEND, MENU_WEEKEND_DES, &weekend_state, toggle_weekend_mode }, 
  { MENU_AUTO_RESET, MENU_AUTO_RESET_DES_OFF, &auto_reset_state, wakeup_toggle }, 
  { MENU_POWER_NAP, MENU_POWER_NAP_DES, &power_nap_state, toggle_power_nap }, 
  { MENU_INVERSE, MENU_INVERSE_DES, &inverse_state, menu_invert }, 
  { MENU_ANALOGUE, MENU_ANALOGUE_DES, &analogue_state, menu_analogue }, 
  { MENU_RESEND, MENU_RESEND_DES, NULL, menu_resend }, 
  { MENU_QUIT, MENU_QUIT_DES, NULL, close_morpheuz } };
#endif

/*
 * Record failures
 */
void mark_failure(FailureNote fn) {
    failure_text[fn] = '0' + fn;
}

/*
 * A callback is used to specify the amount of sections of menu items
 * With this, you can dynamically add and remove sections
 */
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

/*
 * Each section has a number of items;  we use a callback to specify this
 * You can also dynamically add and remove items using this
 */
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return ARRAY_LENGTH(menu_def) - (alarm_on ? 0 : 2);
}

/*
 * A callback is used to specify the height of the section header
 */
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

/*
 * Here we draw what each header is
 */
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  graphics_context_set_text_color(ctx, MENU_HEAD_COLOR);
  char *text = date_text;
  if (strncmp(failure_text, NO_FAILURE, sizeof(failure_text)) != 0) {
    text = failure_text;
  }
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(0, -2, 144, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

/*
 * This is the menu item draw callback where you specify what each item should look like
 */
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {

#ifdef PBL_COLOR
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
#endif

  // Pick up names from the array except for the one instance where we fiddle with it
  int16_t index = alarm_on ? cell_index->row : cell_index->row + 2;
  const char *subtitle = menu_def[index].subtitle;
  GBitmap *icon = menu_def[index].state == NULL ? NULL : menu_icons[*(menu_def[index].state)];

  if (index == OPT_WAKEUP && auto_reset_state == 1 && original_auto_reset_state == 1) {
    snprintf(menu_text, sizeof(menu_text), MENU_AUTO_RESET_DES_ON, twenty_four_to_twelve(get_config_data()->autohr), get_config_data()->automin);
    subtitle = menu_text;
  }

  menu_cell_basic_draw(ctx, cell_layer, menu_def[index].title, subtitle, icon);

}

/*
 * Do menu action after shutting the menu and allowing time for the animations to complete
 */
static void do_menu_action(void *data) {
  menu_def[selected_row].action();
  menu_act = false;
}

#ifndef PBL_COLOR
/*
 * Invert option
 */
static void menu_invert() {
  get_config_data()->invert = !get_config_data()->invert;
  trigger_config_save();
  invert_screen();
}
#endif

/*
 * Analogue option
 */
static void menu_analogue() {
  get_config_data()->analogue = !get_config_data()->analogue;
  trigger_config_save();
  analogue_visible(get_config_data()->analogue, false);
}

/*
 * Resend option
 */
static void menu_resend() {
  resend_all_data(false);
}

/*
 * Here we capture when a user selects a menu item
 */
static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (menu_act) {
    return;
  }
  menu_act = true;
  // Use the row to specify which item will receive the select action
  selected_row = alarm_on ? cell_index->row : cell_index->row + 2;
  if (menu_def[selected_row].state != NULL) {
    *(menu_def[selected_row].state) = !(*(menu_def[selected_row].state));
    layer_mark_dirty(menu_layer_get_layer_jf(menu_layer));
  }
  #ifndef PBL_COLOR
    app_timer_register(MENU_ACTION_HIDE_MS, hide_menu, NULL);
  #else
    if (selected_row != OPT_PRESETS) {
      app_timer_register(MENU_ACTION_HIDE_MS, hide_menu, NULL);
    } 
  #endif
  app_timer_register(MENU_ACTION_MS, do_menu_action, NULL);
}

/*
 * This initializes the menu upon window load
 */
void window_load(Window *window) {
  menu_icons[0] = gbitmap_create_with_resource(RESOURCE_ID_MENU_NO);
  menu_icons[1] = gbitmap_create_with_resource(RESOURCE_ID_MENU_YES);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  menu_layer = menu_layer_create(bounds);

  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks ) { .get_num_sections = menu_get_num_sections_callback, .get_num_rows = menu_get_num_rows_callback, .get_header_height = menu_get_header_height_callback, .draw_header = menu_draw_header_callback, .draw_row = menu_draw_row_callback, .select_click = menu_select_callback, });

  menu_layer_set_click_config_onto_window(menu_layer, window);

  layer_add_child(window_layer, menu_layer_get_layer_jf(menu_layer));

  #ifdef PBL_COLOR
    menu_layer_set_normal_colors(menu_layer, MENU_BACKGROUND_COLOR, MENU_TEXT_COLOR);
    menu_layer_set_highlight_colors(menu_layer, MENU_HIGHLIGHT_BACKGROUND_COLOR, MENU_TEXT_COLOR);
  #endif  

}

/*
 * Unload the menu window
 */
static void window_unload(Window *window) {
  menu_layer_destroy(menu_layer);
  gbitmap_destroy(menu_icons[0]);
  gbitmap_destroy(menu_icons[1]);
  menu_live = false;
}

/*
 * Show the menu
 */
void show_menu() {
  if (menu_live) {
    return;
  }
  menu_live = true;
  menu_act = false;
  ignore_state = get_icon(IS_IGNORE);
  weekend_state = get_config_data()->weekend_until != 0;
  inverse_state = get_config_data()->invert;
  analogue_state = get_config_data()->analogue;
  power_nap_state = is_doing_powernap();
  auto_reset_state = get_config_data()->auto_reset;
  original_auto_reset_state = auto_reset_state;
  alarm_on = get_icon(IS_ALARM_RING);
  window = window_create();
  // Setup the window handlers
  window_set_window_handlers(window, (WindowHandlers ) { .load = window_load, .unload = window_unload, });
  window_stack_push(window, true /* Animated */);
}

/*
 * Hide the menu (destroy)
 */
static void hide_menu(void *data) {
  window_stack_remove(window, true);
  window_destroy(window);
}


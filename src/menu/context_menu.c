/*
 * iOS Style Context Menu Implementation in C (using LVGL)
 * 
 * This code demonstrates how to build the UI from the image using LVGL,
 * a popular C-based graphics library for embedded systems.
 */

#include "lvgl/lvgl.h"

// Custom styles
static lv_style_t style_menu;
static lv_style_t style_item;
static lv_style_t style_divider;

void create_context_menu(lv_obj_t * parent) {
    // 1. Create the main menu container (Glassmorphism effect)
    lv_style_init(&style_menu);
    lv_style_set_bg_color(&style_menu, lv_color_white());
    lv_style_set_bg_opa(&style_menu, LV_OPA_80);
    lv_style_set_radius(&style_menu, 14);
    lv_style_set_shadow_width(&style_menu, 50);
    lv_style_set_shadow_color(&style_menu, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_opa(&style_menu, LV_OPA_30);
    lv_style_set_border_width(&style_menu, 1);
    lv_style_set_border_color(&style_menu, lv_color_hex(0xFFFFFF));
    lv_style_set_border_opa(&style_menu, LV_OPA_40);

    lv_obj_t * menu = lv_obj_create(parent);
    lv_obj_set_size(menu, 280, LV_SIZE_CONTENT);
    lv_obj_add_style(menu, &style_menu, 0);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(menu, 0, 0);
    lv_obj_set_style_pad_row(menu, 0, 0);
    lv_obj_center(menu);

    // 2. Helper function to add items
    auto add_item = [&](const char * label, const char * symbol, bool has_divider, bool has_switch) {
        lv_obj_t * item = lv_obj_create(menu);
        lv_obj_set_size(item, lv_pct(100), 44);
        lv_obj_set_style_bg_opa(item, 0, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        // Icon
        lv_obj_t * icon = lv_label_create(item);
        lv_label_set_text(icon, symbol);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 15, 0);

        // Label
        lv_obj_t * lbl = lv_label_create(item);
        lv_label_set_text(lbl, label);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 45, 0);

        if(has_switch) {
            lv_obj_t * sw = lv_switch_create(item);
            lv_obj_set_size(sw, 50, 25);
            lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -15, 0);
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }

        if(has_divider) {
            lv_obj_t * div = lv_obj_create(menu);
            lv_obj_set_size(div, lv_pct(90), 1);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(div, LV_OPA_10, 0);
            lv_obj_set_style_border_width(div, 0, 0);
            lv_obj_align(div, LV_ALIGN_CENTER, 0, 0);
        }
    };

    // 3. Add items matching the image
    add_item("Start/Stop", LV_SYMBOL_PLAY, true, false);
    add_item("Reset", LV_SYMBOL_REFRESH, true, false);
    add_item("Time Settings", LV_SYMBOL_SETTINGS, true, false);
    add_item("Appearance", LV_SYMBOL_EDIT, true, false);
    add_item("Audio", LV_SYMBOL_VOLUME_MAX, true, false);
    add_item("Presets", LV_SYMBOL_LIST, true, false);
    add_item("Overtime Count", "", true, true);
    add_item("Language", LV_SYMBOL_DIRECTORY, true, false);
    add_item("About", LV_SYMBOL_INFO, true, false);
    add_item("Exit", LV_SYMBOL_POWER, false, false);
}

/* 
 * Win32 API Version (Conceptual)
 * 
 * HMENU hMenu = CreatePopupMenu();
 * AppendMenu(hMenu, MF_STRING, ID_START_STOP, L"Start/Stop");
 * AppendMenu(hMenu, MF_STRING, ID_RESET, L"Reset");
 * ...
 * // Note: Win32 standard menus don't support toggles/icons easily without "Owner Draw"
 * // For the exact look in the image, you would use a custom window with GDI+ or Direct2D.
 */

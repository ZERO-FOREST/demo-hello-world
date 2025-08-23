#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"

// This file now only contains prototypes for common UI creation functions.
// All setting-related logic is handled by the settings_manager.

/**
 * @brief Creates a settings pop-up window.
 *        This function is designed to be called from an event callback, e.g., a button click.
 * @param parent The parent object for the pop-up.
 */
void ui_create_settings_popup(lv_obj_t* parent);


#endif // UI_COMMON_H

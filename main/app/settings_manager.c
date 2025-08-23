#include "settings_manager.h"

// The global variable to hold the current transfer mode setting.
// Default to TCP mode.
static image_transfer_mode_t g_transfer_mode = IMAGE_TRANSFER_MODE_TCP;

void settings_manager_init(void) {
    // In the future, this function can be used to load the saved settings from NVS.
    // For now, we just stick to the default value.
}

void settings_set_transfer_mode(image_transfer_mode_t mode) {
    if (mode == IMAGE_TRANSFER_MODE_TCP || mode == IMAGE_TRANSFER_MODE_UDP) {
        g_transfer_mode = mode;
        // In the future, this function can save the setting to NVS.
    }
}

image_transfer_mode_t settings_get_transfer_mode(void) {
    return g_transfer_mode;
}

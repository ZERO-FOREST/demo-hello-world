#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

// Enum for image transfer mode
typedef enum {
    IMAGE_TRANSFER_MODE_TCP,
    IMAGE_TRANSFER_MODE_UDP,
} image_transfer_mode_t;

/**
 * @brief Initializes the settings manager.
 *        (Could be used to load settings from NVS in the future)
 */
void settings_manager_init(void);

/**
 * @brief Sets the current image transfer mode.
 * 
 * @param mode The new mode to set.
 */
void settings_set_transfer_mode(image_transfer_mode_t mode);

/**
 * @brief Gets the current image transfer mode.
 * 
 * @return The currently configured image_transfer_mode_t.
 */
image_transfer_mode_t settings_get_transfer_mode(void);


#ifdef __cplusplus
}
#endif

#endif // SETTINGS_MANAGER_H

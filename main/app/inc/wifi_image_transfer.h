#ifndef WIFI_IMAGE_TRANSFER_H
#define WIFI_IMAGE_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the Wi-Fi TCP image transfer server.
 *
 * @param port The port number to listen on.
 * @return true if the server started successfully, false otherwise.
 */
bool wifi_image_transfer_start(uint16_t port);

/**
 * @brief Stop the Wi-Fi TCP image transfer server.
 */
void wifi_image_transfer_stop(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_IMAGE_TRANSFER_H
#ifndef UI_P2P_UDP_TRANSFER_H
#define UI_P2P_UDP_TRANSFER_H

#include "lvgl.h"
#include "p2p_udp_image_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建P2P UDP图传界面
 * @param parent 父对象，通常是 lv_scr_act()
 */
void ui_p2p_udp_transfer_create(lv_obj_t* parent);

/**
 * @brief 销毁P2P UDP图传界面
 */
void ui_p2p_udp_transfer_destroy(void);

/**
 * @brief 设置接收到的图像数据（用于显示）
 * @param img_buf 图像数据缓冲区
 * @param width 图像宽度
 * @param height 图像高度
 * @param format 像素格式
 */
void ui_p2p_udp_transfer_set_image_data(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);

/**
 * @brief 更新连接状态显示
 * @param state 连接状态
 * @param info 状态信息
 */
void ui_p2p_udp_transfer_update_status(p2p_connection_state_t state, const char* info);

/**
 * @brief 更新统计信息显示
 * @param tx_packets 发送包数
 * @param rx_packets 接收包数
 * @param lost_packets 丢失包数
 * @param retx_packets 重传包数
 */
void ui_p2p_udp_transfer_update_stats(uint32_t tx_packets, uint32_t rx_packets, uint32_t lost_packets,
                                      uint32_t retx_packets);

#ifdef __cplusplus
}
#endif

#endif // UI_P2P_UDP_TRANSFER_H

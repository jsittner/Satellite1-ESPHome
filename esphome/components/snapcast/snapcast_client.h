#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esp_transport.h"
#include "esphome/components/audio/timed_ring_buffer.h"

#include "messages.h"

namespace esphome {
namespace snapcast {


class SnapcastStream {
public:
    esp_err_t connect();
    esp_err_t disconnect();
    bool receive_next_message();
    esp_err_t read_next_data_chunk(uint8_t *data, size_t &size, uint32_t timeout_ms);
    esp_err_t read_next_data_chunk(std::shared_ptr<esphome::TimedRingBuffer> ring_buffer, uint32_t timeout_ms);
    bool is_connected() { return this->transport_ != NULL; }


protected:
    void send_message_(SnapcastMessage &msg);
    void send_queueed_messages_();
    void send_hello_();
    void send_time_sync_();

    void on_server_settings_msg(ServerSettingsMessage *msg);
    void on_time_msg(MessageHeader msg, tv time);

    uint32_t last_time_sync_{0};
    esp_transport_handle_t transport_{NULL};
    tv est_time_diff_{0, 0};
};


}
}
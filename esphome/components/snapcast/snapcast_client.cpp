#include "snapcast_client.h"
#include "messages.h"

#include "esphome/core/log.h"

#include "esp_transport.h"
#include "esp_transport_tcp.h"

#include "esp_mac.h"

namespace esphome {
namespace snapcast {

static const char *const TAG = "snapcast_client";

static constexpr auto TIME_SYNC_INTERVAL = 10000; //ms

#define TARGET_ADDR "192.168.178.57"
#define TARGET_PORT 1704

static uint8_t tx_buffer[1024];
static uint8_t rx_buffer[4096];
static uint32_t rx_bufer_length = 0; 

static tv_t last_server_time;
static tv_t last_reported_time_delta;

static tv_t get_est_server_time()
{
    const tv_t now = tv_t::now();
    return (last_reported_time_delta + last_server_time + now) / 2;
}



esp_err_t SnapcastStream::connect(){
    char host_ip[] = TARGET_ADDR;
    
    if( this->transport_ == NULL ){
        this->transport_ = esp_transport_tcp_init();
        if (this->transport_ == NULL) {
            ESP_LOGE(TAG, "Error occurred during esp_transport_init()");
            return ESP_FAIL;
        }
    }
    
    error_t err = esp_transport_connect(this->transport_, TARGET_ADDR, TARGET_PORT, -1);
    if (err != 0) {
        ESP_LOGE(TAG, "Client unable to connect: errno %d", errno);
        return ESP_FAIL;
    }
    this->send_hello_();
    return ESP_OK;
}

esp_err_t SnapcastStream::disconnect(){
    if( this->transport_ != NULL ){
        esp_transport_close(this->transport_);
        esp_transport_destroy(this->transport_);
        this->transport_ = NULL;
    }
    return ESP_OK;
}





void SnapcastStream::send_message_(SnapcastMessage &msg){
    assert( msg.getMessageSize() <= sizeof(tx_buffer));
    msg.set_send_time();
    msg.toBytes(tx_buffer);
    int bytes_written = esp_transport_write( this->transport_, (char*) tx_buffer, msg.getMessageSize(), 0);
    printf("Sent:\n");
    msg.print();
    if (bytes_written < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: esp_transport_write() returned %d, errno %d", bytes_written, errno);
    }
}

void SnapcastStream::send_hello_(){
    unsigned char mac_base[6] = {0};
    esp_efuse_mac_get_default(mac_base);
    esp_read_mac(mac_base, ESP_MAC_WIFI_STA);
    unsigned char mac_local_base[6] = {0};
    unsigned char mac_uni_base[6] = {0};
    esp_derive_local_mac(mac_local_base, mac_uni_base);
    HelloMessage hello_msg;
    this->send_message_(hello_msg);
}


void SnapcastStream::send_time_sync_(){
    TimeMessage time_sync_msg; 
    this->send_message_(time_sync_msg);
    this->last_time_sync_ = millis();
}

// Compare function for qsort
static int compare_int32(const void* a, const void* b) {
    int32_t val_a = *(const int32_t*)a;
    int32_t val_b = *(const int32_t*)b;

    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

void SnapcastStream::on_time_msg(MessageHeader msg, tv_t latency_c2s){
    //latency_c2s = t_server-recv - t_client-sent + t_network-latency
    //latency_s2c = t_client-recv - t_server-sent + t_network_latency
    //time diff between server and client as (latency_c2s - latency_s2c) / 2
    tv_t latency_s2c = tv_t::now() - msg.sent;
    this->est_time_diff_ = (latency_c2s - latency_s2c) / 2;
    //printf("Snapcast: Estimated time diff: %d.%06d sec\n", this->est_time_diff_.sec, this->est_time_diff_.usec);
    TimeStats &stats = this->time_stats_;
    if( this->time_stats_.count < MAX_TIMES){
        this->time_stats_.count++;
    }
    this->time_stats_.times[this->time_stats_.next_insert] = ((latency_c2s - latency_s2c) / 2).to_millis();
    this->time_stats_.next_insert++;
    this->time_stats_.next_insert %= MAX_TIMES;
    
    int32_t sorted[MAX_TIMES];
    for (size_t i = 0; i < stats.count; i++) {
        sorted[i] = stats.times[i];
    }
    qsort(sorted, stats.count, sizeof(int32_t), compare_int32);
    printf("Snapcast: Estimated time diff: %d.%06d sec; difference to prev value: %dms, counts: %d\n", 
        this->est_time_diff_.sec, 
        this->est_time_diff_.usec,
        sorted[stats.count / 2] - this->est_time_diff_ms,
        stats.count
    );
    this->est_time_diff_ms = sorted[stats.count / 2];
    
    // if (stats.count % 2 == 0) {
    //     this->est_time_diff_ms = (sorted[stats.count / 2 - 1] + sorted[stats.count / 2]) / 2;
    // } else {
        
    // }

}


esp_err_t SnapcastStream::read_next_data_chunk(std::shared_ptr<esphome::TimedRingBuffer> ring_buffer, uint32_t timeout_ms){
    const uint32_t timeout = millis() + timeout_ms;
    while( millis() < timeout ){
        size_t to_read = sizeof(MessageHeader) > rx_bufer_length ? sizeof(MessageHeader) - rx_bufer_length : 0;
        if( to_read > 0 ){
            int len = esp_transport_read(this->transport_, (char*) rx_buffer + rx_bufer_length, to_read, timeout_ms);
            if (len <= 0) {
                ESP_LOGW(TAG, "Read snapcast message timeout." );
                return ESP_FAIL;
            } else {
                rx_bufer_length += len;
            }
        }
        if (rx_bufer_length < sizeof(MessageHeader)){
            continue;
        }
        const MessageHeader* msg = reinterpret_cast<const MessageHeader*>(rx_buffer);
        //msg->print();
        to_read = msg->getMessageSize() > rx_bufer_length ? msg->getMessageSize() - rx_bufer_length : 0;
        if ( to_read > 0 ){
            int len = esp_transport_read(this->transport_, (char*) rx_buffer + rx_bufer_length, to_read, timeout_ms);
            if (len <= 0) {
                ESP_LOGW(TAG, "Read snapcast message timeout." );
                return ESP_FAIL;
            } else {
                rx_bufer_length += len;
            }
            if ( rx_bufer_length < msg->getMessageSize()){
                continue;
            }
        }
        // Now we have a complete message in rx_buffer
        // reset buffer length here, so we don't need to take care of it in the switch statement
        rx_bufer_length = 0;
        uint8_t *payload = rx_buffer + sizeof(MessageHeader); 
        size_t payload_len = msg->typed_message_size;
        switch( msg->getMessageType() ){
            case message_type::kCodecHeader:
                {
                    CodecHeaderPayloadView codec_header_payload;
                    if( !codec_header_payload.bind( payload, payload_len) ){
                        ESP_LOGE(TAG, "Error binding codec header payload");
                        return ESP_FAIL;
                    }
                    timed_chunk_t *timed_chunk = nullptr;
                    size_t size = codec_header_payload.payload_size;
                    ring_buffer->acquire_write_chunk(&timed_chunk, sizeof(timed_chunk_t) + size, pdMS_TO_TICKS(timeout_ms));
                    if (timed_chunk == nullptr) {
                        ESP_LOGE(TAG, "Error acquiring write chunk from ring buffer");
                        return ESP_FAIL;
                    }
                    if (codec_header_payload.copyPayloadTo(timed_chunk->data, size ))
                    {
                        ESP_LOGI(TAG, "Codec header payload size: %d", size);
                    } else {
                        ESP_LOGE(TAG, "Error copying codec header payload");
                        return ESP_FAIL;
                    }
                    ring_buffer->release_write_chunk(timed_chunk);
                    codec_header_payload.print();
                    return codec_header_payload.payload_size;
                }
                break;
            case message_type::kWireChunk:
                {
                    WireChunkMessageView wire_chunk_msg;
                    if( !wire_chunk_msg.bind(payload, payload_len) ){
                        ESP_LOGE(TAG, "Error binding wire chunk payload");
                        return ESP_FAIL;
                    }
                    timed_chunk_t *timed_chunk = nullptr;
                    size_t size = wire_chunk_msg.payload_size;
                    ring_buffer->acquire_write_chunk(&timed_chunk, sizeof(timed_chunk_t) + size, pdMS_TO_TICKS(timeout_ms));
                    if (timed_chunk == nullptr) {
                        ESP_LOGE(TAG, "Error acquiring write chunk from ring buffer");
                        return ESP_FAIL;
                    }
                    timed_chunk->stamp = this->to_local_time( tv_t(wire_chunk_msg.timestamp_sec, wire_chunk_msg.timestamp_usec));
                    if (wire_chunk_msg.copyPayloadTo(timed_chunk->data, size))
                    {
                        //ESP_LOGI(TAG, "Wire chunk payload size: %d", size);
                    } else {
                        ESP_LOGE(TAG, "Error copying wire chunk payload");
                        return ESP_FAIL;
                    }
                    ring_buffer->release_write_chunk(timed_chunk);
                    //printf( "Number of chunks in stream buffer: %d\n", ring_buffer->chunks_available());
                    return wire_chunk_msg.payload_size;
                }
                break;
            case message_type::kTime:
                {
                  tv_t stamp;
                  std::memcpy(&stamp, payload, sizeof(stamp));
                    this->on_time_msg(*msg, stamp);
                }
                break;
            case message_type::kServerSettings:
                {
                    ServerSettingsMessage server_settings_msg(*msg, payload, payload_len);
                    server_settings_msg.print();
                }
                break;
            
            default:
                ESP_LOGE(TAG, "Unknown message type: %d", msg->type );
        }
    } // while loop
    return ERR_TIMEOUT;
}

}
}
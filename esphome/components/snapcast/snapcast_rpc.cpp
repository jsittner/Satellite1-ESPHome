#include "snapcast_rpc.h"

namespace esphome {
namespace snapcast {

static const char *const TAG = "snapcast_rpc";

esp_err_t SnapcastControlSession::connect(std::string server, uint32_t port){
    
    if( this->transport_ == NULL ){
        this->transport_ = esp_transport_tcp_init();
        if (this->transport_ == NULL) {
            ESP_LOGE(TAG, "Error occurred during esp_transport_init()");
            return ESP_FAIL;
        }
    }
    
    error_t err = esp_transport_connect(this->transport_, server.c_str(), port, -1);
    if (err != 0) {
        ESP_LOGE(TAG, "Client unable to connect: errno %d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t SnapcastControlSession::disconnect(){
    if( this->transport_ != NULL ){
        esp_transport_close(this->transport_);
        esp_transport_destroy(this->transport_);
        this->transport_ = NULL;
    }
    return ESP_OK;
}




}
}

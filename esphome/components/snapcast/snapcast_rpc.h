#pragma once

#include "esp_transport.h"

#include "esphome/core/defines.h"


namespace esphome {
namespace snapcast {

class SnapcastControlSession {
public:
    esp_err_t connect(std::string server, uint32_t port);
    esp_err_t disconnect();
    
protected:
    esp_transport_handle_t transport_{NULL};
};


}
}

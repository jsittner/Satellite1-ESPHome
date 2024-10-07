#include "sat_gpio.h"

namespace esphome {
namespace satellite1 {

static const char *TAG = "Satellite1-GPIOs";

void Satellite1GPIOPin::digital_write(bool value){
  uint8_t payload[2] = { this->pin_, value };
  this->parent_->transfer( DC_RESOURCE::GPIO_PORTA, GPIO_SERVICER_CMD_SET_PIN, payload, 2);
}

bool Satellite1GPIOPin::digital_read(){
  this->parent_->request_status_register_update();
  uint8_t port_value = this->parent_->get_dc_status( DC_STATUS_REGISTER::GPIO_PORTB );
  return !!( port_value & (1 << this->pin_) ) != this->inverted_;
}

}
}
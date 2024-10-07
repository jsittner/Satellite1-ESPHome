#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace satellite1 {

static const uint8_t CONTROL_STATUS_REGISTER_LEN = 4;

static const uint8_t CONTROL_RESOURCE_CNTRL_ID   = 1;

static const uint8_t RET_STATUS_PAYLOAD_AVAIL = 23;

static const uint8_t CONTROL_COMMAND_IGNORED_IN_DEVICE = 7;

static const uint8_t GPIO_SERVICER_RESID_PORTA = 221;
static const uint8_t GPIO_SERVICER_RESID_PORTB = 211;

namespace DC_RESOURCE {
enum dc_resource_enum {
    CNTRL_ID   = 1,
    GPIO_PORTA = 252,
    GPIO_PORTB = 254
};
}
namespace DC_RET_STATUS {
enum dc_ret_status_enum {
    CMD_SUCCESS = 0,
    DEVICE_BUSY = 7,
    PAYLOAD_AVAILABLE = 23
};
}

namespace DC_STATUS_REGISTER {
enum register_id {
    GPIO_PORTB = 0,
    REGISTER_LEN    
};
}


class Satellite1 : public Component,
                   public spi::SPIDevice <spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                          spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  bool transfer( uint8_t resource_id, uint8_t command, uint8_t* payload, uint8_t payload_len);

  bool request_status_register_update();
  uint8_t get_dc_status( DC_STATUS_REGISTER::register_id reg){assert(reg < CONTROL_STATUS_REGISTER_LEN); return this->dc_status_register_[reg]; }
  
  void set_xmos_rst_pin(GPIOPin* xmos_rst_pin){this->xmos_rst_pin_ = xmos_rst_pin;}
  void set_flash_sw_pin(GPIOPin* flash_sw_pin){this->flash_sw_pin_ = flash_sw_pin;}

protected:
  void set_spi_flash_direct_access_mode_(bool enable);
  
  uint8_t dc_status_register_[CONTROL_STATUS_REGISTER_LEN];
  bool spi_flash_direct_access_enabled_{false};
  
  GPIOPin* xmos_rst_pin_{nullptr};
  GPIOPin* flash_sw_pin_{nullptr};
};



class Satellite1SPIService : public Parented<Satellite1> {
public:
   virtual bool handle_response(uint8_t status, uint8_t res_id, uint8_t cmd, uint8_t* payload, uint8_t payload_len){return false;}

protected:
   uint8_t servicer_id_;

};




}
}
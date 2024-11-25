#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace satellite1 {

static const uint8_t CONTROL_RESOURCE_CNTRL_ID   = 1;
static const uint8_t CONTROL_CMD_READ_BIT = 0x80;

static const uint8_t RET_STATUS_PAYLOAD_AVAIL = 23;

static const uint8_t CONTROL_COMMAND_IGNORED_IN_DEVICE = 7;

static const uint8_t GPIO_SERVICER_RESID_PORT_IN_A = 211;
static const uint8_t GPIO_SERVICER_RESID_PORT_IN_B = 212;
static const uint8_t GPIO_SERVICER_RESID_PORT_OUT_A = 221;

static const uint8_t DFU_CONTROLLER_SERVICER_RESID = 240; 


namespace DC_RESOURCE {
enum dc_resource_enum {
    CNTRL_ID   = 1,
    DFU_CONTROLLER = DFU_CONTROLLER_SERVICER_RESID,
    GPIO_PORT_IN_A = GPIO_SERVICER_RESID_PORT_IN_A,
    GPIO_PORT_IN_B = GPIO_SERVICER_RESID_PORT_IN_B,
    GPIO_PORT_OUT_A = GPIO_SERVICER_RESID_PORT_OUT_A
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
    DEVICE_STATUS   =  0,
    GPIO_PORT_IN_A  =  1,
    GPIO_PORT_IN_B  =  2,
    GPIO_PORT_OUT_A =  3,
    
    REGISTER_LEN    =  4
};
}

namespace DC_DFU_CMD {
enum dc_dfu_cmd_id {
   GET_VERSION = (88 | CONTROL_CMD_READ_BIT),
};  
}




class Satellite1 : public Component,
                   public spi::SPIDevice <spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                          spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ> {
 public:
  std::string xmos_firmware_version(){
    if( this->xmos_firmware_version_major_  || this->xmos_firmware_version_minor_ ||  this->xmos_firmware_version_patch_ ){
       
       return  (   "v" + std::to_string(this->xmos_firmware_version_major_)
                +  "." + std::to_string(this->xmos_firmware_version_minor_) 
                +  "." + std::to_string(this->xmos_firmware_version_patch_));
    }
    return "No Firmware Found.";
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  bool transfer( uint8_t resource_id, uint8_t command, uint8_t* payload, uint8_t payload_len);

  bool request_status_register_update();
  uint8_t get_dc_status( DC_STATUS_REGISTER::register_id reg){assert(reg < DC_STATUS_REGISTER::REGISTER_LEN); return this->dc_status_register_[reg]; }
  
  void set_xmos_rst_pin(GPIOPin* xmos_rst_pin){this->xmos_rst_pin_ = xmos_rst_pin;}
  void set_flash_sw_pin(GPIOPin* flash_sw_pin){this->flash_sw_pin_ = flash_sw_pin;}
  
  void set_spi_flash_direct_access_mode(bool enable);
protected:
  uint8_t dc_status_register_[DC_STATUS_REGISTER::REGISTER_LEN];
  bool spi_flash_direct_access_enabled_{false};
  
  GPIOPin* xmos_rst_pin_{nullptr};
  GPIOPin* flash_sw_pin_{nullptr};

  bool dfu_get_version_();
  uint8_t xmos_firmware_version_major_{0};
  uint8_t xmos_firmware_version_minor_{0};
  uint8_t xmos_firmware_version_patch_{0};
};



class Satellite1SPIService : public Parented<Satellite1> {
public:
   virtual bool handle_response(uint8_t status, uint8_t res_id, uint8_t cmd, uint8_t* payload, uint8_t payload_len){return false;}

protected:
   uint8_t transfer_byte(uint8_t byte ) {return this->parent_->transfer_byte(byte);}
   void enable() {this->parent_->enable();}
   void disable(){this->parent_->disable();}
   uint8_t servicer_id_;

};




}
}
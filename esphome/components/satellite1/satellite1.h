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

static const uint8_t GPIO_SERVICER_RESID = 250;
static const uint8_t GPIO_SERVICER_RESID_PORTA = 252;
static const uint8_t GPIO_SERVICER_RESID_PORTB = 254;

static const uint8_t GPIO_SERVICER_CMD_READ_PORT  = 0x00;
static const uint8_t GPIO_SERVICER_CMD_WRITE_PORT = 0x01;
static const uint8_t GPIO_SERVICER_CMD_SET_PIN    = 0x02;

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


enum class XMOSPort : uint8_t { PORT_A = 0, PORT_B };
enum class XMOSPortMode : uint8_t { GPIO_IN = 0, GPIO_OUT, LED };

static const uint8_t port_to_resource_id[2] = { GPIO_SERVICER_RESID_PORTA, GPIO_SERVICER_RESID_PORTB };

class SatelliteSPIService;

class Satellite1 : public Component,
                   public spi::SPIDevice <spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                          spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  bool transfer( uint8_t resource_id, uint8_t command, uint8_t* payload, uint8_t payload_len);

  bool request_status_register_update();
  uint8_t get_dc_status( DC_STATUS_REGISTER::register_id reg){ assert(reg < CONTROL_STATUS_REGISTER_LEN); return this->dc_status_register_[reg]; }

protected:
  uint8_t dc_status_register_[CONTROL_STATUS_REGISTER_LEN];

};



class SatelliteSPIService : public Parented<Satellite1> {
public:
   
   virtual bool handle_response(uint8_t status, uint8_t res_id, uint8_t cmd, uint8_t* payload, uint8_t payload_len){return false;}
   

protected:
   uint8_t servicer_id_;

};



class Satellite1GPIOPin : public GPIOPin
{
public:
    void setup() override {};
    void pin_mode(gpio::Flags flags) override {}
    bool digital_read() override;
    void digital_write(bool value) override;

    void set_parent(Satellite1 *parent) { this->parent_ = parent; }
    void set_pin(XMOSPort port, uint8_t pin) { this->port_ = port; this->pin_ = pin; }
    void set_inverted(bool inverted) { this->inverted_ = inverted; }
    void set_flags(gpio::Flags flags) { this->flags_ = flags; }

    std::string dump_summary() const override {return ""; };

protected:
    Satellite1 *parent_;
    XMOSPort port_;
    uint8_t pin_;
    bool inverted_;
    gpio::Flags flags_;
};



}
}
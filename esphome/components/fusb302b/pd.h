#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#define PD_MAX_NUM_DATA_OBJECTS 7

namespace esphome {
namespace power_delivery {

enum pd_spec_revision_t {
  PD_SPEC_REV_1 = 0,
  PD_SPEC_REV_2 = 1,
  PD_SPEC_REV_3 = 2
  /* 3 Reserved */
};

enum pd_data_msg_type {
	  /* 0 Reserved */
    PD_DATA_SOURCE_CAP = 0x01,
    PD_DATA_REQUEST = 0x02,
    PD_DATA_BIST = 0x03,
    PD_DATA_SINK_CAP = 0x04,
    PD_DATA_BATTERY_STATUS = 0x05,
    PD_DATA_ALERT = 0x06,
    PD_DATA_GET_COUNTRY_INFO = 0x07,
    PD_DATA_ENTER_USB = 0x08,
    PD_DATA_EPR_REQUEST = 0x09,
    PD_DATA_EPR_MODE = 0x0A,
    PD_DATA_SOURCE_INFO = 0x0B,
    PD_DATA_REVISION = 0x0C,  
    PD_DATA_VENDOR_DEF = 0x0F  
};

enum pd_control_msg_type {
  PD_CNTRL_GOODCRC = 0x01,
  PD_CNTRL_GOTOMIN = 0x02,
  PD_CNTRL_ACCEPT = 0x03,
  PD_CNTRL_REJECT = 0x04,
  PD_CNTRL_PING = 0x05,
  PD_CNTRL_PS_RDY = 0x06,
  PD_CNTRL_GET_SOURCE_CAP = 0x07,
  PD_CNTRL_GET_SINK_CAP = 0x08,
  PD_CNTRL_DR_SWAP = 0x09,
  PD_CNTRL_PR_SWAP = 0x0A,
  PD_CNTRL_VCONN_SWAP = 0x0B,
  PD_CNTRL_WAIT = 0x0C,
  PD_CNTRL_SOFT_RESET = 0x0D,
  PD_CNTRL_NOT_SUPPORTED = 0x10,
  PD_CNTRL_GET_SOURCE_CAP_EXTENDED = 0x11,
  PD_CNTRL_GET_STATUS = 0x12,
  PD_CNTRL_FR_SWAP = 0x13,
  PD_CNTRL_GET_PPS_STATUS = 0x14,
  PD_CNTRL_GET_COUNTRY_CODES = 0x15,
  PD_CNTRL_GET_SINK_CAP_EXTENDED = 0x16,
  PD_CNTRL_GET_SOURCE_INFO = 0x17,
  PD_CNTRL_GET_REVISION = 0x18
};




enum pd_power_data_obj_type {   /* Power data object type */
    PD_PDO_TYPE_FIXED_SUPPLY    = 0,
    PD_PDO_TYPE_BATTERY         = 1,
    PD_PDO_TYPE_VARIABLE_SUPPLY = 2,
    PD_PDO_TYPE_AUGMENTED_PDO   = 3     /* USB PD 3.0 */
};


typedef struct {
    enum pd_power_data_obj_type type;
    uint16_t min_v;     /* Voltage in 50mV units */
    uint16_t max_v;     /* Voltage in 50mV units */
    uint16_t max_i;     /* Current in 10mA units */
    uint16_t max_p;     /* Power in 250mW units */
} pd_power_info_t;

class PDMsg {
public:
  PDMsg() = default;
  PDMsg(uint16_t header);
  PDMsg(pd_control_msg_type cntrl_msg_type);
  PDMsg(pd_data_msg_type data_msg_type, uint32_t* objects, uint8_t len);

  uint16_t get_coded_header() const;
  bool set_header(uint16_t header);
  
  uint8_t type;
  pd_spec_revision_t spec_rev;
  uint8_t id;
  uint8_t num_of_obj;
  bool extended;
  uint32_t data_objects[PD_MAX_NUM_DATA_OBJECTS];

  void debug_log() const;  

//protected:
  static uint8_t msg_cnter_;
  static pd_spec_revision_t spec_rev_;
};




typedef uint32_t pd_pdo_t;


class PowerDelivery {
public:

protected:
  void protocol_reset_();
  
  bool handle_message_(const PDMsg &msg);
  bool handle_data_message_(const PDMsg &msg);
  bool handle_cntrl_message_(const PDMsg &msg);

  pd_power_info_t parse_power_info_( pd_pdo_t &pdo ) const;
  bool respond_to_src_cap_msg_( const PDMsg &msg );
  PDMsg create_fallback_request_message() const;
  
  virtual bool read_message_(PDMsg &msg) = 0;
  virtual bool send_message_(const PDMsg &msg) = 0;
  
  pd_spec_revision_t spec_revision_{pd_spec_revision_t::PD_SPEC_REV_2};
};




}
}
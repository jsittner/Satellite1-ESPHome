#include "pd.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace power_delivery {

static const char *const TAG = "PowerDelivery";

bool PowerDelivery::handle_message_(const PDMsg &msg){
  if( msg.num_of_obj  == 0){
    return this->handle_cntrl_message_(msg);
  } else {
    return this->handle_data_message_(msg);
  }
}

bool PowerDelivery::handle_data_message_(const PDMsg &msg){
  switch( msg.type ){
    case PD_DATA_SOURCE_CAP:
        if( PDMsg::spec_rev_ == pd_spec_revision_t::PD_SPEC_REV_1 ){
          if( msg.spec_rev >= pd_spec_revision_t::PD_SPEC_REV_3 ){
            PDMsg::spec_rev_ = pd_spec_revision_t::PD_SPEC_REV_3;
          } else {
            PDMsg::spec_rev_ = pd_spec_revision_t::PD_SPEC_REV_2;
          }
        }
        PDMsg::spec_rev_ = msg.spec_rev;
        this->respond_to_src_cap_msg_(msg);   
      break;
    default:
     break;  
  }
  return true;
}

bool PowerDelivery::handle_cntrl_message_(const PDMsg &msg){
  switch( msg.type ){
    case PD_CNTRL_GOODCRC:
        ESP_LOGD(TAG, "recieved good crc msg");
        PDMsg::msg_cnter_++;
      break;
    default:
     break;  
  }
  return true;
}




pd_power_info_t pd_parse_power_info( const pd_pdo_t &pdo )
{
  pd_power_info_t power_info;
  power_info.type = static_cast<pd_power_data_obj_type>(pdo >> 30);
  switch (power_info.type) {
  case PD_PDO_TYPE_FIXED_SUPPLY:
      /* Reference: 6.4.1.2.3 Source Fixed Supply Power Data Object */
      power_info.min_v = 0;
      power_info.max_v = (pdo >> 10) & 0x3FF;    /*  B19...10  Voltage in 50mV units */
      power_info.max_i = (pdo >>  0) & 0x3FF;    /*  B9 ...0   Max Current in 10mA units */
      power_info.max_p = 0;
      break;
  case PD_PDO_TYPE_BATTERY:
      /* Reference: 6.4.1.2.5 Battery Supply Power Data Object */
      power_info.min_v = (pdo >> 10) & 0x3FF;    /*  B19...10  Min Voltage in 50mV units */
      power_info.max_v = (pdo >> 20) & 0x3FF;    /*  B29...20  Max Voltage in 50mV units */
      power_info.max_i = 0;
      power_info.max_p = (pdo >>  0) & 0x3FF;    /*  B9 ...0   Max Allowable Power in 250mW units */
      break;
  case PD_PDO_TYPE_VARIABLE_SUPPLY:
      /* Reference: 6.4.1.2.4 Variable Supply (non-Battery) Power Data Object */
      power_info.min_v = (pdo >> 10) & 0x3FF;    /*  B19...10  Min Voltage in 50mV units */
      power_info.max_v = (pdo >> 20) & 0x3FF;    /*  B29...20  Max Voltage in 50mV units */
      power_info.max_i = (pdo >>  0) & 0x3FF;    /*  B9 ...0   Max Current in 10mA units */
      power_info.max_p = 0;
      break;
  case PD_PDO_TYPE_AUGMENTED_PDO:
      /* Reference: 6.4.1.3.4 Programmable Power Supply Augmented Power Data Object */
      power_info.max_v = ((pdo >> 17) & 0xFF) * 2;   /*  B24...17  Max Voltage in 100mV units */
      power_info.min_v = ((pdo >>  8) & 0xFF) * 2;   /*  B15...8   Min Voltage in 100mV units */
      power_info.max_i = ((pdo >>  0) & 0x7F) * 5;   /*  B6 ...0   Max Current in 50mA units */
      power_info.max_p = 0;
      break;
  }
  return power_info;
}


static PDMsg build_source_cap_response( pd_power_info_t pwr_info, uint8_t pos )
{
  
  /* Reference: 6.4.2 Request Message */
  uint32_t data[1];
  if (pwr_info.type != PD_PDO_TYPE_AUGMENTED_PDO) {
      //uint32_t req = pwr_info.max_i ? pwr_info.max_i : pwr_info.max_p;
      uint32_t req = 10;
      data[0] = ((uint32_t) req <<  0) |   /* B9 ...0    Max Operating Current 10mA units / Max Operating Power in 250mW units */
                ((uint32_t) req << 10) |   /* B19...10   Operating Current 10mA units / Operating Power in 250mW units */
                //((uint32_t)   1 << 25) |   /* B25        USB Communication Capable */
                ((uint32_t) 1 << 24 ) |    /*B24  No USB Suspend */
                ((uint32_t) pos << 28);    /* B30...28   Object position (000b is Reserved and Shall Not be used) */
  } else {
    ESP_LOGE(TAG, "Augmented PDO is not supported yet" );
  }
  
  return PDMsg(pd_data_msg_type::PD_DATA_REQUEST, data, 1);
}

PDMsg PowerDelivery::create_fallback_request_message() const {
  //request first PDO, which is always the 5V Fixed Supply
  uint8_t  pos = 1;
  //set max and operational current to 500mA (default maximum for usb) 
  uint32_t req = 10;
  uint32_t data[1] = {
    ((uint32_t) req <<  0)  |   /* B9 ...0    Max Operating Current 10mA units */
    ((uint32_t) req << 10)  |   /* B19...10   Operating Current 10mA units */
                                /* B21...20 Reserved - Shall be set to zero */
  //((uint32_t)   1 << 22)  |   /* B22 EPR Mode Capable */
  //((uint32_t)   1 << 23)  |   /* B23 Unchunked Extended Messages Supported */                         
    ((uint32_t)   1 << 24)  |   /* B24 No USB Suspend */
  //((uint32_t)   1 << 25)  |   /* B25 USB Communication Capable */
  //((uint32_t)   1 << 26)  |   /* B26 Capability Mismatch */
  //((uint32_t)   1 << 27)  |   /* B27 GiveBack flag = 0 */
    ((uint32_t) pos << 28)      /* B31...28   Object position (000b is Reserved and Shall Not be used) */
  };
  return PDMsg(pd_data_msg_type::PD_DATA_REQUEST, data, 1);
}




bool PowerDelivery::respond_to_src_cap_msg_( const PDMsg &msg ){
  // {.limit = 100,  .use_voltage = 1, .use_current = 0},    /* PD_POWER_OPTION_MAX_20V */
  pd_power_info_t selected_info;
  memset( &selected_info, 0 , sizeof(pd_power_info_t) );
  uint8_t selected = 255;
  for(int idx=0; idx < msg.num_of_obj; idx++){
    pd_power_info_t pwr_info = pd_parse_power_info( msg.data_objects[idx] );
    ESP_LOGD(TAG, "SRC_CAP: type: %d V(%d - %d) I(%d) P(%d)",
        pwr_info.type,
        pwr_info.min_v,
        (pwr_info.max_v * 50) / 1000,
        pwr_info.max_i,
        pwr_info.max_p
     );
    if (pwr_info.type == PD_PDO_TYPE_AUGMENTED_PDO) {
        continue;
    } else {
        uint8_t v = true  ? pwr_info.max_v >> 2 : 1;
        uint8_t i = false ? pwr_info.max_i >> 2 : 1;
        uint16_t power = (uint16_t)v * i;  /* reduce 10-bit power info to 8-bit and use 8-bit x 8-bit multiplication */
        if ( pwr_info.max_v * 50 / 1000 >= 20 || selected == 255) {
            selected_info = pwr_info;
            selected = idx;
        }
    }
  }
  //PDMsg response = create_fallback_request_message();
  PDMsg response = build_source_cap_response(selected_info, selected + 1);
  this->send_message_( response );

  return true;
}




pd_spec_revision_t PDMsg::spec_rev_ = pd_spec_revision_t::PD_SPEC_REV_1;
uint8_t PDMsg::msg_cnter_ = 0;

PDMsg::PDMsg(uint16_t header){
  this->set_header(header);
}

bool PDMsg::set_header(uint16_t header){
  this->type = static_cast<pd_data_msg_type>((header >> 0) & 0x1F);  /*   4...0  Message Type */
  this->spec_rev = (pd_spec_revision_t) ((header >> 6) & 0x3);                              /*   7...6  Specification Revision */
  this->id = (header >> 9) & 0x7;                                    /*  11...9  MessageID */
  this->num_of_obj = (header >> 12) & 0x7;                           /* 14...12  Number of Data Objects */
  this->extended = (header >> 15);                                   /* */
  return true;
}

PDMsg::PDMsg(pd_control_msg_type cntrl_msg_type){
   this->type = cntrl_msg_type;
   this->spec_rev = this->spec_rev_;
   this->id = (this->msg_cnter_) % 8;
   this->num_of_obj = 0;
   this->extended = false;
}


PDMsg::PDMsg(pd_data_msg_type msg_type, uint32_t* objects, uint8_t len){
   assert( len > 0 && len < PD_MAX_NUM_DATA_OBJECTS );
   this->type = msg_type;
   this->spec_rev = this->spec_rev_;
   this->id = (this->msg_cnter_) % 8;
   this->num_of_obj = len;
   this->extended = false;
   
   memcpy( this->data_objects, objects, len * sizeof(uint32_t) );
}

uint16_t PDMsg::get_coded_header() const {
  uint16_t h = ((uint16_t) this->type         <<  0 ) |    
               ((uint16_t) 0x00               <<  5 ) |    /* DataRole 0: UFP   */
               ((uint16_t) this->spec_rev     <<  6 ) |
               ((uint16_t) 0x00               <<  8 ) |    /* PowerRole 0: sink */
               ((uint16_t) this->id           <<  9 ) |
               ((uint16_t) this->num_of_obj   << 12 ) |
               ((uint16_t) !!(this->extended) << 15 );  
  return h;
}

void PDMsg::debug_log() const{
  ESP_LOGD(TAG, "PD Message (%d)", this->type );
  ESP_LOGD(TAG, "   type: %d", this->type );
  ESP_LOGD(TAG, "    rev: %d", this->spec_rev );
  ESP_LOGD(TAG, "     id: %d", this->id );
  ESP_LOGD(TAG, "   #obj: %d", this->num_of_obj );
  ESP_LOGD(TAG, "    ext: %d", !!(this->extended) );
  ESP_LOGD(TAG, "  coded: %d", this->get_coded_header());
  ESP_LOGD(TAG, "Current Cnter: %d", this->msg_cnter_);
}


}
}
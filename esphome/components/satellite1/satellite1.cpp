#include "satellite1.h"

#include "esphome/core/log.h"

namespace esphome {
namespace satellite1 {

static const char *TAG = "Satellite1";


void Satellite1::setup(){
    this->spi_setup();
}   


void Satellite1::dump_config(){
}


bool Satellite1::request_status_register_update(){
  bool ret = this->transfer( 0, 0, NULL, 0 );
  uint8_t *arr = this->dc_status_register_;
  return ret;
}


bool Satellite1::transfer( uint8_t resource_id, uint8_t command, uint8_t* payload, uint8_t payload_len){
  if( this->spi_flash_direct_access_enabled_ ){
    return false;
  }
  
  uint8_t send_recv_buf[256+3] = {0};
  int status_report_dummies = std::max<int>( 0, CONTROL_STATUS_REGISTER_LEN - payload_len - 1);
  
  int attempts = 3; 
  do {
    send_recv_buf[0] = resource_id;
    send_recv_buf[1] = command;
    send_recv_buf[2] = payload_len + !!(command & 0x80) ;
    memcpy( &send_recv_buf[3], payload, payload_len );
    this->enable();
    this->transfer_array(&send_recv_buf[0], payload_len + 3 + status_report_dummies);
    this->disable();
    vTaskDelay(1);
  } while( send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE && attempts-- > 0 );
  
    
  if( send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE ) {
    return false;
  }

  // XMOS not responding at all
  if( (send_recv_buf[0] + send_recv_buf[1] + send_recv_buf[2]) == 0 ) {
    return false;
  }

  // Got status register report
  if( send_recv_buf[0] == DC_RESOURCE::CNTRL_ID && send_recv_buf[1] != DC_RET_STATUS::PAYLOAD_AVAILABLE ){
    memcpy( this->dc_status_register_, &send_recv_buf[2], CONTROL_STATUS_REGISTER_LEN );
    uint8_t *arr = this->dc_status_register_;
  }
  
  if( command & 0x80 ){
    attempts = 3; 
    do {
        memset( send_recv_buf, 0, payload_len + 3);
        this->enable();
        this->transfer_array(&send_recv_buf[0], payload_len + 3);
        this->disable();
        vTaskDelay(1);
    } while( send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE && attempts-- > 0 );
  
    if( send_recv_buf[0] == CONTROL_COMMAND_IGNORED_IN_DEVICE ) {
      return false;
    }

    memcpy( payload, &send_recv_buf[1], payload_len );
  }
  
  return true;
}


void Satellite1::set_spi_flash_direct_access_mode(bool enable){
  this->xmos_rst_pin_->digital_write(enable);
  this->flash_sw_pin_->digital_write(enable);
  this->spi_flash_direct_access_enabled_ = enable;
}


}
}
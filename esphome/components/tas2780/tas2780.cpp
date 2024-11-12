#include "tas2780.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tas2780 {

static const char *const TAG = "tas2780";

static const uint8_t TAS2780_PAGE_SELECT = 0x00;        // Page Select

static const uint8_t TAS2780_MODE_CTRL = 0x02;        
static const uint8_t TAS2780_MODE_CTRL_BOP_SRC__PVDD_UVLO  = 0x80;
static const uint8_t TAS2780_MODE_CTRL_MODE_MASK  = 0x07;
static const uint8_t TAS2780_MODE_CTRL_MODE__ACTIVE  = 0x00;
static const uint8_t TAS2780_MODE_CTRL_MODE__ACTIVE_MUTED  = 0x01;
static const uint8_t TAS2780_MODE_CTRL_MODE__SFTW_SHTDWN  = 0x02;


void TAS2780::setup(){
  // select page 0
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
   
  // software reset
  this->reg(0x01) = 0x01;
  
  uint8_t chd1 = this->reg(0x05).get();
  uint8_t chd2 = this->reg(0x68).get();
  uint8_t chd3 = this->reg(0x02).get();

  if( chd1 == 0x41 ){
    ESP_LOGD(TAG, "TAS2780 chip found.");
    ESP_LOGD(TAG, "Reg 0x68: %d.", chd2 );
    ESP_LOGD(TAG, "Reg 0x02: %d.", chd3 );
  }
  else
  {
    ESP_LOGD(TAG, "TAS2780 chip not found.");
    this->mark_failed();
    return;
  }
  
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  this->reg(0x0e) = 0x44;
  this->reg(0x0f) = 0x40;


  this->reg(TAS2780_PAGE_SELECT) = 0x01;
  this->reg(0x17) = 0xc0;
  this->reg(0x19) = 0x00;
  this->reg(0x21) = 0x00;
  this->reg(0x35) = 0x74;

  this->reg(TAS2780_PAGE_SELECT) = 0xFD;
  this->reg(0x0d) = 0x0d;
  this->reg(0x3e) = 0x4a;
  this->reg(0x0d) = 0x00;


  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  //Power Mode 2 (no external VBAT)
  this->reg(0x03) = 0xe8;
  this->reg(0x04) = 0xa1;
  this->reg(0x71) = 0x12; 
  

  //Set interrupt masks
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  //mask VBAT1S Under Voltage
  this->reg(0x3d) = 0xFF;
  //mask all PVDD and VBAT1S interrupts
  this->reg(0x40) = 0xFF;
  this->reg(0x41) = 0xFF;
  
  
  // set interrupt to trigger For 
  // 0h : On any unmasked live interrupts
  // 3h : 2 - 4 ms every 4 ms on any unmasked latched
  uint8_t reg_0x5c = this->reg(0x5c).get();
  this->reg(0x5c) = (reg_0x5c & ~0x03) | 0x00;   

  //set to software shutdown
  this->reg(TAS2780_MODE_CTRL) = (TAS2780_MODE_CTRL_BOP_SRC__PVDD_UVLO & ~TAS2780_MODE_CTRL_MODE_MASK) | TAS2780_MODE_CTRL_MODE__SFTW_SHTDWN;
  }


void TAS2780::activate(){
  // clear interrupt latches
    this->reg(0x5c) = 0x19 | (1 << 2);
  // activate 
  this->reg(TAS2780_MODE_CTRL) = (TAS2780_MODE_CTRL_BOP_SRC__PVDD_UVLO & ~TAS2780_MODE_CTRL_MODE_MASK) | TAS2780_MODE_CTRL_MODE__ACTIVE;
}

void TAS2780::deactivate(){
  // activate 
  //set to software shutdown
  this->reg(TAS2780_MODE_CTRL) = (TAS2780_MODE_CTRL_BOP_SRC__PVDD_UVLO & ~TAS2780_MODE_CTRL_MODE_MASK) | TAS2780_MODE_CTRL_MODE__SFTW_SHTDWN;
}


void TAS2780::reset(){
  // select page 0
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
   
  // software reset
  this->reg(0x01) = 0x01;
  
  uint8_t chd1 = this->reg(0x05).get();
  uint8_t chd2 = this->reg(0x68).get();
  uint8_t chd3 = this->reg(0x02).get();

  if( chd1 == 0x41 ){
    ESP_LOGD(TAG, "TAS2780 chip found.");
    ESP_LOGD(TAG, "Reg 0x68: %d.", chd2 );
    ESP_LOGD(TAG, "Reg 0x02: %d.", chd3 );
  }
  else
  {
    ESP_LOGD(TAG, "TAS2780 chip not found.");
    this->mark_failed();
    return;
  }
  
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  this->reg(0x0e) = 0x44;
  this->reg(0x0f) = 0x40;


  this->reg(TAS2780_PAGE_SELECT) = 0x01;
  this->reg(0x17) = 0xc0;
  this->reg(0x19) = 0x00;
  this->reg(0x21) = 0x00;
  this->reg(0x35) = 0x74;

  this->reg(TAS2780_PAGE_SELECT) = 0xFD;
  this->reg(0x0d) = 0x0d;
  this->reg(0x3e) = 0x4a;
  this->reg(0x0d) = 0x00;


  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  //Power Mode 2 (no external VBAT)
  this->reg(0x03) = 0xe8;
  this->reg(0x04) = 0xa1;
  this->reg(0x71) = 0x12; 
  

  //Set interrupt masks
  this->reg(TAS2780_PAGE_SELECT) = 0x00;
  //mask VBAT1S Under Voltage
  this->reg(0x3d) = 0xFF;
  //mask all PVDD and VBAT1S interrupts
  this->reg(0x40) = 0xFF;
  this->reg(0x41) = 0xFF;
  
  
  // set interrupt to trigger For 
  // 0h : On any unmasked live interrupts
  // 3h : 2 - 4 ms every 4 ms on any unmasked latched
  uint8_t reg_0x5c = this->reg(0x5c).get();
  this->reg(0x5c) = (reg_0x5c & ~0x03) | 0x00;   

  this->activate();
}


void TAS2780::loop(){
  static uint32_t last_call = millis();
  if ( millis() - last_call > 4000 ){
#if 0  
    last_call = millis();
    uint8_t reg2 = this->reg(0x02).get();
    ESP_LOGD(TAG, "Reg 0x02: %d.", reg2 );

    reg2 = this->reg(0x49).get();
    ESP_LOGD(TAG, "Reg 0x49: %d.", reg2 );
    reg2 = this->reg(0x4A).get();
    ESP_LOGD(TAG, "Reg 0x4A: %d.", reg2 );
    reg2 = this->reg(0x4B).get();
    ESP_LOGD(TAG, "Reg 0x4B: %d.", reg2 );
    reg2 = this->reg(0x4F).get();
    ESP_LOGD(TAG, "Reg 0x4F: %d.", reg2 );
    reg2 = this->reg(0x50).get();
    ESP_LOGD(TAG, "Reg 0x50: %d.\n", reg2 );

    // clear interrupt latches
    this->reg(0x5c) = 0x19 | (1 << 2);
    
    // activate 
    this->reg(0x02) = 0x80;
#endif
  }
}



void TAS2780::dump_config(){

}

bool TAS2780::set_mute_off(){
  this->is_muted_ = false;
  return this->write_mute_();
}

bool TAS2780::set_mute_on(){
  this->is_muted_ = true;
  return this->write_mute_();
}

bool TAS2780::set_volume(float volume) {
  this->volume_ = clamp<float>(volume, 0.0, 1.0);
  return this->write_volume_();
}

bool TAS2780::is_muted() {
  return this->is_muted_;
}

float TAS2780::volume() {
  return this->volume_;
}

bool TAS2780::write_mute_() {
  return true;
}

bool TAS2780::write_volume_() {
  return true;
}



}
}
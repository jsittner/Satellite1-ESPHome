#include "external_dac.h"

#ifdef I2S_EXTERNAL_DAC

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstdint>

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "DAC_PCM5122";

static const uint8_t PCM5122_REG00_PAGE_SELECT = 0x00;        // Page Select

//Page 0



bool PCM5122::init_device(){
  
  // select page 0
  this->reg(PCM5122_REG00_PAGE_SELECT) = 0x00;
   
  
  uint8_t chd1 = this->reg(0x09).get();
  uint8_t chd2 = this->reg(0x10).get();
  if( chd1 == 0x00 && chd2 == 0x00 ){
    ESP_LOGD(TAG, "PCM5122 chip found.");
  }
  else
  {
    ESP_LOGD(TAG, "PCM5122 chip not found.");
    return false;
  }

  //RESET
  this->reg(0x01) = 0x10;
  delay(20);
  this->reg(0x01) = 0x00;

  uint8_t err_detect = this->reg(0x25).get();
  //set 'Ignore Clock Halt Detection'
  err_detect |=  (1 << 3);
  //enable Clock Divider Autoset
  err_detect &= ~(1 << 1);
  this->reg(0x25) = err_detect;
  
  //set 32bit - I2S
  this->reg(0x28) = 3; //32bits

  //001: The PLL reference clock is BCK
  uint8_t pll_ref = this->reg(0x0D).get();
  pll_ref &= ~(7 << 4); 
  pll_ref |=  (1 << 4);  
  this->reg(0x0D) = pll_ref;

  return true;
}

bool PCM5122::apply_i2s_settings(const i2s_driver_config_t&  i2s_cfg){
    return true;
}
bool PCM5122::set_mute_audio( bool mute ){
    return true;
}
bool PCM5122::set_volume( float volume ){
    uint8_t status = this->reg(0x5B).get();
    ESP_LOGD(TAG, "DTFS: %hhu", status & 7 );
    ESP_LOGD(TAG, "SCK ratio: %hhu", status >> 3);

    status = this->reg(0x5D).get();
    ESP_LOGD(TAG, "DTBR: %hhu", status );
    
    status = this->reg(0x5E).get();
    ESP_LOGD(TAG, "CDST:  %hhu", (status >> 6) & 1 );
    ESP_LOGD(TAG, "PLL-L: %hhu",    (status >> 5) & 1 );
    ESP_LOGD(TAG, "LrckBck : %hhu", (status >> 4) & 1 );
    ESP_LOGD(TAG, "fS-SCKr : %hhu", (status >> 3) & 1 );
    ESP_LOGD(TAG, "SCKval : %hhu", (status >> 2) & 1 );
    ESP_LOGD(TAG, "BCKval : %hhu", (status >> 1) & 1 );
    ESP_LOGD(TAG, "fSval : %hhu", status  & 1 );

    status = this->reg(0x5F).get();
    ESP_LOGD(TAG, "Clock Error : %hhu", status  & 1 );

    status = this->reg(0x76).get();
    ESP_LOGD(TAG, "BOTM:  %hhu", (status >> 7) & 1 );
    ESP_LOGD(TAG, "Power State:  %hhu", status & 15 );

    
    
    uint8_t vol_value = 255 - std::max<int>( 0 , std::min<int>( volume * 255, 255));
    //set left channel volume:
    this->reg(0x3D) = vol_value;
    //set right channel volume:
    this->reg(0x3E) = vol_value;
    
    return true;
}


}
}

#endif
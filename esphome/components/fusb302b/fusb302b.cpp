#include "fusb302b.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "fusb302_defines.h"

namespace esphome {
namespace power_delivery {

static const char *const TAG = "fusb302b";

static const uint8_t REG_DEVICE_ID = 0x01;
static const uint8_t REG_DEVICE_RESET = 0x0C;     

enum FUSB302_transmit_data_tokens_t {
    TX_TOKEN_TXON    = 0xA1,
    TX_TOKEN_SOP1    = 0x12,
    TX_TOKEN_SOP2    = 0x13,
    TX_TOKEN_SOP3    = 0x1B,
    TX_TOKEN_RESET1  = 0x15,
    TX_TOKEN_RESET2  = 0x16,
    TX_TOKEN_PACKSYM = 0x80,
    TX_TOKEN_JAM_CRC = 0xFF,
    TX_TOKEN_EOP     = 0x14,
    TX_TOKEN_TXOFF   = 0xFE,
};


void FUSB302B::setup(){

  uint8_t chd1 = this->reg(REG_DEVICE_ID).get();
  if( chd1 == 0x81 ){
    ESP_LOGD(TAG, "FUSB302 found, initializing...");
  } else {
    ESP_LOGD(TAG, "FUSB302 not found.");
    ESP_LOGD(TAG, "Device ID: %d.", chd1 );
    this->mark_failed();
    return;
  }
  
  this->fusb_reset_();
  
  /* Set interrupt masks */
  // Setting to 0 so interrupts are allowed
  this->reg(FUSB_MASK1) = 0x00;
  this->reg(FUSB_MASKA) = 0x00;
  this->reg(FUSB_MASKB) = 0x00;
  uint8_t cntrl0 = this->reg(FUSB_CONTROL0).get();
  this->reg(FUSB_CONTROL0) = cntrl0 & ~FUSB_CONTROL0_INT_MASK;

  /* Enable automatic retransmission */
  uint8_t cntrl3 = this->reg(FUSB_CONTROL3).get(); 
  cntrl3 &= ~FUSB_CONTROL3_N_RETRIES_MASK;
  cntrl3 |= (0x03 << FUSB_CONTROL3_N_RETRIES_SHIFT) | FUSB_CONTROL3_AUTO_RETRY;
  this->reg(FUSB_CONTROL3) = cntrl3;
  
  
  this->reg(FUSB_POWER) = 0x0F;

  /* Flush the RX buffer */
  this->reg(FUSB_CONTROL1) = FUSB_CONTROL1_RX_FLUSH;

  
  this->fusb_reset_();

   this->startup_delay_ = millis();
}

void FUSB302B::dump_config(){
}

void FUSB302B::loop(){
  this->check_status_();
  //vTaskDelay( 10 / portTICK_PERIOD_MS);
}


bool FUSB302B::cc_line_selection_(){
  /* Measure CC1 */
  this->reg(FUSB_SWITCHES0) = 0x07;
  this->reg(FUSB_SWITCHES1) = 0x01 << FUSB_SWITCHES1_SPECREV_SHIFT;
  this->reg(FUSB_MEASURE) = 49;
  delay(10);
  
  uint8_t cc1 = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t tmp = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
    if (cc1 != tmp) {
      return false;
    }
  }
  
  /* Measure CC2 */
  this->reg(FUSB_SWITCHES0) = 0x0B;
  delay(10);
  uint8_t cc2 = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t tmp = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
    if (cc2 != tmp) {
      return false;
    }
  }
    
  /* Select the correct CC line for BMC signaling; also enable AUTO_CRC */
  if (cc1 > 0 && cc2 == 0 ) {
    ESP_LOGD(TAG, "CC select: 1");
    
    // PWDN1 | PWDN2 | MEAS_CC1
    this->reg(FUSB_SWITCHES0) = 0x07;
    
    this->reg(FUSB_SWITCHES1) = ( 
                    FUSB_SWITCHES1_TXCC1 | 
                    FUSB_SWITCHES1_AUTO_CRC | 
                    (0x01 << FUSB_SWITCHES1_SPECREV_SHIFT));

  } else if (cc1 == 0 && cc2 > 0) {
    ESP_LOGD(TAG, "CC select: 2");
    // PWDN1 | PWDN2 | MEAS_CC2
    this->reg(FUSB_SWITCHES0) = 0x0B;
    
    this->reg(FUSB_SWITCHES1) = (
                    FUSB_SWITCHES1_TXCC2 | 
                    FUSB_SWITCHES1_AUTO_CRC | 
                    (0x01 << FUSB_SWITCHES1_SPECREV_SHIFT));
  }
  else{
    return false;
  }
  
  return true;
}

bool FUSB302B::check_cc_line_(){
  //uint8_t switches1 = this->reg(FUSB_SWITCHES1).get();
  uint8_t ccX = this->reg(FUSB_STATUS0).get() & FUSB_STATUS0_BC_LVL;
  return ccX > 0;  
}


void FUSB302B::fusb_reset_(){
  /* Flush the TX buffer */
  this->reg(FUSB_CONTROL0) = FUSB_CONTROL0_TX_FLUSH;
  
  /* Flush the RX buffer */
  this->reg(FUSB_CONTROL1) = FUSB_CONTROL1_RX_FLUSH;

  /* Reset the PD logic */
  this->reg(FUSB_RESET) = FUSB_RESET_PD_RESET;

  PDMsg::msg_cnter_ = 0;
}

void FUSB302B::fusb_hard_reset_(){
  uint8_t cntrl3 = this->reg(FUSB_CONTROL3).get();
  this->reg(FUSB_CONTROL3) = cntrl3 | FUSB_CONTROL3_SEND_HARD_RESET;
  delay(5);
  /* Reset the PD logic */
  this->reg(FUSB_RESET) = FUSB_RESET_PD_RESET;
}


void FUSB302B::read_status_(){
  this->read_register(FUSB_STATUS0A, this->status_.bytes , 7);
}


void FUSB302B::check_status_(){
  this->read_status_();
  //ESP_LOGD(TAG, "status0: %d", this->status_.status0 );

  switch( this->state_){
    case FUSB302_STATE_UNATTACHED:
      if( this->status_.status0 & FUSB_STATUS0_VBUSOK )
      {
        /* enable internal oscillator */
        this->reg(FUSB_POWER) = PWR_BANDGAP | PWR_RECEIVER | PWR_MEASURE | PWR_INT_OSC;
        
        if( this->startup_delay_ && millis() - this->startup_delay_ < 10000){
          return; 
        }
        this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2;
        delay(1);
        if( !this->cc_line_selection_() ){
          return;
        }
        
        if( this->startup_delay_ )
        {
          ESP_LOGD(TAG, "Statup delay reached!");
          this->startup_delay_ = 0;
          this->high_freq_.start();
          //uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
          //this->reg(FUSB_SWITCHES1) = sw1 | FUSB_SWITCHES1_AUTO_CRC;
          //this->send_message_( create_fallback_request_message() );
          //this->send_message_( PDMsg(pd_control_msg_type::PD_CNTRL_GET_SOURCE_CAP) ); 
        }        


        this->state_ = FUSB302_STATE_ATTACHED;
        ESP_LOGD(TAG, "USB-C attached");
      }
      break;
    
    case FUSB302_STATE_ATTACHED:
        
#if 0      
      if( this->status_.interrupt ){
        uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
        ESP_LOGD(TAG, "switch1: %d", sw1);
        ESP_LOGD(TAG, "Interrupt: %d", this->status_.interrupt);
        ESP_LOGD(TAG, "status0: %d", this->status_.status0 );
        ESP_LOGD(TAG, "status1: %d", this->status_.status1 );
        ESP_LOGD(TAG, "status0a: %d", this->status_.status0a );
        ESP_LOGD(TAG, "status1a: %d", this->status_.status1a );
      }
#endif      
      if( !(this->status_.status0 & FUSB_STATUS0_VBUSOK)){
        
          //reset cc pins to pull down
          this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2;
          this->reg(FUSB_SWITCHES1) = 0x01 << 5;  
          this->reg(FUSB_MEASURE) = 49;
          
          /* turn off internal oscillator */
          this->reg(FUSB_POWER) = PWR_BANDGAP | PWR_RECEIVER | PWR_MEASURE;
          
          this->state_ = FUSB302_STATE_UNATTACHED;
          this->get_src_cap_retry_count_ = 0;
          this->wait_src_cap_ = true;
          ESP_LOGD(TAG, "USB-C unattached.");
          return;
        
      }
      if ( this->status_.status0a & FUSB_STATUS0A_HARDRST ) {
        ESP_LOGE(TAG, "FUSB_STATUS0A_HARDRST");
        this->fusb_reset_();
        return;
      }

#if 1      
      if( this->status_.interrupta ){
        ESP_LOGD(TAG, "Interrupta: %d", this->status_.interrupta);
      }
      if( this->status_.interrupta & FUSB_INTERRUPTA_I_TXSENT ){
        ESP_LOGD(TAG, "Interrupta: I_TXSENT");
      }
      
      if( this->status_.interrupta & FUSB_INTERRUPTA_I_RETRYFAIL ){
        ESP_LOGE(TAG, "Interrupta: Sending Message Failed.");
      }
      
      if( (this->status_.interrupt & FUSB_INTERRUPT_I_ACTIVITY) && 
          (this->status_.interrupt & FUSB_INTERRUPT_I_CRC_CHK )){
        ESP_LOGD(TAG, "Interrupt: Valid Package was received");
      }
      
      
      
      if (this->status_.interruptb & FUSB_INTERRUPTB_I_GCRCSENT) {
        //Sent a GoodCRC acknowledge packet in response to 
        //an incoming packet that has the correct CRC value
        ESP_LOGD(TAG, "FUSB302_EVENT_GOOD_CRC_SENT");
        this->response_timer_ = millis();
      }
#endif      
      if( !(this->status_.status1 & FUSB_STATUS1_RX_EMPTY)) {
        PDMsg msg;
        if( this->read_message_(msg) ){
          uint8_t empty = this->reg(FUSB_STATUS1).get() & FUSB_STATUS1_RX_EMPTY; 
          ESP_LOGD(TAG, "Message read. buffer empty: %d", !!(empty) );  
          this->handle_message_(msg);
        } else {
          ESP_LOGD(TAG, "reading message failed.");
          //uint8_t cntrl1 = this->reg(FUSB_CONTROL1).get();
          //this->reg(FUSB_CONTROL1) = cntrl1 | FUSB_CONTROL1_RX_FLUSH;
        }
        
      }
#if 0      
      if( false && this->wait_src_cap_ ){
        if( get_src_cap_retry_count_ && millis() - get_src_cap_time_stamp_ < 5000 ){
          return;
        }
        if( !get_src_cap_retry_count_ ){
          get_src_cap_retry_count_++;
          get_src_cap_time_stamp_ = millis();
          return;  
        }
        get_src_cap_retry_count_++;
        get_src_cap_time_stamp_ = millis();
        if( get_src_cap_retry_count_ < 4){
          PDMsg msg = PDMsg(pd_control_msg_type::PD_CNTRL_GET_SOURCE_CAP);
          this->send_msg_(msg);
          ESP_LOGD(TAG, "send get_source_cap msg (%d)", get_src_cap_retry_count_);
        } else {
          ESP_LOGD(TAG, "send get_source_cap reached max count.");
          this->fusb_hard_reset_();
          this->get_src_cap_retry_count_ = 0;
          this->wait_src_cap_ = false;
        } 
     }
#endif
      break;
  }
}

bool FUSB302B::read_message_(PDMsg &msg){
  ESP_LOGD(TAG, "PD-Received new message.");
  
  uint8_t fifo_byte = this->reg(FUSB_FIFOS).get(); 
  bool isSOP = true;
  if( ( fifo_byte & FUSB_FIFO_RX_TOKEN_BITS) != FUSB_FIFO_RX_SOP )
  {
    ESP_LOGD(TAG, "Non SOP packet: %d", fifo_byte);
    isSOP = false;
  }
  uint16_t header;  
  this->read_register(FUSB_FIFOS, (uint8_t*) &header, 2 );
  
  ESP_LOGD(TAG, "Received header: %d", header);
  msg.set_header( header );
  msg.debug_log();
  
  if (msg.num_of_obj > 7 ){
    ESP_LOGE(TAG, "Parsing error, num of objects can't exceed 7.");
    return false;
  }
  
  this->read_register(FUSB_FIFOS, (uint8_t*) msg.data_objects, msg.num_of_obj * sizeof(uint32_t) );
  
  /* Read CRC32 only, the PHY already checked it. */
  uint8_t dummy[4];
  this->read_register(FUSB_FIFOS, dummy, 4);
  return isSOP;
}

bool FUSB302B::send_message_(const PDMsg &msg){
  ESP_LOGD(TAG, "Sending Message (%d) id: %d.", (int) msg.type, msg.id );
  msg.debug_log();
  uint8_t buf[40];
  uint8_t *pbuf = buf;
  
  uint16_t header = msg.get_coded_header();
  uint8_t obj_count = msg.num_of_obj;
  
  *pbuf++ = (uint8_t)TX_TOKEN_SOP1;
  *pbuf++ = (uint8_t)TX_TOKEN_SOP1;
  *pbuf++ = (uint8_t)TX_TOKEN_SOP1;
  *pbuf++ = (uint8_t)TX_TOKEN_SOP2;
  *pbuf++ = (uint8_t)TX_TOKEN_PACKSYM | ((obj_count << 2) + 2);
  *pbuf++ = header & 0xFF; header >>= 8;
  *pbuf++ = header & 0xFF;
  for (uint8_t i = 0; i < obj_count; i++) {
      uint32_t d = msg.data_objects[i];
      *pbuf++ = d & 0xFF; d >>= 8;
      *pbuf++ = d & 0xFF; d >>= 8;
      *pbuf++ = d & 0xFF; d >>= 8;
      *pbuf++ = d & 0xFF;
  }
  *pbuf++ = (uint8_t)TX_TOKEN_JAM_CRC;
  *pbuf++ = (uint8_t)TX_TOKEN_EOP;
  *pbuf++ = (uint8_t)TX_TOKEN_TXOFF;
  *pbuf++ = (uint8_t)TX_TOKEN_TXON;
  
  if( this->write_register( FUSB_FIFOS, buf, pbuf - buf) != i2c::ERROR_OK ){
    ESP_LOGE(TAG, "Sending Message (%d) failed.", (int) msg.type );
  }
  ESP_LOGD( TAG, "Sent message within %dms", millis() -  this->response_timer_);
  delay(1);
	return true;
}



}
}
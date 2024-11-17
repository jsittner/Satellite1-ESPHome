#include "fusb302b.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_rom_gpio.h"
#include <driver/gpio.h>
#include <hal/gpio_types.h>

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

TaskHandle_t xTaskHandle = NULL;


// ISR handler
void IRAM_ATTR fusb302b_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Notify the task from the ISR, using xTaskNotifyFromISR
    xTaskNotifyFromISR(xTaskHandle, 0x01, eSetBits, &xHigherPriorityTaskWoken);

    // Perform a context switch if needed
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}


static void trigger_task(void *params){
  FUSB302B* fusb302b = (FUSB302B*) params;
  uint32_t ulNotificationValue;
  
  while(true){
    if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY) == pdTRUE) {
      uint8_t interrupt = fusb302b->reg(FUSB_INTERRUPT).get();
      uint8_t interrupta = fusb302b->reg(FUSB_INTERRUPTA).get();
      uint8_t interruptb = fusb302b->reg(FUSB_INTERRUPTB).get();
      uint8_t status0 = fusb302b->reg(FUSB_STATUS0).get();
      uint8_t status1 = fusb302b->reg(FUSB_STATUS1).get();
      uint8_t status1a = fusb302b->reg(FUSB_STATUS1A).get();
#if 0      
      ESP_LOGD(TAG, "Interrupt: %d",  interrupt);
      ESP_LOGD(TAG, "Interrupta: %d", interrupta);
      ESP_LOGD(TAG, "Interruptb: %d", interruptb);
      ESP_LOGD(TAG, "status0: %d", status0);
      ESP_LOGD(TAG, "status1: %d", status1);
#endif      
      while( (interrupt & FUSB_INTERRUPT_I_CRC_CHK) || ( interrupta &  FUSB_INTERRUPTA_I_HARDRST) || ( interrupta &  FUSB_INTERRUPTA_I_SOFTRST) )
      {
        if( interrupta & FUSB_INTERRUPTA_I_SOFTRST ){
          ESP_LOGW(TAG, "SOFT_RESET_REQUEST");
        }
        
        if( interrupta & FUSB_INTERRUPTA_I_HARDRST ){
          ESP_LOGE(TAG, "FUSB_STATUS0A_HARDRST");
        }
        
        if( interrupt & FUSB_INTERRUPT_I_CRC_CHK ){
          if( status0 & FUSB_STATUS0_CRC_CHK ){
            while( !(status1 & FUSB_STATUS1_RX_EMPTY) ){
              PDMsg msg;
              if( fusb302b->read_message_(msg) ){
                fusb302b->handle_message_(msg);
              } else {
                ESP_LOGD(TAG, "reading sop message failed.");
                /* Flush the RX buffer */
                fusb302b->reg(FUSB_CONTROL1) = FUSB_CONTROL1_RX_FLUSH;
                break;
              }
              status1 = fusb302b->reg(FUSB_STATUS1).get();
            }
          }
        }
        
        interrupt = fusb302b->reg(FUSB_INTERRUPT).get();
        interrupta = fusb302b->reg(FUSB_INTERRUPTA).get();
        interruptb = fusb302b->reg(FUSB_INTERRUPTB).get();
        status0 = fusb302b->reg(FUSB_STATUS0).get();
        status1 = fusb302b->reg(FUSB_STATUS1).get();
        status1a = fusb302b->reg(FUSB_STATUS1A).get();
      }
      ESP_LOGD(TAG, "Exit while loop.");
    }
  }
}


void FUSB302B::setup(){

  uint8_t chd1 = this->reg(FUSB_DEVICE_ID).get();
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
  this->reg(FUSB_MASK1) = ~FUSB_MASK1_M_CRC_CHK;
  //this->reg(FUSB_MASK1) = 0x51;
  this->reg(FUSB_MASKA) = ~FUSB_MASKA_M_HARDRST & ~FUSB_MASKA_M_RETRYFAIL;
  
  //Mask the I_GCRCSENT interrupt
  this->reg(FUSB_MASKB) = FUSB_MASKB_M_GCRCSENT;
  
    

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
                    //FUSB_SWITCHES1_AUTO_CRC | 
                    (0x01 << FUSB_SWITCHES1_SPECREV_SHIFT));

  } else if (cc1 == 0 && cc2 > 0) {
    ESP_LOGD(TAG, "CC select: 2");
    // PWDN1 | PWDN2 | MEAS_CC2
    this->reg(FUSB_SWITCHES0) = 0x0B;
    
    this->reg(FUSB_SWITCHES1) = (
                    FUSB_SWITCHES1_TXCC2 | 
                    //FUSB_SWITCHES1_AUTO_CRC | 
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

  this->read_status_();
  
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
  switch( this->state_){
    case FUSB302_STATE_UNATTACHED:
      {
      uint8_t status0 = this->reg(FUSB_STATUS0).get();
      if( status0 & FUSB_STATUS0_VBUSOK )
      {
        if( this->startup_delay_ && millis() - this->startup_delay_ < 3000){
          return; 
        }
        /* enable internal oscillator */
        this->reg(FUSB_POWER) = PWR_BANDGAP | PWR_RECEIVER | PWR_MEASURE | PWR_INT_OSC;

        delay(1);
        if( !this->cc_line_selection_() ){
          return;
        }
        
        if( this->startup_delay_ )
        {
          ESP_LOGD(TAG, "Statup delay reached!");
          this->startup_delay_ = 0;

          gpio_num_t irq_gpio_pin = static_cast<gpio_num_t>(this->irq_pin_);

          gpio_config_t io_conf;
          io_conf.pin_bit_mask = (1ULL << irq_gpio_pin);
          io_conf.mode = GPIO_MODE_INPUT;
          io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
          io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
          io_conf.intr_type = GPIO_INTR_NEGEDGE	;

          gpio_config(&io_conf);

          // Install ISR service and attach the ISR handler
          gpio_set_intr_type((gpio_num_t) irq_gpio_pin , GPIO_INTR_NEGEDGE	);
          gpio_install_isr_service(0);
          gpio_isr_handler_add( irq_gpio_pin, fusb302b_isr_handler, NULL);

          // Create the task that will wait for notifications
          xTaskCreatePinnedToCore(trigger_task, "fusb3202b_task", 4096, this , configMAX_PRIORITIES, &xTaskHandle, 0);
          delay(1);
        }  
        
        uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
        this->reg(FUSB_SWITCHES1) = sw1 | FUSB_SWITCHES1_AUTO_CRC;
        this->fusb_reset_();
        
        this->get_src_cap_time_stamp_ = millis();
        this->get_src_cap_retry_count_ = 0;
        this->wait_src_cap_ = true;

        this->state_ = FUSB302_STATE_ATTACHED;
        this->set_state_(PD_STATE_DEFAULT_CONTRACT);
        ESP_LOGD(TAG, "USB-C attached");
      }
      }  
    break;
    
    case FUSB302_STATE_ATTACHED:
        
      uint8_t status0 = this->reg(FUSB_STATUS0).get();
      if( !active_ams_ && !(status0 & FUSB_STATUS0_ACTIVITY) && ((status0 & FUSB_STATUS0_BC_LVL) == 0) ){
          for(int cnt=0; cnt < 5; cnt++ ){
            status0 = this->reg(FUSB_STATUS0).get();
            if( !(status0 & FUSB_STATUS0_ACTIVITY) && ((status0 & FUSB_STATUS0_BC_LVL) > 0))
            {
              return;
            }
          }
          this->fusb_reset_();

          //reset cc pins to pull down only (disable cc measuring)
          this->reg(FUSB_SWITCHES0) = FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2;
          this->reg(FUSB_SWITCHES1) = 0x01 << 5;  
          this->reg(FUSB_MEASURE) = 49;
          
          /* turn off internal oscillator */
          this->reg(FUSB_POWER) = PWR_BANDGAP | PWR_RECEIVER | PWR_MEASURE;

          /* turn off auto_crc responses */
          uint8_t sw1 = this->reg(FUSB_SWITCHES1).get();
          this->reg(FUSB_SWITCHES1) = sw1 | FUSB_SWITCHES1_AUTO_CRC;

          this->state_ = FUSB302_STATE_UNATTACHED;
          this->get_src_cap_retry_count_ = 0;
          this->wait_src_cap_ = true;
          this->set_state_(PD_STATE_DISCONNECTED);
          ESP_LOGD(TAG, "USB-C unattached.");
          return;
      }

      if( this->wait_src_cap_ ){
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
          /* Flush the RX buffer */
          this->reg(FUSB_CONTROL1) = FUSB_CONTROL1_RX_FLUSH;
          this->send_message_(PDMsg( pd_control_msg_type::PD_CNTRL_GET_SOURCE_CAP));
        } else {
          ESP_LOGD(TAG, "send get_source_cap reached max count.");
          this->get_src_cap_retry_count_ = 0;
          this->wait_src_cap_ = false;
        } 
     }
     break;
  }
}

bool FUSB302B::read_message_(PDMsg &msg){
  
  uint8_t fifo_byte = this->reg(FUSB_FIFOS).get(); 
  uint8_t ret = 0;
  
  if( ( fifo_byte & FUSB_FIFO_RX_TOKEN_BITS) != FUSB_FIFO_RX_SOP )
  {
    ESP_LOGD(TAG, "Non SOP packet: %d", fifo_byte);
    ret = 1;
  }
  
  uint16_t header;  
  ret |= this->read_register(FUSB_FIFOS, (uint8_t*) &header, 2);
  msg.set_header( header );
    
  if (msg.num_of_obj > 7 ){
    ESP_LOGE(TAG, "Parsing error, num of objects can't exceed 7.");
    return false;
  } else if ( msg.num_of_obj > 0){
    ret |= this->read_register(FUSB_FIFOS, (uint8_t*) msg.data_objects, msg.num_of_obj * sizeof(uint32_t) );
  }
  
  /* Read CRC32 only, the PHY already checked it. */
  uint8_t dummy[4];
  ret |= this->read_register(FUSB_FIFOS, dummy, 4);
  
  if( ret == 0 ){
    ESP_LOGD(TAG, "PD-Received new message (%d, %d).", msg.type, msg.num_of_obj);
    //msg.debug_log();
  }
  
  return (ret == 0);
}

bool FUSB302B::send_message_(const PDMsg &msg){
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
  
  static uint32_t last_call = millis();
  if( this->write_register( FUSB_FIFOS, buf, pbuf - buf) != i2c::ERROR_OK ){
    ESP_LOGE(TAG, "Sending Message (%d) failed.", (int) msg.type );
  } else {
    ESP_LOGD(TAG, "Sent Message (%d) id: %d. (%d)ms", (int) msg.type, msg.id, millis() - last_call );
  }
  last_call = millis();
  //msg.debug_log();
  
	return true;
}


  


}
}
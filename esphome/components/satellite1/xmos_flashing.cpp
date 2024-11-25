#include "xmos_flashing.h"
#include "esphome/core/log.h"

#include <endian.h>
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace satellite1 {

static const char *const TAG = "xmos_flasher";

static const size_t FLASH_PAGE_SIZE = 256;
static const size_t FLASH_SECTOR_SIZE = 4096;


void XMOSFlasher::dump_flash_info(){
  esph_log_config(TAG, "Satellite1-Flasher:");
  esph_log_config(TAG, "	JEDEC-manufacturerID %hhu", this->_manufacturerID);
  esph_log_config(TAG, "	JEDEC-memoryTypeID %hhu", this->_memoryTypeID);
  esph_log_config(TAG, "	JEDEC-capacityID %hhu", this->_capacityID);	
}


bool XMOSFlasher::init_flasher(){
  ESP_LOGD(TAG, "Setting up XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode(true);
  this->read_JEDECID_();
  this->dump_flash_info();
  return true;
 }

bool XMOSFlasher::deinit_flasher(){
  ESP_LOGD(TAG, "Stopping XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode(false);
  return true;
 }

void XMOSFlasher::flash_remote_image(){
  if (this->state != XMOS_FLASHER_IDLE){
    ESP_LOGE(TAG, "XMOS flasher is busy, can't inititate new flash");
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
    return;
  }
  
  this->set_state_(XMOS_FLASHER_RECEIVING_IMG_INFO);
  ESP_LOGI(TAG, "Starting flashing...");
  
  if (this->url_.empty()) {
    ESP_LOGE(TAG, "URL not set; cannot start flashing");
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
    return;
  }

  if (this->md5_expected_.empty() && !this->http_get_md5_()) {
    ESP_LOGE(TAG, "XMOS flasher is busy, can't inititate new flash");
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
    return;
  }
  
  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  HttpImageReader http_reader = HttpImageReader(this->http_request_, this->url_);
  this->set_state_(XMOS_FLASHER_INITIALIZING);
  if (this->init_flasher()){
    this->set_state_(XMOS_FLASHER_FLASHING);
    
    if ( this->write_to_flash_(http_reader) != XMOS_FLASHING_OK ){
      this->set_state_(XMOS_FLASHER_ERROR_STATE);
    }
    else {
      this->set_state_(XMOS_FLASHER_SUCCESS_STATE);
    }
  }
  else {
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
  }

  this->md5_computed_.clear();  // will be reset at next attempt
  this->md5_expected_.clear();  // will be reset at next attempt
      
  delay(5);
  this->deinit_flasher();
}


void XMOSFlasher::flash_embedded_image(){
  if (this->state != XMOS_FLASHER_IDLE){
    ESP_LOGE(TAG, "XMOS flasher is busy, can't inititate new flash");
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
    return;
  }
  
  ESP_LOGI(TAG, "Starting flashing...");

  if (this->embedded_image_.length == 0) {
    ESP_LOGE(TAG, "Didn't find embedded image!");
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
    return;
  }

  this->md5_expected_ = this->embedded_image_.md5;
  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  EmbeddedImageReader embedded_reader = EmbeddedImageReader(this->embedded_image_);
  this->set_state_(XMOS_FLASHER_INITIALIZING);
  if (this->init_flasher()){
    this->set_state_(XMOS_FLASHER_FLASHING);  
    
    if( this->write_to_flash_(embedded_reader) == XMOS_FLASHING_OK )
    {
      this->set_state_(XMOS_FLASHER_SUCCESS_STATE);
    }
    else {
      this->set_state_(XMOS_FLASHER_ERROR_STATE);
    }
  }
  else {
    this->set_state_(XMOS_FLASHER_ERROR_STATE);
  }
  
  this->md5_computed_.clear();  // will be reset at next attempt
  this->md5_expected_.clear();  // will be reset at next attempt
  
  delay(5);
  this->deinit_flasher();
}


void XMOSFlasher::read_JEDECID_(){
  /*
	_beginSPI(JEDECID); { _nextByte(WRITE, 0x9F);}
 	_chip.manufacturerID = _nextByte(READ);		// manufacturer id
 	_chip.memoryTypeID =   _nextByte(READ);		// memory type
 	_chip.capacityID =     _nextByte(READ);	  // capacity
  */
  this->enable();
  this->transfer_byte(0x9F);
  this->_manufacturerID = this->transfer_byte(0);
  this->_memoryTypeID = this->transfer_byte(0);
  this->_capacityID = this->transfer_byte(0);
  this->disable();
}


bool XMOSFlasher::http_get_md5_(){
  if (this->md5_url_.empty()) {
    return false;
  }

  ESP_LOGI(TAG, "Connecting to: %s", this->md5_url_.c_str());
  auto container = this->http_request_->get(this->md5_url_);
  if (container == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to MD5 URL");
    return false;
  }
  size_t length = container->content_length;
  if (length == 0) {
    container->end();
    return false;
  }
  if (length < MD5_SIZE) {
    ESP_LOGE(TAG, "MD5 file must be %u bytes; %u bytes reported by HTTP server. Aborting", MD5_SIZE, length);
    container->end();
    return false;
  }

  this->md5_expected_.resize(MD5_SIZE);
  int read_len = 0;
  while (container->get_bytes_read() < MD5_SIZE) {
    read_len = container->read((uint8_t *) this->md5_expected_.data(), MD5_SIZE);
    App.feed_wdt();
    yield();
  }
  container->end();

  ESP_LOGV(TAG, "Read len: %u, MD5 expected: %u", read_len, MD5_SIZE);
  return read_len == MD5_SIZE;
}

bool XMOSFlasher::validate_url_(const std::string &url){
  return true;
}

void XMOSFlasher::set_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->url_ = url;
}

void XMOSFlasher::set_md5_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->md5_url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->md5_url_ = url;
  this->md5_expected_.clear();  // to be retrieved later
}



bool XMOSFlasher::wait_while_flash_busy_(uint32_t timeout_ms){
  int32_t timeout_invoke = millis();
  const uint8_t WEL  = 2;
  const uint8_t BUSY = 1;

  while( (millis() - timeout_invoke) < timeout_ms ){
    this->enable();
    this->transfer_byte(0x05);
    uint8_t status = this->transfer_byte(0x00);
    this->disable();
    if( (status & BUSY) == 0){
       return true;      
    }
  }
  return false;
}

bool XMOSFlasher::enable_writing_(){
  //enable writing
  this->enable();
  this->transfer_byte(0x06);
  this->disable();
  
  this->enable();
  this->transfer_byte(0x05);
  uint8_t status = this->transfer_byte(0x00);
  this->disable();
  const uint8_t WEL  = 2;
  if( !(status & WEL)){
    return false;      
  }
  return true;
}

bool XMOSFlasher::disable_writing_(){
  //disable writing
  this->enable();
  this->transfer_byte(0x04);
  this->disable();
  return true;
}

bool XMOSFlasher::erase_sector_(int sector){
  //erase 4kB sector
  assert( FLASH_SECTOR_SIZE == 4096 );
  uint32_t u32 = htole32(sector * FLASH_SECTOR_SIZE);
  uint8_t* u8_ptr = (uint8_t*) &u32;
  
  if( !this->enable_writing_()){
    return false;
  }
  
  this->enable();
  this->transfer_byte(0x20);
  this->transfer_byte( *(u8_ptr+2));
  this->transfer_byte( *(u8_ptr+1));
  this->transfer_byte( *(u8_ptr)  );
  this->disable();

  if( !this->wait_while_flash_busy_(200) ){
    ESP_LOGE(TAG, "Erasing timeout" );
    return false;
  }
  this->disable_writing_();
  return true;
}  

bool XMOSFlasher::write_page_( uint32_t byte_addr, uint8_t* buffer ){
  if ((byte_addr & (FLASH_PAGE_SIZE - 1)) != 0){
    ESP_LOGE(TAG, "Address needs to be page aligned (%d).", FLASH_PAGE_SIZE);
    return false;
  }
  if( !this->enable_writing_()){
    ESP_LOGE(TAG, "Couldn't enable writing");
    return false;
  }
  
  uint32_t u32 = htole32(byte_addr);
  uint8_t* u8_ptr = (uint8_t*) &u32;
  this->enable();
  this->transfer_byte(0x02);
  this->transfer_byte( *(u8_ptr+2));
  this->transfer_byte( *(u8_ptr+1));
  this->transfer_byte( *(u8_ptr)  );
  for(int pos = 0; pos < FLASH_PAGE_SIZE; pos++){
    this->transfer_byte(*(buffer + pos));
  }
  this->disable();

  if( !this->wait_while_flash_busy_(15) ){
    ESP_LOGE(TAG, "Writing page timeout");
    return false;
  }
  this->disable_writing_();
  return true;
}

bool XMOSFlasher::read_page_( uint32_t byte_addr, uint8_t* buffer ){
  if ((byte_addr & (FLASH_PAGE_SIZE - 1)) != 0){
    return false;
  }
  uint32_t u32 = htole32(byte_addr);
  uint8_t* u8_ptr = (uint8_t*) &u32;
  this->enable();
  this->transfer_byte(0x0B);
  this->transfer_byte( *(u8_ptr+2));
  this->transfer_byte( *(u8_ptr+1));
  this->transfer_byte( *(u8_ptr)  );
  this->transfer_byte(0x00);
  for(int pos=0; pos < FLASH_PAGE_SIZE; pos++){
    *(buffer + pos) = this->transfer_byte(0x00);
  }
  this->disable();
  return true;
}




uint8_t XMOSFlasher::write_to_flash_(XMOSImageReader &reader) {
  this->flashing_progress = 0;
  uint32_t last_progress = 0;
  uint32_t update_start_time = millis();
  md5::MD5Digest md5_receive;
  std::unique_ptr<char[]> md5_receive_str(new char[33]);

  // we will compute MD5 on the fly for verification
  md5_receive.init();
  ESP_LOGV(TAG, "MD5Digest initialized");
  
  reader.init_reader();
  size_t size_in_bytes = reader.get_image_size();
  size_t size_in_pages = ((size_in_bytes + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE);
  size_t size_in_sectors = ((size_in_pages * FLASH_PAGE_SIZE + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);

  
  for( int sector = 0; sector < size_in_sectors; sector++ ){
    if( !this->erase_sector_(sector) ){
      ESP_LOGE(TAG, "Error while erasing sector %d", sector );
      return XMOS_INIT_FLASH_ERROR;
    }
  }
  
  uint8_t dnload_req[HTTP_RECV_BUFFER]; 
  uint8_t cmp_buf[HTTP_RECV_BUFFER];
 
  size_t page_pos = 0;
  size_t bytes_read = 0;
  while ( bytes_read < size_in_bytes ) {

    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = reader.read_image_block(dnload_req, HTTP_RECV_BUFFER);
    bytes_read += bufsize;
    //ESP_LOGD( TAG, "read %d from %d (current: %d)", bytes_read, size_in_bytes, bufsize );

    if (bufsize < 0) {
      ESP_LOGE(TAG, "Stream closed");
      reader.deinit_reader();
      return XMOS_CONNECTION_ERROR;
    } else if (bufsize == FLASH_PAGE_SIZE) {
      md5_receive.add(&dnload_req[0], bufsize);
      
      if( !this->write_page_(page_pos, &dnload_req[0] )){
        ESP_LOGE(TAG, "writing page error");
      }
      
      //read back the page that has just been written
      this->read_page_(page_pos, &cmp_buf[0] );
      
      if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0){
        // not equal, give it a second try
        if( !this->write_page_(page_pos, &dnload_req[0] )){
          ESP_LOGE(TAG, "writing page error");
        } 
        
        this->read_page_(page_pos, &cmp_buf[0] );
        if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0){
          ESP_LOGE(TAG, "Read page mismatch, page addr: %d", page_pos );
          return XMOS_INIT_FLASH_ERROR;
        }  
      }
      page_pos += FLASH_PAGE_SIZE;

    } else if (bufsize > 0 && bufsize < HTTP_RECV_BUFFER) {
      // add read bytes to MD5
      if ( ( bytes_read != size_in_bytes) ){
        return XMOS_CONNECTION_ERROR;
      }
      
      md5_receive.add(&dnload_req[0], bufsize);
      memset(&dnload_req[bufsize], 0, HTTP_RECV_BUFFER - bufsize);
      this->write_page_(page_pos, &dnload_req[0] );
      this->read_page_(page_pos, &cmp_buf[0] );
      if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0 ){
        return XMOS_INIT_FLASH_ERROR;
      }
      page_pos += FLASH_PAGE_SIZE;
    }
    
    this->flashing_progress = bytes_read * 100 / size_in_bytes;

    App.feed_wdt();
    uint32_t now = millis();
    if ((now - last_progress > 1000) or (bytes_read == size_in_bytes)) {
      last_progress = now;
      float percentage = bytes_read * 100.0f / size_in_bytes;
      this->flashing_progress = (uint8_t) percentage;
      this->state_callback_.call();
      ESP_LOGD(TAG, "Progress: %0.1f%%", percentage);
    }
  }  // while

  reader.deinit_reader();

  ESP_LOGI(TAG, "Done in %.0f seconds", float(millis() - update_start_time) / 1000);

  // verify MD5 is as expected and act accordingly
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str.get());
  this->md5_computed_ = md5_receive_str.get();
  if (strncmp(this->md5_computed_.c_str(), this->md5_expected_.c_str(), MD5_SIZE) != 0) {
    ESP_LOGE(TAG, "MD5 computed: %s - Aborting due to MD5 mismatch", this->md5_computed_.c_str());
    return XMOS_MD5_MISMATCH_ERROR;
  } else {
    ESP_LOGD(TAG, "MD5 computed: %s - Matches!", this->md5_computed_.c_str());
  }

    
  return XMOS_FLASHING_OK;
}



bool HttpImageReader::init_reader(){
  auto url_with_auth = this->url_;
    if (url_with_auth.empty() || this->http_request_ == nullptr) {
        return false;
    }
    
    ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
    ESP_LOGI(TAG, "Connecting to: %s", this->url_.c_str());
    
    this->container_ = this->http_request_->get(url_with_auth);
    if (this->container_ == nullptr) {
      return false;
    }
    return true;
}

bool HttpImageReader::deinit_reader(){
    if( this->container_ != nullptr ){
      this->container_->end();
    }
    return true;
}


int HttpImageReader::read_image_block(uint8_t *buffer, size_t block_size){
    int bytes_read = this->container_->read(buffer, block_size);
    ESP_LOGVV(TAG, "bytes_read_ = %u, body_length_ = %u, bufsize = %i", container->get_bytes_read(),
              container->content_length, bytes_read);
    return bytes_read;
} 


}
}
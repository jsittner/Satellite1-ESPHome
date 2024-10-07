#include "flashing.h"
#include "esphome/core/log.h"

#include <endian.h>
#include "esphome/components/md5/md5.h"

namespace esphome {
namespace satellite1 {

static const char *const TAG = "xmos_flasher";

static const size_t FLASH_PAGE_SIZE = 256;
static const size_t FLASH_SECTOR_SIZE = 4096;

void SatelliteFlasher::setup() {
}

void SatelliteFlasher::dump_config(){
}


void SatelliteFlasher::dump_flash_info(){
  esph_log_config(TAG, "Satellite1-Flasher:");
  esph_log_config(TAG, "	JEDEC-manufacturerID %hhu", this->_manufacturerID);
  esph_log_config(TAG, "	JEDEC-memoryTypeID %hhu", this->_memoryTypeID);
  esph_log_config(TAG, "	JEDEC-capacityID %hhu", this->_capacityID);	
}


bool SatelliteFlasher::init_flasher(){
  ESP_LOGD(TAG, "Setting up XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode_(true);
  this->read_JEDECID_();
  this->dump_flash_info();
  return true;
 }

bool SatelliteFlasher::deinit_flasher(){
  ESP_LOGD(TAG, "Stopping XMOS flasher...");
  this->parent_->set_spi_flash_direct_access_mode_(false);
  return true;
 }



void SatelliteFlasher::read_JEDECID_(){
  /*
	_beginSPI(JEDECID); { _nextByte(WRITE, 0x9F);}
 	_chip.manufacturerID = _nextByte(READ);		// manufacturer id
 	_chip.memoryTypeID =   _nextByte(READ);		// memory type
 	_chip.capacityID =     _nextByte(READ);		   // capacity
  */
  this->enable();
  this->transfer_byte(0x9F);
  this->_manufacturerID = this->transfer_byte(0);
  this->_memoryTypeID = this->transfer_byte(0);
  this->_capacityID = this->transfer_byte(0);
  this->disable();
}


bool SatelliteFlasher::check_crc_consistency(){
  this->enable();
  
  //read command
  this->transfer_byte(0x0B);
  // address to read from (32-bit, program size s i stored at address 0x00 in 32-bit):   
  this->transfer_byte(0x0);
  this->transfer_byte(0x0);
  this->transfer_byte(0x0);
  // dummy byte needed for 0x0B command
  this->transfer_byte(0x0);

  CRC32 crc32;
  
  //read program size in words (s) 32-bit
  //The program size and CRC are stored least significant byte first.
  uint32_t size = 0; 
  for( uint8_t byte_pos=0; byte_pos < 4; byte_pos++ ){
	uint8_t new_byte = this->transfer_byte(0x0);
	size  |= new_byte << (8 * byte_pos);
	crc32.update(new_byte);	
  }
  
  for( int byte_pos=0; byte_pos < size * 4; byte_pos++ ){
	uint8_t new_byte = this->transfer_byte(0x0);
	crc32.update(new_byte);	
  }	
  
  uint32_t crc = 0; 
  for( uint8_t byte_pos=0; byte_pos < 4; byte_pos++ ){
	uint8_t new_byte = this->transfer_byte(0x0);
	crc  |= new_byte << (8 * byte_pos);
  }

  this->disable();

  esph_log_d(TAG, "Program size is: %lu", (unsigned long ) size);
  esph_log_d(TAG, "read CRC: %lu", (unsigned long ) crc);
  esph_log_d(TAG, "calc CRC: %lu", (unsigned long )  crc32.get_value());
  esph_log_d(TAG, "calc CRC: %lu", (unsigned long ) ~crc32.get_value());
  
  return true;
}

bool SatelliteFlasher::http_get_md5_(){
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

bool SatelliteFlasher::validate_url_(const std::string &url){
  return true;
}

void SatelliteFlasher::set_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->url_ = url;
}

void SatelliteFlasher::set_md5_url(const std::string &url) {
  if (!this->validate_url_(url)) {
    this->md5_url_.clear();  // URL was not valid; prevent flashing until it is
    return;
  }
  this->md5_url_ = url;
  this->md5_expected_.clear();  // to be retrieved later
}

void SatelliteFlasher::flash(){
  if (this->url_.empty()) {
    ESP_LOGE(TAG, "URL not set; cannot start update");
    return;
  }

  ESP_LOGI(TAG, "Starting update...");
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
#endif

  uint8_t ota_status;
  if (this->init_flasher() ){
    ota_status = this->write_to_flash_();
    this->deinit_flasher();
  }
  else {
    ota_status = OTA_CONNECTION_ERROR;
  }
  
  ESP_LOGD(TAG, "flashing return status: %d", ota_status);
 
  switch (ota_status) {
    case ota::OTA_RESPONSE_OK:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, ota_status);
#endif
      // delay(10);
      // App.safe_reboot();
      break;

    default:
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_ERROR, 0.0f, ota_status);
#endif
      this->md5_computed_.clear();  // will be reset at next attempt
      this->md5_expected_.clear();  // will be reset at next attempt
      break;
  }
}


bool SatelliteFlasher::wait_while_flash_busy_(uint32_t timeout_ms){
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

bool SatelliteFlasher::enable_writing_(){
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

bool SatelliteFlasher::disable_writing_(){
  //disable writing
  this->enable();
  this->transfer_byte(0x04);
  this->disable();
  return true;
}

bool SatelliteFlasher::erase_sector_(int sector){
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

bool SatelliteFlasher::write_page_( uint32_t byte_addr, uint8_t* buffer ){
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

bool SatelliteFlasher::read_page_( uint32_t byte_addr, uint8_t* buffer ){
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



uint8_t SatelliteFlasher::write_to_flash_() {
  uint32_t last_progress = 0;
  uint32_t update_start_time = millis();
  md5::MD5Digest md5_receive;
  std::unique_ptr<char[]> md5_receive_str(new char[33]);

  if (this->md5_expected_.empty() && !this->http_get_md5_()) {
    return OTA_MD5_INVALID;
  }

  ESP_LOGD(TAG, "MD5 expected: %s", this->md5_expected_.c_str());

  auto url_with_auth = this->url_;
  if (url_with_auth.empty()) {
    return OTA_BAD_URL;
  }

  // we will compute MD5 on the fly for verification
  md5_receive.init();
  ESP_LOGV(TAG, "MD5Digest initialized");

  ESP_LOGVV(TAG, "url_with_auth: %s", url_with_auth.c_str());
  ESP_LOGI(TAG, "Connecting to: %s", this->url_.c_str());

  auto container = this->http_request_->get(url_with_auth);

  if (container == nullptr) {
    return OTA_CONNECTION_ERROR;
  }

  ESP_LOGV(TAG, "OTA begin");
  
  size_t size_in_pages = ((container->content_length + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE);
  size_t size_in_sectors = ((size_in_pages * FLASH_PAGE_SIZE + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);

  
  for( int sector = 0; sector < size_in_sectors; sector++ ){
    if( !this->erase_sector_(sector) ){
      ESP_LOGE(TAG, "Error while erasing sector %d", sector );
      return OTA_INIT_FLASH_ERROR;
    }
  }
  
  uint8_t dnload_req[HTTP_RECV_BUFFER]; 
  uint8_t cmp_buf[HTTP_RECV_BUFFER];
 
  this->read_page_(0, &cmp_buf[0] );
  for(int pos=0; pos < 10 ; pos++){
    ESP_LOGD(TAG, "%.2x", cmp_buf[pos] );
  }

  size_t page_pos = 0;
  while (container->get_bytes_read() < container->content_length) {

    // read a maximum of chunk_size bytes into buf. (real read size returned)
    int bufsize = container->read(&dnload_req[0], HTTP_RECV_BUFFER);
    ESP_LOGVV(TAG, "bytes_read_ = %u, body_length_ = %u, bufsize = %i", container->get_bytes_read(),
              container->content_length, bufsize);

    if (bufsize < 0) {
      ESP_LOGE(TAG, "Stream closed");
      this->cleanup_(container);
      return OTA_CONNECTION_ERROR;
    } else if (bufsize == FLASH_PAGE_SIZE) {
      md5_receive.add(&dnload_req[0], bufsize);
      
      //this->read_page_(page_pos, &dbg_buf[0] );
      
      if( !this->write_page_(page_pos, &dnload_req[0] )){
        ESP_LOGE(TAG, "writing page error");
      }
      
      this->read_page_(page_pos, &cmp_buf[0] );
      
      if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0){
        //give it a second try
        /*
        for(int pos=0; pos < FLASH_PAGE_SIZE ; pos++){
          if( cmp_buf[pos] != dnload_req[pos] ){
             ESP_LOGD(TAG, "%d: %.2x - %.2x ", pos, cmp_buf[pos], dnload_req[pos] );
          }
        }
        */
        if( !this->write_page_(page_pos, &dnload_req[0] )){
          ESP_LOGE(TAG, "writing page error");
        } 
        
        this->read_page_(page_pos, &cmp_buf[0] );
        if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0){
          ESP_LOGE(TAG, "Read page mismatch, page addr: %d", page_pos );
          return OTA_INIT_FLASH_ERROR;
        }  
      }
      page_pos += FLASH_PAGE_SIZE;

    } else if (bufsize > 0 && bufsize < HTTP_RECV_BUFFER) {
      // add read bytes to MD5
      if ( (container->get_bytes_read() != container->content_length) ){
        return OTA_CONNECTION_ERROR;
      }
      md5_receive.add(&dnload_req[0], bufsize);
      memset(&dnload_req[bufsize], 0, HTTP_RECV_BUFFER - bufsize);
      this->write_page_(page_pos, &dnload_req[0] );
      this->read_page_(page_pos, &cmp_buf[0] );
      if (memcmp(&dnload_req[0], &cmp_buf[0], FLASH_PAGE_SIZE) != 0 ){
        return OTA_INIT_FLASH_ERROR;
      }
      page_pos += FLASH_PAGE_SIZE;
    }

    uint32_t now = millis();
    if ((now - last_progress > 1000) or (container->get_bytes_read() == container->content_length)) {
      last_progress = now;
      float percentage = container->get_bytes_read() * 100.0f / container->content_length;
      ESP_LOGD(TAG, "Progress: %0.1f%%", percentage);
#ifdef USE_OTA_STATE_CALLBACK
      this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage, 0);
#endif
    }
  }  // while


  container->end();

  ESP_LOGI(TAG, "Done in %.0f seconds", float(millis() - update_start_time) / 1000);

  // verify MD5 is as expected and act accordingly
  md5_receive.calculate();
  md5_receive.get_hex(md5_receive_str.get());
  this->md5_computed_ = md5_receive_str.get();
  if (strncmp(this->md5_computed_.c_str(), this->md5_expected_.c_str(), MD5_SIZE) != 0) {
    ESP_LOGE(TAG, "MD5 computed: %s - Aborting due to MD5 mismatch", this->md5_computed_.c_str());
    return ota::OTA_RESPONSE_ERROR_MD5_MISMATCH;
  }

  /*
  ESP_LOGI(TAG, "Rebooting XMOS SoC...");
  if (!this->dfu_reboot_()) {
    return OTA_COMMUNICATION_ERROR;
  }

  delay(100);  // NOLINT

  while (!this->dfu_get_version_()) {
    delay(250);  // NOLINT
    // feed watchdog and give other tasks a chance to run
    App.feed_wdt();
    yield();
  }

  ESP_LOGI(TAG, "Update complete");
  */
  
  return ota::OTA_RESPONSE_OK;
}


void SatelliteFlasher::cleanup_(const std::shared_ptr<http_request::HttpContainer> &container) {
  ESP_LOGV(TAG, "Aborting HTTP connection");
  container->end();
};



void CRC32::generate_table(){
    uint32_t polynomial = 0xEDB88320;
	for (uint32_t i = 0; i < 256; i++)
	{
		uint32_t c = i;
		for (size_t j = 0; j < 8; j++)
		{
			if (c & 1) {
				c = polynomial ^ (c >> 1);
			}
			else {
				c >>= 1;
			}
		}
		this->table[i] = c;
	}
}

void CRC32::update(const uint8_t val){
  /*
  uint32_t c = initial ^ 0xFFFFFFFF;
  c = table[(c ^ val) & 0xFF] ^ (c >> 8);
  initial = c ^ 0xFFFFFFFF;
  */
  uint32_t crc32_rev = crc;
  for (int i = 0; i < 8; i++)
  {
	if ((crc32_rev & 1) != ((val >> i) & 1) )
		crc32_rev = (crc32_rev >> 1) ^ 0xEDB88320;
	else
		crc32_rev = (crc32_rev >> 1);
 } 
 crc  =  crc32_rev;	

}


}
}
#pragma once

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/spi/spi.h"

#include "esphome/components/satellite1/satellite1.h"

namespace esphome {
namespace satellite1 {

static const uint8_t MD5_SIZE = 32;

enum XMOSFlasherError : uint8_t {
  XMOS_FLASHING_OK,
  XMOS_INIT_FLASH_ERROR,
  XMOS_WRITE_TO_FLASH_ERROR, 
  XMOS_MD5_MISMATCH_ERROR,

  XMOS_MD5_INVALID,
  XMOS_BAD_URL,
  XMOS_CONNECTION_ERROR,

  XMOS_NO_EMBEDDED_IMAGE_ERROR
};

enum XMOSFlasherState : uint8_t {
  XMOS_FLASHER_IDLE,
  XMOS_FLASHER_RECEIVING_IMG_INFO,
  XMOS_FLASHER_INITIALIZING,
  XMOS_FLASHER_FLASHING,
  XMOS_FLASHER_SUCCESS_STATE,
  XMOS_FLASHER_ERROR_STATE
};


struct FlashImage {
  const uint8_t *data{nullptr};
  size_t length{0};
  std::string md5;
};

class XMOSImageReader {
public:  
  virtual bool init_reader(){return true;}
  virtual bool deinit_reader(){return true;}
  
  virtual size_t get_image_size() = 0;
  virtual int read_image_block(uint8_t *buffer, size_t bock_size) = 0; 
};

class HttpImageReader : public XMOSImageReader {
public:
  HttpImageReader(http_request::HttpRequestComponent* http_request, std::string url) : http_request_(http_request), url_(url) {}
  bool init_reader() override;
  bool deinit_reader() override;

  size_t get_image_size() override {
    if (this->container_ == nullptr ){
      return 0;
    }
    return this->container_->content_length;
  }
  int read_image_block(uint8_t *buffer, size_t block_size) override;
  


protected:
  http_request::HttpRequestComponent* http_request_{nullptr};
  std::shared_ptr<esphome::http_request::HttpContainer> container_{nullptr};
  std::string url_{};
};


class EmbeddedImageReader : public XMOSImageReader {
public:
  EmbeddedImageReader(FlashImage img) : image_(img) {}   
  bool init_reader() override {this->read_pos_ = 0; return true;}
  size_t get_image_size() override { return this->image_.length; }
  
  int read_image_block(uint8_t *buffer, size_t block_size) override {
    size_t to_read = ((this->image_.length - read_pos_) >= block_size) ? block_size : this->image_.length - read_pos_;
    memcpy( buffer, this->image_.data + read_pos_, to_read );
    this->read_pos_ += to_read;
    return to_read;
  }

protected:
  FlashImage image_;
  size_t read_pos_ = 0;
};


class XMOSFlasher : public Satellite1SPIService {
public:
  uint8_t flashing_progress{0};
  XMOSFlasherState state{XMOS_FLASHER_IDLE};  
  
  bool init_flasher();
  void dump_flash_info();
  bool deinit_flasher();

  void flash_remote_image();
  void flash_embedded_image();
  
  void set_embedded_image(const uint8_t *pgm_pointer, size_t length, std::string expected_md5){
    this->embedded_image_.data = pgm_pointer;
    this->embedded_image_.length = length;
    this->embedded_image_.md5 = expected_md5;    
  }
    
  void set_http_request_component(http_request::HttpRequestComponent* http_request ){
    this->http_request_ = http_request;
  }
  
  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_expected_ = md5; }
  void set_url(const std::string &url);
  
  void add_on_state_callback(std::function<void()> &&callback) {
    this->state_callback_.add(std::move(callback));
  }

protected:
  GPIOPin* xmos_rst_n_{nullptr};

  void read_JEDECID_();
  bool enable_writing_();
  bool disable_writing_();
  bool erase_sector_(int sector);
  bool wait_while_flash_busy_(uint32_t timeout_ms);
  bool read_page_( uint32_t byte_addr, uint8_t* buffer );
  bool write_page_( uint32_t byte_addr, uint8_t* buffer );
  uint8_t _manufacturerID;
  uint8_t _memoryTypeID;
  uint8_t _capacityID;
  int32_t _capacity;

  uint8_t write_to_flash_(XMOSImageReader &reader);
  
  void set_state_(XMOSFlasherState new_state){
    this->state = new_state;
    this->state_callback_.call();
    if( new_state == XMOS_FLASHER_ERROR_STATE || new_state == XMOS_FLASHER_SUCCESS_STATE){
      this->state = XMOS_FLASHER_IDLE;
      this->state_callback_.call();
    }
  }
  
  CallbackManager<void()> state_callback_{};

  /* flashing embedded image*/
  FlashImage embedded_image_;
  
  /* flashing remote image */
  bool http_get_md5_();
  bool validate_url_(const std::string &url);
  void cleanup_(const std::shared_ptr<http_request::HttpContainer> &container);
  
  http_request::HttpRequestComponent* http_request_;
  
  std::string md5_computed_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string password_{};
  std::string username_{};
  std::string url_{};
  static const uint16_t HTTP_RECV_BUFFER = 256;
};


}
}
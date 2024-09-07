#pragma once

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/spi/spi.h"


namespace esphome {
namespace satellite1 {

static const uint8_t MD5_SIZE = 32;

enum SatelliteFlasherError : uint8_t {
  OTA_MD5_INVALID = 0x10,
  OTA_BAD_URL = 0x11,
  OTA_CONNECTION_ERROR = 0x12,
  OTA_INIT_FLASH_ERROR = 0x20,
};



class SatelliteFlasher : public ota::OTAComponent,
                         public spi::SPIDevice <spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
public:
  void setup();
  void dump_config();
  
  bool init_flasher();
  void dump_flash_info();
  bool check_crc_consistency();
  bool deinit_flasher();

  void set_http_request_component(http_request::HttpRequestComponent* http_request ){
    this->http_request_ = http_request;
  }
  void set_md5_url(const std::string &md5_url);
  void set_md5(const std::string &md5) { this->md5_expected_ = md5; }
  void set_url(const std::string &url);

  void flash();

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

  bool http_get_md5_();
  bool validate_url_(const std::string &url);
  void cleanup_(const std::shared_ptr<http_request::HttpContainer> &container);
  
  uint8_t write_to_flash_();
  
  http_request::HttpRequestComponent* http_request_;
  
  std::string md5_computed_{};
  std::string md5_expected_{};
  std::string md5_url_{};
  std::string password_{};
  std::string username_{};
  std::string url_{};
  static const uint16_t HTTP_RECV_BUFFER = 256;
};


class CRC32
{
public:
	CRC32()
		: crc(0xFFFFFFFF)
	{
		generate_table();
	}
  
    void generate_table();
    void update( const uint8_t val);
    uint32_t get_value() const { return crc; }

private:
	uint32_t table[256];
	uint32_t crc;
};


}
}
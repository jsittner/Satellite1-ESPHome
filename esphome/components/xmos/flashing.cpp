#include "flashing.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fph_xmos {

static const char *const TAG = "xmos_flasher";

void XMOSFlasher::setup() {
  
}

void XMOSFlasher::init_flasher(){
  ESP_LOGD(TAG, "Setting up XMOS flasher...");
  this->spi_setup();
  
  this->read_fpga_firmware_version();
  
}


bool XMOSFlasher::check_crc_consistency(){
/*
32-bit program size s in words
Program consisting of s × 4 bytes
A 32-bit CRC

The CRC is calculated over the byte stream represented by the program size and the program itself. The polynomial used is 0xEDB88320 (IEEE 802.3); the CRC register is initialized with 0xFFFFFFFF and the residue is inverted to produce the CRC.
*/
CRC32 crc32();

uin32_t program_size;
//read program_size s
crc32.update( (uint8_t*) &program_size, 4)
//read s * 4 bytes
// crc32.update( program )
//read 32-bit CRC 

/*

*/

}

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

void CRC32::update(const unsigned __int8 * buf, size_t len){
  uint32_t c = initial ^ 0xFFFFFFFF;
  const uint8_t* u = static_cast<const uint8_t*>(buf);
  for (size_t i = 0; i < len; ++i)
  {
    c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFF;
}

}
}
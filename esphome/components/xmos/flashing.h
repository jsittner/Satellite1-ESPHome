#pragma once

#include "esphome/components/spi/spi.h"

namespace esphome {
namespace fph_xmos {

/*
FPGA_SPI_CS:   GPIO_23
FPGA_SPI_MOSI: GPIO_33
FPGA_SPI_MISO: GPIO_21
FPGA_SPI_SCLK: GPIO_32
*/

const uint16_t CONF_BASE_ADDRESS = 0x0000;
const uint32_t MATRIX_CREATOR_ID = 0x05C344E8;
const uint32_t MATRIX_VOICE_ID = 0x6032BAD2;

struct hardware_address {
  uint8_t readnwrite : 1;
  uint16_t reg : 15;
};

class XMOSFlasher : public Component,
                    public spi::SPIDevice <spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                          spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_8MHZ> {
  public:
    void setup();
    void dump_config();

    void init_flasher();
    bool check_crc_consistency();

  protected:
    GPIOPout* xmos_rst_n_{nullptr};

};

class CRC32
{
public:
	CRC32()
		: initial(0xFFFFFFFF)
	{
		generate_table();
	}
  void generate_table();

	void update(const unsigned __int8 * buf, size_t len)
	{
		initial = crc32_s.update(table, initial, (const void *)buf, len);
	}

	uint32_t GetValue() const
	{
		return initial;
	}

private:
	uint32_t table[256];
	CRC32_s crc32_s;
	uint32_t initial;
};


}
}
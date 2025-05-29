#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include <cinttypes>
#include <memory>

#include "esphome/core/hal.h"

namespace esphome {

#pragma pack(push, 1)  // Prevent padding
/// Time value
/// Time value
typedef struct tv
{
    /// seconds
    int32_t sec;
    /// micro seconds
    int32_t usec;
    
    /// c'tor
    tv() : sec(0), usec(0) {}
    /// C'tor, construct from timeval @p tv
    explicit tv(timeval tv) : sec(tv.tv_sec), usec(tv.tv_usec){};
    /// C'tor, construct from @p _sec and @p _usec
    tv(int32_t _sec, int32_t _usec) : sec(_sec), usec(_usec){};

    static tv from_millis(int32_t millis)
    {
        tv result;
        result.sec = millis / 1000;
        result.usec = (millis % 1000) * 1000;
        return result;
    }
    
    static tv now()
    {
        tv result;
        uint32_t usec_now = micros();
        result.sec = usec_now / 1000000;
        result.usec = usec_now % 1000000;
        return result;
    }
    
    int32_t to_millis() const
    {
        return (sec * 1000) + (usec / 1000);
    }

    /// add another tv
    tv operator+(const tv& other) const
    {
        tv result(*this);
        result.sec += other.sec;
        result.usec += other.usec;
        if (result.usec > 1000000)
        {
            result.sec += result.usec / 1000000;
            result.usec %= 1000000;
        }
        return result;
    }

    /// subtract another tv
    tv operator-(const tv& other) const
    {
        tv result(*this);
        result.sec -= other.sec;
        result.usec -= other.usec;
        while (result.usec < 0)
        {
            result.sec -= 1;
            result.usec += 1000000;
        }
        return result;
    }
    tv operator/(int32_t div) const
    {
        tv result(*this);
        result.sec /= div;
        result.usec /= div;
        return result;
    }
} tv_t;

typedef struct timed_chunk
{
    tv_t stamp;
    uint8_t data[0];
} timed_chunk_t;  

#pragma pack(pop)


class TimedRingBuffer {
 public:
  ~TimedRingBuffer();

  /**
   * @brief Reads from the ring buffer, waiting up to a specified number of ticks if necessary.
   *
   * Available bytes are read into the provided data pointer. If not enough bytes are available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before reading what is available.
   *
   * @param data Pointer to copy read data into
   * @param max_len Number of bytes to read
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Number of bytes read
   */
  int32_t read(void *data, size_t max_len, TickType_t ticks_to_wait = 0);

  /**
   * @brief Reads from the ring buffer, waiting up to a specified number of ticks if necessary.
   *
   * Available bytes are read into the provided data pointer. If not enough bytes are available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before reading what is available.
   *
   * @param data Pointer to copy read data into
   * @param max_len Number of bytes to read
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Number of bytes read
   */
  int32_t read(void *data, size_t max_len, tv &stamp, TickType_t ticks_to_wait = 0);

  /**
   * @brief Writes to the ring buffer without overwriting oldest data.
   *
   * The provided data is written to the ring buffer. If not enough space is available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before writing as much as possible.
   *
   * @param data Pointer to data for writing
   * @param len Number of bytes to write
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Number of bytes written
   */
  size_t write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait = 0);

  /**
   * @brief Writes to the ring buffer without overwriting oldest data.
   *
   * The provided data is written to the ring buffer. If not enough space is available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before writing as much as possible.
   *
   * @param ppvItem [out] Double pointer to memory acquired (set to NULL if no memory were retrieved)
   * @param len Number of bytes to allocate (excluding time header)
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return pointer to allocated write_chunk
   */
  error_t acquire_write_chunk(timed_chunk_t **write_chunk, size_t len, TickType_t ticks_to_wait = 0);

  error_t release_write_chunk(timed_chunk_t *write_chunk);



  /**
   * @brief Returns the number of available bytes in the ring buffer.
   *
   * This function provides the number of bytes that can be read from the ring buffer
   * without blocking the calling FreeRTOS task.
   *
   * @return Number of available chunks
   */
  size_t chunks_available() const;

  /**
   * @brief Returns the number of free bytes in the ring buffer.
   *
   * This function provides the number of bytes that can be written to the ring buffer
   * without overwriting data or blocking the calling FreeRTOS task.
   *
   * @return Number of free bytes
   */
  size_t free() const;

  /**
   * @brief Resets the ring buffer, discarding all stored data.
   *
   * @return pdPASS if successful, pdFAIL otherwise
   */
  BaseType_t reset();

  static std::unique_ptr<TimedRingBuffer> create(size_t len);

 protected:
  /// @brief Discards data from the ring buffer.
  /// @param discard_chunks amount of chunks to discard
  /// @return True if all bytes were successfully discarded, false otherwise
  bool discard_chunks_(size_t discard_bytes);

  RingbufHandle_t handle_{nullptr};
  StaticRingbuffer_t structure_;
  uint8_t *storage_{nullptr};
  size_t size_{0};
  size_t bytes_waiting_in_chunk{0};
  timed_chunk_t *curr_chunk{nullptr}; 
};

}  // namespace esphome

#endif

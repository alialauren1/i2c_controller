#include "app.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_i2c.h"
#include "sl_sleeptimer.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define SENSOR_I2C_ADDR     0x40

#define SAMPLE_INTERVAL_MS  9         // using 9ms — satisfies 8ms minimum conversion time with 1ms margin

#define STATUS_FIXED_BIT    (1 << 6)  // always 1 on real Keller sensor
#define STATUS_BUSY_BIT     (1 << 5)  // 1 = sensor still converting
#define STATUS_MEM_ERR_BIT  (1 << 2)  // 1 = internal checksum failed

static bool sensor_init(void) // checks if sensor responds to its address being called
{ // Send a zero-length write to confirm the sensor is on the bus
  I2C_TransferSeq_TypeDef seq;
  uint8_t dummy = 0;  //0 is placeholder byte

  seq.addr        = SENSOR_I2C_ADDR << 1; // bit shift by 1 the sensor address to make room for R/W bit
  seq.flags       = I2C_FLAG_WRITE; // defined in em_i2c.h line 117, tells driver to set up write
  seq.buf[0].data = &dummy;         // Write buffer: pointer to data location (dummy since nothing sent)
  seq.buf[0].len  = 0;              //               zero bytes to send b/c only checking ACK, no data needed
  seq.buf[1].data = NULL;           // Read buffer: ignore because we're writing only here
  seq.buf[1].len  = 0;              //              also not used

  return (I2CSPM_Transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone); // runs transaction on bus and returns true if sensor ACKed
}

static bool sensor_trigger(void)
{ // Send 0xAC to start a conversion — result is ready after required millisecond duration
  I2C_TransferSeq_TypeDef seq;
  uint8_t cmd = 0xAC;

  seq.addr        = SENSOR_I2C_ADDR << 1;
  seq.flags       = I2C_FLAG_WRITE;
  seq.buf[0].data = &cmd;
  seq.buf[0].len  = 1;
  seq.buf[1].data = NULL;
  seq.buf[1].len  = 0;

  return (I2CSPM_Transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone);
}

static bool sensor_read(uint8_t *data, uint16_t len)
{ // Read conversion result — 5 bytes: [status][P_hi][P_lo][T_hi][T_lo]
  I2C_TransferSeq_TypeDef seq;

  seq.addr        = SENSOR_I2C_ADDR << 1;
  seq.flags       = I2C_FLAG_READ;
  seq.buf[0].data = data;
  seq.buf[0].len  = len;
  seq.buf[1].data = NULL;
  seq.buf[1].len  = 0;

  return (I2CSPM_Transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone);
}

static sl_sleeptimer_timer_handle_t sample_timer;
static volatile bool                tick_flag  = false;
static bool                         sensor_ok  = false;  // default until proven if true by line 35-51

static void on_sample_timer(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  tick_flag = true;
}

// this guarantees full required gap between WRITE (trigger) and READ (result) that Keller requires
static void start_sample_timer(void) // Start timer, called after every sensor_trigger()
{
  sl_sleeptimer_start_timer_ms(&sample_timer,
                               SAMPLE_INTERVAL_MS,
                               on_sample_timer,
                               NULL, 0, 0);
}

void app_init(void) // Initialize application
{
  sensor_ok = sensor_init(); // checks to see if sensor responds to address being called

  if (!sensor_ok) { // returns if sensor not ACKed
    printf("ERROR: No I2C ACK\r\n");
    return;
  }

  printf("Sensor found at 0x%02X\r\n", SENSOR_I2C_ADDR);

  // Trigger first conversion, then start millisecond timer
  sensor_trigger();     // fire the first WRITE 0xAC
  start_sample_timer(); // start the first required duration timer before first READ
}

void app_process_action(void)
{ // do nothing until both following conditions met:
  if (!sensor_ok) return; // sensor responding to address
  if (!tick_flag)  return; // tick_flag set when required time duration between W/R is done
  tick_flag = false; // reset flag to false after using it

  // Read result from previous trigger
  uint8_t raw[5] = { 0 }; // 5-byte buffer: [status][High P][Low P][High T][Low T]
  if (!sensor_read(raw, sizeof(raw))) { // try to read 5 bytes from sensor into raw
    printf("ERROR: I2C read failed\r\n");
    sensor_trigger();
    start_sample_timer();
    return;
  }

  uint8_t status = raw[0];

  if (!(status & STATUS_FIXED_BIT)) {
    printf("ERROR: Bad status byte 0x%02X — not a Keller sensor?\r\n", status);
    sensor_trigger();
    start_sample_timer();
    return;
  }

  if (status & STATUS_BUSY_BIT) {
    printf("ERROR: Sensor busy — conversion not ready\r\n");
    sensor_trigger();
    start_sample_timer();
    return;
  }

  if (status & STATUS_MEM_ERR_BIT) {
    printf("ERROR: Sensor memory error\r\n");
    sensor_trigger();
    start_sample_timer();
    return;
  }

  uint16_t pressure = (uint16_t)((raw[1] << 8) | raw[2]);  // P [u16] — unsigned 16-bit integer per data sheet
  uint16_t temp_raw = (uint16_t)((raw[3] << 8) | raw[4]);  // T [u16] — unsigned 16-bit integer per data sheet

  // Real Keller conversion formulas (0-100 bar sensor) — integer arithmetic, no float printf needed
  // intermediate cast to int64_t prevents overflow before /32768 brings result back to int32_t range
  // Pmax=100 bar hardcoded; Pmin=0x13-0x14, Pmax=0x15-0x16 stored in sensor memory (readable on startup)
  int32_t p_mbar  = (int32_t)(((int64_t)pressure - 16384) * 100000 / 32768);  // milli-bars (3 decimal places)
  int32_t t_centi = ((int32_t)(temp_raw >> 4) - 24) * 5 - 5000;    // centi-degrees C (2 decimal places)

  printf("P=%d.%03d bar  T=%d.%02d C\r\n",
         (int)(p_mbar  / 1000), (int)(p_mbar  % 1000),
         (int)(t_centi / 100),  (int)(t_centi % 100));

  // Trigger next conversion, then start timer
  // required timing gap guaranteed between this WRITE and the next READ
  sensor_trigger();
  start_sample_timer();
}

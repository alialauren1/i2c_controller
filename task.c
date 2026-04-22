/*
 * task.c
 *
 *  Created on: Apr 21, 2026
 *      Author: aliawolken
 *
 ********************************************************************************
 *
 *      The following are Micrium Specific:
 *        #include "os.h", #include "rtos_err.h",
 *        the ..._task_create() functions, OSTaskCreate(),
 *        DEF_NULL, OS_OPT_TASK_STK_CHK, OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
 *        OS_TCB, CPU_STK, RTOS_ERR
 *
 *      The following is Silicon Labs specific:
 *        sl_sleeptimer_delay_millisecond()
 *
 ********************************************************************************/


#include "os.h"
#include "rtos_err.h"
#include "task.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"
#include "em_i2c.h"
#include "sl_sleeptimer.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

//For Keller_get_pressure_task
#define SENSOR_I2C_ADDR     0x40
#define SAMPLE_INTERVAL_MS  500         // use min 8ms — satisfies 8ms minimum conversion time
#define STATUS_FIXED_BIT    (1 << 6)  // always 1 on real Keller sensor
#define STATUS_BUSY_BIT     (1 << 5)  // 1 = sensor still converting
#define STATUS_MEM_ERR_BIT  (1 << 2)  // 1 = internal checksum failed
#define P_OFFSET_MBAR 0 // calibration offset

//For Keller_get_pressure_task_create
#define KELLER_GET_PRESSURE_TASK_PRIO      11u
#define KELLER_GET_PRESSURE_TASK_STK_SIZE  256u
static CPU_STK keller_stk[KELLER_GET_PRESSURE_TASK_STK_SIZE];
static OS_TCB  keller_tcb;

//For Printing Pressure tasks
#define PRINT_PRESSURE_TASK_PRIO      12u
#define PRINT_PRESSURE_TASK_STK_SIZE  256u
static CPU_STK print_stk[PRINT_PRESSURE_TASK_STK_SIZE];
static OS_TCB  print_tcb;

//For Keller_get_pressure_task
static bool Keller_P_sensor_init(void) // checks if sensor responds to its address being called
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

//For Keller_get_pressure_task
static bool Keller_P_sensor_trigger(void)
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

//For Keller_get_pressure_task
static bool Keller_P_sensor_read(uint8_t *data, uint16_t len)
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

void keller_get_pressure_task(void *p_arg); // forward declaration
void print_pressure_task(void *p_arg); // forward declaration
//-------------------------------------------------------------------------------------------------------------

void keller_get_pressure_task_create(void) {
  RTOS_ERR err;

  OSTaskCreate(&keller_tcb,
               "Keller ACQ",
               keller_get_pressure_task,
               NULL,
               KELLER_GET_PRESSURE_TASK_PRIO,
               &keller_stk[0],
               (KELLER_GET_PRESSURE_TASK_STK_SIZE / 10u),
               KELLER_GET_PRESSURE_TASK_STK_SIZE,
               0u,
               0u,
               DEF_NULL,
               OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
               &err);
}

void keller_get_pressure_task(void *p_arg)  // Sealed Gauge Sensor, measures 1 bar absolute reference meaning zero ref is 1 bar (atm)
{
  (void)p_arg;

  // Initialize application
  bool Keller_P_sensor_ok = Keller_P_sensor_init(); // checks to see if sensor responds to address being called

  if (!Keller_P_sensor_ok) { // returns if sensor not ACKed
      printf("ERROR: No I2C ACK\r\n");
      return;}

  printf("Sensor found at 0x%02X\r\n", SENSOR_I2C_ADDR);

  bool first_loop = true; // flag to note first iteration

  while (1){

      // Read result from previous trigger
      uint8_t raw[5] = { 0 }; // 5-byte buffer: [status][High P][Low P][High T][Low T]

      bool read_P_sensor = Keller_P_sensor_read(raw,sizeof(raw)); // Read 5 bytes from sensor into raw

      Keller_P_sensor_trigger(); // Trigger next conversion (Trigger is essentially Write but it doesnt transfer data just triggers a conversion on sensor)

      if (first_loop){
          printf("first looooop\r\n");
          first_loop = false;
      }
      else if (!read_P_sensor) { // if read transaction did not succeed
        printf("ERROR: I2C read failed\r\n");
      }
      else {
          uint8_t status = raw[0];

          if (!(status & STATUS_FIXED_BIT)) {
            printf("ERROR: Bad status byte 0x%02X — not a Keller sensor?\r\n", status);
          }
          else if (status & STATUS_BUSY_BIT) {
            printf("ERROR: Sensor busy — conversion not ready\r\n");
          }
          else if (status & STATUS_MEM_ERR_BIT) {
            printf("ERROR: Sensor memory error\r\n");
          }
          else {
              uint16_t pressure = (uint16_t)((raw[1] << 8) | raw[2]);  // P [u16] — unsigned 16-bit integer per data sheet
              uint16_t temp_raw = (uint16_t)((raw[3] << 8) | raw[4]);  // T [u16] — unsigned 16-bit integer per data sheet

              // Real Keller conversion formulas (0-100 bar sensor) — integer arithmetic, no float printf needed
              // Pmax=100 bar hardcoded; Pmin=0x13-0x14, Pmax=0x15-0x16 stored in sensor memory (readable on startup)
              int32_t p_mbar  = (int32_t)(((int64_t)pressure - 16384) * 100000 / 32768);  // milli-bars (3 decimal places)
              int32_t t_centi = ((int32_t)(temp_raw >> 4) - 24) * 5 - 5000;    // centi-degrees C (2 decimal places)

              // p_mbar -= P_OFFSET_MBAR; // shorthand for replace p_mbar with p_mbar - p offset

              printf("P=%s%d.%03d bar, T=%d.%02d C\r\n",
                     p_mbar < 0 ? "-" : "",
                     (int)(abs(p_mbar) / 1000),
                     (int)(abs(p_mbar) % 1000),
                     (int)(t_centi / 100),
                     (int)(t_centi % 100));

          }}

      sl_sleeptimer_delay_millisecond(SAMPLE_INTERVAL_MS); // // required timing gap guaranteed between this WRITE and the next READ
  }
}

void print_pressure_task_create(void) {
  RTOS_ERR err;

  OSTaskCreate(&print_tcb,
               "Print",
               print_pressure_task,
               NULL,
               PRINT_PRESSURE_TASK_PRIO,
               &print_stk[0],
               (PRINT_PRESSURE_TASK_STK_SIZE / 10u),
               PRINT_PRESSURE_TASK_STK_SIZE,
               0u,
               0u,
               DEF_NULL,
               OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
               &err);
}

void print_pressure_task(void *p_arg) {
  (void)p_arg;

  while (1) {
      // drain circular buffer and printf — to be implemented
  }
}

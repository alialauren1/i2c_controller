/*
 * task.h
 *
 *  Created on: Apr 21, 2026
 *      Author: aliawolken
 */

#ifndef TASK_H_
#define TASK_H_

#define SAMPLE_RATE_HZ_DEFAULT  10 // default, allowable range is 1 Hz (1 s) to 100 Hz (0.01 sec)

void keller_get_pressure_task_create(void);
void retrieve_pressure_from_buffer_task_create(void);
void button_task_create(void);
void err_msg_task_create(void);

void stop_recording_task(void);   // stop data collection, flush and close SD
void start_recording_task(void);  // resume data collection, open new SD file
void single_read_task(void);      // trigger one pressure reading to console

void config_task(unsigned int rate_hz);

#endif /* TASK_H_ */

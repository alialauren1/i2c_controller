/*
 * task.h
 *
 *  Created on: Apr 21, 2026
 *      Author: aliawolken
 */

#ifndef TASK_H_
#define TASK_H_

void keller_get_pressure_task_create(void);
void retrieve_pressure_from_buffer_task_create(void);
void button_task_create(void);
void err_msg_task_create(void);

void stop_recording_task(void);   // stop data collection, flush and close SD
void start_recording_task(void);  // resume data collection, open new SD file
void single_read_task(void);      // trigger one pressure reading to console

#endif /* TASK_H_ */

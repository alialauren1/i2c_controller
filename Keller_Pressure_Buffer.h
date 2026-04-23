/*
 * Keller_Pressure_Buffer.h
 *
 *  Created on: Apr 23, 2026
 *      Author: aliawolken
 */

#ifndef KELLER_PRESSURE_BUFFER_H_
#define KELLER_PRESSURE_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

#define KELLER_BUFFER_SIZE 16

typedef struct {
    int32_t p_mbar;
    int32_t t_centi;
} keller_sample_t;

void Keller_buffer_init(void);
bool Keller_buffer_write(int32_t p_mbar, int32_t t_centi);
bool Keller_buffer_read(keller_sample_t *sample);
bool Keller_buffer_is_empty(void);

#endif /* KELLER_PRESSURE_BUFFER_H_ */

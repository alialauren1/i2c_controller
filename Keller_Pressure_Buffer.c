/*
 * Keller_Pressure_Buffer.c
 *
 *  Created on: Apr 23, 2026
 *      Author: aliawolken
 */


#include "Keller_Pressure_Buffer.h"

static keller_sample_t buf[KELLER_BUFFER_SIZE];
static int head  = 0;
static int tail  = 0;
static int count = 0;

void Keller_buffer_init(void) {
    head  = 0;
    tail  = 0;
    count = 0;
}

bool Keller_buffer_write(int32_t p_mbar, int32_t t_centi) {
    if (count >= KELLER_BUFFER_SIZE) {
        return false;
    }
    buf[head].p_mbar  = p_mbar;
    buf[head].t_centi = t_centi;
    head = (head + 1) % KELLER_BUFFER_SIZE;
    count++;
    return true;
}

bool Keller_buffer_read(keller_sample_t *sample) {
    if (count == 0) {
        return false;
    }
    *sample = buf[tail];
    tail = (tail + 1) % KELLER_BUFFER_SIZE;
    count--;
    return true;
}

bool Keller_buffer_is_empty(void) {
    return count == 0;
}

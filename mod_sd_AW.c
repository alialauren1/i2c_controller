/*
 * mod_sd_AW.c
 *
 *  Created on: Apr 28, 2026
 *      Author: aliawolken
 *
 *      TODO:
 *      Keep track of the location that stuff is being added to other files.
 *
 *      mod_sd.c has mod_sd_AW_open

 *      The following changes were made in other files
 *      // in config file, sl_sleeptimer_config.h, enabled: #define SL_SLEEPTIMER_WALLCLOCK_CONFIG  1
 */

#include "mod_sd_AW.h"
#include "mod_sd.h"
#include <stdio.h>

static FIL fp; // file object, accessible by all functions in this file. declares variable fp (file pointer) of type FIL (struct type)

void mod_sd_AW_open(void){
  UINT bw; // bw (bytes written) so f_write fills this in after the write
        TCHAR file_name[16]; // array for the UTF-16 encoded file path
        mod_sd_ff_encode("data.txt", file_name,8); // convert "data.txt" from char to TCHAR for FatFS

        FRESULT fres = f_open(&fp, file_name, FA_CREATE_ALWAYS | FA_WRITE); // create file, FA_CREATE_ALWAYS truncates if it already exists

        if(fres==FR_OK){
            f_write(&fp,"hello\r\n",7,&bw); // writes 7 bytes to the file, bw receives the actual bytes written
            if(bw != 7){
                printf("Write error: only %d of 7 bytes written\r\n", bw); // checks that all bytes were written to the file
            }
            else{
                printf("File wrote all bytes\r\n");
            }
            printf("File created. \r\n");
        }
        else {
            printf("File open has failed: %d\r\n",fres);
        }
}

void mod_sd_AW_close(void){
  f_close(&fp);
  f_mount(NULL, (TCHAR*)"",0);
  printf("SD card safe to remove. \r\n");
}


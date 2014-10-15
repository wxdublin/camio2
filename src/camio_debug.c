/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   15 Oct 2014
 *  File name: camio_debug.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stdio.h>
#include "camio_debug.h"

//******************************************//
//Just for debugging



ch_word camio_debug_out_(
        ch_word line_num,
        const char* filename,
        const char* format, ... ) //Intentionally using char* here as these are passed in as constants
{
    va_list args;
    va_start(args,format);
    dprintf(STDERR_FILENO,"[%s:%i] -- ", filename, (int)line_num);
    ch_word result = vdprintf(STDERR_FILENO,format,args);
    va_end(args);

    return result;
}

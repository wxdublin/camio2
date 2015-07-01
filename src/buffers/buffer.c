/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jul 1, 2015
 *  File name:  buffer.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "buffer.h"

//Reset the buffers internal pointers to point to nothing
void reset_buffer(camio_buffer_t* dst)
{
    dst->buffer_start       = NULL;
    dst->buffer_len         = 0;
    dst->data_start         = NULL;
    dst->data_len           = 0;
}


//Assign the pointers from one buffer to another
void assign_buffer(camio_buffer_t* dst, camio_buffer_t* src, void* data_start, ch_word data_len)
{
    dst->buffer_start       = src->buffer_start;
    dst->buffer_len         = src->buffer_len;
    dst->data_start         = data_start;
    dst->data_len           = data_len;
}

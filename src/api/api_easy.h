/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 12, 2015
 *  File name:  api_easy.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef API_EASY_H_
#define API_EASY_H_

#include <src/camio.h>
#include <src/devices/channel.h>

/**
 * A nice wrapper around device creation and connection. Creates a device and uses the device to produce a channel.
 * Assumes that the device will connect immidately.
 */
camio_error_t camio_easy_channel_new(char* uri, camio_channel_t** channel);

camio_error_t camio_easy_device_new(char* uri, camio_dev_t** device_o);


#endif /* API_EASY_H_ */

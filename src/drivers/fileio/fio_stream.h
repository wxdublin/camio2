/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_FIO_FIO_STREAM_H_
#define SRC_DRIVERS_FIO_FIO_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(fio);

camio_error_t fio_stream_construct(camio_stream_t* this, camio_connector_t* connector, int fd);

#endif /* SRC_DRIVERS_FIO_FIO_STREAM_H_ */

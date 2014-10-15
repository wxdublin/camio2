/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: camio_api_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the stream used
 */

#include "../src/camio.h"
#include <stdio.h>
#include "../src/camio_debug.h"

USE_CAMIO;

int main(int argc, char** argv)
{

    (void)argc; //We don't use these for the test ... yet
    (void)argv;

    camio_connector_t* connector1 = NULL;
    camio_connector_t* connector2 = NULL;

    new_camio_transport_str("netm:/dev/netmap/",NULL,&connector1);
    DBG("Got new connector at address %p\n", connector1);

    new_camio_transport_bin("netm",NULL,&connector2);


    DBG("Got new connector at address %p\n", connector2);

    /*
    camio_stream_t* stream = NULL;
    while(!camio_connect(connector,&stream)){
        //Just spin waiting for a connection
    }

    camio_wr_buffer_t* wr_buffer_chain;
    ch_word count = 1;
    while(!camio_write_acquire(stream, &wr_buffer_chain, &count)){
        //Just spin waiting for a new write buffer
    }

    if(count != 1){
        //Shit, we should have got this!
    }


    while(1){

        camio_rd_buffer_t* rd_buffer_chain;
        while(camio_read_acquire(stream, &rd_buffer_chain, 0 , 0)){
            //Just spin waiting for a new read buffer
        }

        //Do a copy here

        camio_write_commit(stream, wr_buffer_chain, 0, 0 );

        camio_read_release(stream, rd_buffer_chain);
    }

    camio_write_release(stream,wr_buffer_chain);
    */


    return 0;
}

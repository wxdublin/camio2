/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 25, 2015
 *  File name:  camio_perf_client.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stdio.h>

#include <deps/chaste/chaste.h>

#include <src/api/api_easy.h>
#include <src/camio_debug.h>
#include <src/drivers/delimiter/delim_device.h>
#include "camio_perf_packet.h"

#include "options.h"
extern struct options_t options;

#define CONNECTOR_ID 0


static ch_word delimit(char* buffer, ch_word len)
{
    if((size_t)len < sizeof(camio_perf_packet_head_t)){
        return -1;
    }

    camio_perf_packet_head_t* head = (camio_perf_packet_head_t*)buffer;
    if(len < head->size){
        return -1;
    }

    return head->size;
}

static camio_error_t connect_delim(ch_cstr client_channel_uri, camio_controller_t** controller) {

    //Find the ID of the delim channel
    ch_word id;
    camio_error_t err = camio_device_get_id("delim", &id);

    //Fill in the delim channel paramters structure
    delim_params_t delim_params = {
            .base_uri = client_channel_uri,
            .delim_fn = delimit
    };
    ch_word params_size = sizeof(delim_params_t);
    void* params = &delim_params;

    //Use the parameters structure to construct a new controller object
    err = camio_device_constr(id, &params, params_size, controller);
    if (err) {
        DBG("Could not construct controller\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    //And we're done!
    DBG("Got controller\n");
    return CAMIO_ENOERROR;
}

static camio_wr_req_t wreq;
static camio_error_t send_message(camio_muxable_t* muxable, ch_word ts, ch_word seq)
{
    camio_error_t err = CAMIO_ENOERROR;
    DBG("Current req_buffer=%p\n", wreq.buffer);
    if(!wreq.buffer){
        //Get and init a buffer for writing stuff to. Hang on to it if possible
        DBG("No write buffer, grabbing a new one parent=%p, buffer=%p\n", muxable->parent.channel, &wreq.buffer);
        err = camio_write_acquire(muxable->parent.channel, &wreq.buffer);
        if(err){
            ERR("Could not get a writing buffer to channel\n");
            return err;
            //return CAMIO_EINVALID; /*TODO XXX put a better error here*/
        }

        //Figure out how much we should send. -- Just some basic sanity checking
        const ch_word req_bytes     = MAX((size_t)options.len, sizeof(camio_perf_packet_head_t));
        const ch_word bytes_to_send = MIN(req_bytes,wreq.buffer->data_len);

        //Initialize the packet with some non zero junk
        for(int i = 0; i < bytes_to_send; i++){
            *((char*)wreq.buffer->data_start + i) = i % 27 + 'A';
        }
    }

    const ch_word req_bytes     = MAX((size_t)options.len, sizeof(camio_perf_packet_head_t));
    const ch_word bytes_to_send = MIN(req_bytes,wreq.buffer->data_len);
    DBG("Trying to send %lli from request of %lli bytes (Min req=%lli / data len=%lli)\n", bytes_to_send, req_bytes, req_bytes, wreq.buffer->data_len);

    camio_perf_packet_head_t* head = wreq.buffer->data_start;
    head->size = bytes_to_send;
    wreq.buffer->data_len = bytes_to_send;
    head->seq_number = seq;
    head->time_stamp = ts;

    wreq.src_offset_hint = CAMIO_WRITE_REQ_SRC_OFFSET_NONE;
    wreq.dst_offset_hint = CAMIO_WRITE_REQ_DST_OFFSET_NONE;


    DBG("Write request buffer %p of size %lli with data at %p\n",
            wreq.buffer,
            wreq.buffer->data_len,
            wreq.buffer->data_start
    );

    err = camio_write_request(muxable->parent.channel,&wreq,1 );
    if(err != CAMIO_ENOERROR && err != CAMIO_ETRYAGAIN){
        ERR("Unexpected write error %lli\n", err);
        return err;
    }

    return CAMIO_ENOERROR;
}

int camio_perf_clinet(ch_cstr client_channel_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf client\n");
    //Create a new multiplexer for channels to go into
    camio_mux_t* mux = NULL;
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Construct a delimiter
    camio_controller_t* controller = NULL;
    err = connect_delim(client_channel_uri, &controller);
    if(err){
        DBG("Could not create delimiter!\n");
        return CAMIO_EINVALID;
    }

    //Insert the controller into the mux
    err = camio_mux_insert(mux,&controller->muxable, CONNECTOR_ID);
    if(err){
        DBG("Could not insert controller into multiplexer\n");
        return CAMIO_EINVALID;
    }

    struct timeval now = {0};
    gettimeofday(&now, NULL);

    ch_word time_start_ns       = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
    ch_word time_int_start_ns   = time_start_ns;
    ch_word time_now_ns         = 0;
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word intv_bytes          = 0;
    ch_word inflight_bytes      = 0;

    DBG("Staring main loop\n");
    camio_muxable_t* muxable     = NULL;
    ch_word which                = 0;
    ch_word seq                  = 0;

    while(!*stop){
        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        if(time_now_ns >= time_int_start_ns + time_interval_ns){
            time_int_start_ns       = time_now_ns;
            total_bytes             += intv_bytes;
            ch_word total_time_ns   = time_now_ns - time_start_ns;
            double inst_rate_mbs    = ((double)intv_bytes / (double)time_interval_ns) * 1000;
            double ave_rate_mbs     = ((double)total_bytes / (double)total_time_ns) * 1000;

            printf("#### [%lli - %lli] Bytes sent=%lli, inst rate=%3.3fMbs, total_bytes sent=%lli, average rate=%3.3fMBs\n",
                time_start_ns,
                time_now_ns,
                intv_bytes,
                inst_rate_mbs * 8,
                total_bytes,
                ave_rate_mbs
            );

            intv_bytes  = 0;

        }

        //Block waiting for a channel to be ready to read or connect
        camio_mux_select(mux,&muxable,&which);

        //Have a look at what we have
        switch(muxable->mode){
            //This is a controller that's just fired
            case CAMIO_MUX_MODE_CONNECT:{
                DBG("Handling connect ready()\n");
                camio_channel_t* tmp_channel = NULL;
                err = camio_connect(muxable->parent.controller,&tmp_channel);

                if(err == CAMIO_EALLREADYCONNECTED){ //No more connections possible
                    DBG("Removing expired controller\n");
                    camio_mux_remove(mux,muxable);
                    continue;
                }
                if(err != CAMIO_ENOERROR){
                    ERR("Could not get channel. Got errr %i\n", err);
                    return CAMIO_EINVALID;
                }

                camio_mux_insert(mux,&tmp_channel->rd_muxable,CONNECTOR_ID + 1);
                camio_mux_insert(mux,&tmp_channel->wr_muxable,CONNECTOR_ID + 3);

                //Kick things off by sending the first message
                send_message(&tmp_channel->wr_muxable,time_now_ns,seq++);
                inflight_bytes = wreq.buffer->data_len;
                DBG("Sending %lli bytes\n", inflight_bytes);

                break;
            }
            case CAMIO_MUX_MODE_READ:{
                ERR("Handling read ready() -- I didn't expect this...\n");
                break;
            }
            case CAMIO_MUX_MODE_WRITE:{
                DBG("Handling write ready()\n");
                DBG("Sent %lli bytes\n", inflight_bytes);
                intv_bytes += inflight_bytes;
                send_message(muxable,time_now_ns,seq++);
                break;
            }
            default:{
                ERR("Um? What\n");
            }
        }
    }

//    if(wr_buffer){
//        camio_write_release(tmp_channel,&wr_buffer);
//    }


    return 0;

}

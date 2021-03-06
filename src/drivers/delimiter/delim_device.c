/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <deps/chaste/utils/debug.h>
#include <src/types/device_params_vec.h>

#include "delim_device.h"
#include "delim_device.h"

#include <src/devices/device.h>
#include <deps/chaste/utils/debug.h>

#include "delim_device.h"
#include "delim_channel.h"

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
typedef struct delim_priv_s {
    //Parameters used when a connection happens
    delim_params_t* params;

    //Properties of the base channel
    camio_dev_t* base;
    void* base_params;
    ch_word base_params_size;
    ch_word base_id;

} delim_device_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/
camio_error_t delim_channel_request( camio_dev_t* this, camio_msg_t* req_vec, ch_word* vec_len_io)
{
    DBG("Doing delim channel request...!\n");

    //Simply forward the request to the base device.
    delim_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    const camio_error_t err = camio_ctrl_chan_req(priv->base,req_vec,vec_len_io);

    DBG("Delim channel request done - %lli requests added\n", *vec_len_io);
    return err;
}


static camio_error_t delim_channel_ready(camio_muxable_t* this)
{
    delim_device_priv_t* priv = DEVICE_GET_PRIVATE(this->parent.dev);
    DBG("Doing channel ready\n");

    //Simply forward the ready to the base device.
    const camio_error_t err = camio_ctrl_chan_ready(priv->base);

    return err;
}



static camio_error_t delim_channel_result(camio_dev_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    delim_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    DBG("Getting result from base device\n");
    camio_error_t err = camio_ctrl_chan_res(priv->base,res_vec,vec_len_io);
    if(unlikely(err)){
        DBG("Base device returned error=%i\n", err);
        return err;
    }
    if(unlikely(0 == *vec_len_io)){
        DGB("WTF? Base device returned no new channels and no error\n");
        return CAMIO_EINVALID; //TODO XXX -- better error code
    }
    DBG("Base device returned %lli new channels\n", *vec_len_io);

    //Iterate over the channels returned, and replace them with delimiter channels
    ch_word channels = 0;
    for(ch_word i = 0; i < *vec_len_io; i++){
        //Do a bunch of sanity checking -- should probably compile this stuff out for high speed runtime
        if(unlikely(res_vec[i].type == CAMIO_MSG_TYPE_IGNORE)){
            DBG("Ignoring message at index %lli\n", i);
            continue;
        }
        if(unlikely(res_vec[i].type != CAMIO_MSG_TYPE_CHAN_RES)){
            DBG("Expected a channel response message (%i) at index %lli but got %i\n",
                    CAMIO_MSG_TYPE_CHAN_RES, i, res_vec[i].type);
            continue;
        }
        camio_chan_res_t res = &res_vec[i].ch_res;
        if(unlikely(!res.channel)){
            DBG("Expected a channel response, but got NULL and no error?\n");
            res->status = CAMIO_EINVALID;
            continue;
        }

        //OK. We can be pretty sure that we have a valid channel from the base device. Try to make a delimiter for it.
        DBG("Making new delimiter channel at index %lli\n", i);
        camio_channel_t* delim_channel = NEW_CHANNEL(delim);
        if(unlikely(!delim_channel)){
            ERR("No memory for new delimiter\n"); //EEk! Ok, bug out
            camio_channel_destroy(res->channel);
            res->status = CAMIO_ENOMEM;
            continue;
        }

        //Nice, we have memory and a delimiter channel. Set everything up for it
        DBG("Done Constructing delimiter with delimfn=%p\n", priv->params->delim_fn);
        err = delim_channel_construct(delim_channel,this,res->channel, priv->params->delim_fn, priv->params->rd_buffs);
        if(unlieky(err)){
            DBG("Unexpected error construction delimiter\n");
            res->status = err;
            continue;
        }

        //Now that the delimiter channel is looking after the base channel, we can do the old switch-a-roo
        res->channel = delim_channel;
        res->status = CAMIO_ENOERROR;
        channels++;
    }

    DBG("Delimiter created %lli channels from %lli responese\n", channels, *vec_len_io);
    *vec_len_io = channels;
    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t delim_construct(camio_dev_t* this, void** params, ch_word params_size)
{
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(delim_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO DELIM : Need better error values
    }

    //Sanity check the paramters
    delim_params_t* delim_params = (delim_params_t*)(*params);
    if(delim_params->delim_fn == NULL){
        DBG("Delimiter function is not populated. Cannot continue!\n");
        return CAMIO_EINVALID; //TODO XXX -- better errors
    }
    if(delim_params->rd_buffs <= 0){
        DBG("Read buffers count must be > 0 Cannot continue!\n");
        return CAMIO_EINVALID; //TODO XXX -- better errors
    }

    //Populate the parameters
    delim_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    priv->params = delim_params;

    //OK try to construct the base channel device...
    camio_error_t err = camio_device_params_new(
        priv->params->base_uri,
        &priv->base_params,
        &priv->base_params_size,
        &priv->base_id
    );
    if(unlikely(err)){
        DBG("Uh ohh, got %lli error trying to make base device parameters\n",err);
        return err;
    }

    err = camio_device_constr(priv->base_id,&priv->base_params,priv->base_params_size,&priv->base);
    if(unlikely(err)){
        DBG("Uh ohh, got %lli error trying to construct base device\n",err);
        return err;
    }

    this->muxable.fd = priv->base->muxable.fd; //Pass this out, but retain the ready function.
                                               //Smells a bit bad, but is probably ok.

    DBG("Finished constructing delimiter device and base device\n");
    return CAMIO_ENOERROR;
}


static void delim_destroy(camio_dev_t* this)
{
    DBG("Destroying delim device...\n");
    delim_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    if(priv->base){ camio_device_destroy(priv->base); }
    if(priv->params) { free(priv->params); }
    free(this);
    DBG("Done destroying delim device structure\n");
}

NEW_DEVICE_DEFINE(delim, delim_device_priv_t)


void delim_init()
{

    DBG("Initializing delim...\n");

    //No params to add. The delimiter should only be used in binary form!
    register_new_device(scheme,strlen(scheme),0,NEW_DEVICE(delim),0,NULL,0);
    DBG("Initializing delim...Done\n");
}

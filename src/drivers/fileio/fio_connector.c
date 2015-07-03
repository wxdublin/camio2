/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <sys/stat.h>

#include <src/transports/connector.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "fio_transport.h"
#include "fio_connector.h"
#include "fio_stream.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct fio_priv_s {

    fio_params_t* params;  //Parameters used when a connection happens

    bool is_connected;          //Has connect be called?
    int base_rd_fd;             //File descriptor for reading
    int base_wr_fd;             //File descriptor for writing

} fio_connector_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible. With UDP, it is always possible.
static camio_error_t fio_connect_peek(camio_connector_t* this)
{
    DBG("Doing connect peek\n");
    fio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->base_rd_fd > -1 && priv->base_wr_fd > -1){
        return CAMIO_ENOERROR; //Ready to go, please call connect!
    }

    //Open up the file and get it ready to connect
    //This is the mode that the user has requested
    int mode = O_RDWR;
        mode = priv->params->rd_only ? O_RDONLY : mode;
        mode = priv->params->wr_only ? O_WRONLY : mode;

     //But ... if the read file is already open, then write only is the only possibility
    if(priv->base_rd_fd > -1){
        mode = O_WRONLY;
    }
    //And ... if the write file is already open, then read only is the only possibility
    if(priv->base_wr_fd > -1){
        mode = O_RDONLY;
    }

    //OK, now open the file
    int tmp_fd = open(priv->params->hierarchical.str, mode);
    if(tmp_fd < 0){
        ERR("Could not open file \"%s\". Error=%s\n", priv->params->hierarchical.str, strerror(errno));
        return CAMIO_EINVALID;
    }

    //And assign the descriptors
    if(priv->base_rd_fd < 0){
        priv->base_rd_fd = tmp_fd;
    }
    if(priv->base_wr_fd < 0){
        priv->base_wr_fd = tmp_fd;
    }


//    //Get the file size
//    struct stat st;
//    stat(priv->params->hierarchical.str, &st);
//    DBG("Got file size of %lli\n", st.st_size);

    return CAMIO_ENOERROR;

}

static camio_error_t fio_connector_ready(camio_muxable_t* this)
{
    if(this->fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = fio_connect_peek(this->parent.connector);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t fio_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    fio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = fio_connect_peek(this);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    if(priv->is_connected){
        ERR("Already connected! Why are you calling this twice?\n");
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    //DBG("Done connecting, now constructing UDP stream...\n");
    camio_stream_t* stream = NEW_STREAM(fio);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = fio_stream_construct(stream, this, priv->params, priv->base_rd_fd, priv->base_wr_fd);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t fio_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    fio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(fio_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    fio_params_t* fio_params = (fio_params_t*)(*params);
    priv->params = fio_params;

    //We must have a file descriptor or a file name
    if(fio_params->hierarchical.str_len == 0 && fio_params->rd_fd < 0 && fio_params->wr_fd < 0){
        ERR("Expecting either a file name or a file descriptor. You have supplied neither try fio:filename\n");
        return CAMIO_EINVALID;
    }
    if(fio_params->hierarchical.str_len > 0 && fio_params->rd_fd >-1 && fio_params->wr_fd >-1){
        ERR("Expecting either a file name or a file descriptor. You have supplied both!");
        return CAMIO_EINVALID;
    }

    if(fio_params->rd_only && fio_params->wr_only){
        ERR("File I/O cannot be read only and write only at the same time\n");
        return CAMIO_EINVALID;
    }

    //If the user has passed in a file descriptor, let's use that, otherwise we'll use the file name later in connect
    priv->base_rd_fd = -1;
    priv->base_wr_fd = -1;
    if(fio_params->rd_fd > -1){
        if(fio_params->wr_only){
            ERR("A read file descriptor is supplied, but write only is set.\n");
            return CAMIO_EINVALID;
        }
        priv->base_rd_fd = fio_params->rd_fd;
    }
    if(fio_params->wr_fd > -1){
        if(fio_params->rd_only){
            ERR("A write file descriptor is supplied, but read only is set.\n");
            return CAMIO_EINVALID;
        }
        priv->base_wr_fd = fio_params->wr_fd;
    }

    //Populate the rest of the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = fio_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void fio_destroy(camio_connector_t* this)
{
    DBG("Destorying fio connector\n");
    fio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(fio, fio_connector_priv_t)

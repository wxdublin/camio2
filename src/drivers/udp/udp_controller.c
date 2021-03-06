/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../devices/device.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include <src/buffers/buffer_malloc_linear.h>

#include "udp_device.h"
#include "udp_device.h"
#include "udp_channel.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <memory.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
#define CAMIO_UDP_MAX_ADDR_STR 1024
#define CAMIO_UDP_MAX_PROT_STR 10
typedef struct udp_priv_s {

    //Parameters used when a connection happens
    udp_params_t* params;

    //Local state for each socket
    int rd_fd;
    int wr_fd; //Hmmm. It's bad to have these duplicated here. These should be removed.

    //Has connect be called?
    bool is_connected;

} udp_device_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

static camio_error_t resolve_bind_connect(char* address, char* prot, ch_bool do_bind, ch_bool do_connect,
        int* socket_fd_out)
{
    struct addrinfo hints, *res, *res0;
    int s;
    char* cause = "";
    int error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; //This could be more general in the future if the code is moved out of the UDP channel
    error = getaddrinfo(address, prot, &hints, &res0);
    if (error) {
        ERR("Getaddrinfo() failed: %s\n", gai_strerror(error));
        return CAMIO_EBADOPT;
    }
    s = -1;
    for (res = res0; res; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0) {
            cause = "socket";
            continue;
        }

        if(do_connect){
            if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
                cause = "connect";
                close(s);
                s = -1;
                continue;
            }
        }

        if(do_bind){
            if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
                cause = "connect";
                close(s);
                s = -1;
                continue;
            }
        }

        break;  /* okay we got one */
    }
    if (s < 0) {
        ERR("Socket failed: %s\n", cause);
        (void)cause; //For when debug is compiled out
        return CAMIO_EINVALID;
    }

    //If we get here, s is populated with something meaningful
    *socket_fd_out = s;

    DBG("Done %s to address %s with protocol %s\n", do_bind ? "binding" : "connecting", address, prot);

    return CAMIO_ENOERROR;
}

//Try to see if connecting is possible. With UDP, it is always possible.
static camio_error_t udp_connect_peek(udp_device_priv_t* priv)
{
    if(priv->is_connected){
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    if(priv->rd_fd > -1 || priv->wr_fd > -1){
        return CAMIO_ENOERROR; //Ready to go, please call connect!
    }

    //Parse up the address and port/protocol
    if(priv->params->rd_address.str && priv->params->rd_protocol.str){
        if(resolve_bind_connect(priv->params->rd_address.str,priv->params->rd_protocol.str,true,false, &priv->rd_fd)){
            return CAMIO_EINVALID; //We cannot connect something went wrong. //TODO XXX better error code
        }
    }

    if(priv->params->wr_address.str && priv->params->wr_protocol.str){
        if(resolve_bind_connect(priv->params->wr_address.str,priv->params->wr_protocol.str,false, true, &priv->wr_fd)){
            if(priv->rd_fd){ //Tear down the whole world.
                close(priv->rd_fd > -1);
                priv->rd_fd = -1;
            }
            return CAMIO_EINVALID; //We cannot connect something went wrong. //TODO XXX better error code
        }
    }

    return CAMIO_ENOERROR;
}

static camio_error_t udp_device_ready(camio_muxable_t* this)
{
    udp_device_priv_t* priv = DEVICE_GET_PRIVATE(this->parent.dev);
    if(priv->rd_fd > -1 || priv->wr_fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = udp_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t udp_connect(camio_dev_t* this, camio_channel_t** channel_o )
{
    udp_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    camio_error_t err = udp_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }
    //DBG("Done connecting, now constructing UDP channel...\n");

    camio_channel_t* channel = NEW_CHANNEL(udp);
    if(!channel){
        *channel_o = NULL;
        return CAMIO_ENOMEM;
    }
    *channel_o = channel;

    err = udp_channel_construct(channel, this, priv->params, priv->rd_fd, priv->wr_fd);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t udp_construct(camio_dev_t* this, void** params, ch_word params_size)
{

    udp_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(udp_params_t)){
        ERR("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    udp_params_t* udp_params = (udp_params_t*)(*params);
    DBG("Constructing UDP with parameters: hier=%s, rd_add=%s, rd_prot=%s, wr_addr=%s, wr_pro=%s\n",
            udp_params->hierarchical.str,
            udp_params->rd_address.str,
            udp_params->rd_protocol.str,
            udp_params->wr_address.str,
            udp_params->wr_protocol.str
    );

    if( udp_params->rd_address.str_len  &&
        udp_params->rd_protocol.str_len &&
        udp_params->wr_address.str_len  &&
        udp_params->wr_protocol.str_len &&
        udp_params->hierarchical.str_len ){
        ERR("A hierarchical part was supplied, but is not needed because options were supplied too.\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }


    if( udp_params->rd_address.str_len  == 0 ||
        udp_params->rd_protocol.str_len == 0 ||
        udp_params->wr_address.str_len  == 0 ||
        udp_params->wr_protocol.str_len == 0 ){ //We're missing info that we need. See if we can get it

        //We do require a hierarchical part!
        if(udp_params->hierarchical.str_len == 0){
            ERR("Expecting a hierarchical part in the UDP URI, but none was given. e.g udp:localhost:2000\n");
            return CAMIO_EINVALID; //TODO XXX : Need better error values
        }

        //OK. we've got one, go looking for a protocol mark
        const char* protocol_mark   = strchr(udp_params->hierarchical.str, ':');
        const ch_word protocol_len = udp_params->hierarchical.str_len - (protocol_mark - udp_params->hierarchical.str + 1);
        ch_word address_len  = 0;
        if(protocol_mark){
            address_len = protocol_mark - udp_params->hierarchical.str ;
        }
        else{
            address_len = udp_params->hierarchical.str_len;
        }
        DBG("Protocol len = %i address len = %i\n", protocol_len, address_len);

        //Copy the addresses if we need them
        if(udp_params->rd_address.str_len == 0){
            strncpy(udp_params->rd_address.str, udp_params->hierarchical.str, address_len );
            udp_params->rd_address.str_len = address_len;
        }

        //Copy the addresses if we need them
        if(udp_params->wr_address.str_len == 0){
            strncpy(udp_params->wr_address.str, udp_params->hierarchical.str, address_len );
            udp_params->wr_address.str_len = address_len;
        }

        //Copy the protocols if we have them
        if( (udp_params->rd_protocol.str_len == 0 || udp_params->wr_protocol.str_len == 0) && protocol_mark == NULL){
            ERR("Expecting a protocol mark in the UDP URI, but none was given. e.g udp:localhost:2000\n");
            return CAMIO_EINVALID; //TODO XXX : Need better error values
        }

        if(udp_params->rd_protocol.str_len == 0){
            strncpy(udp_params->rd_protocol.str, protocol_mark + 1, protocol_len );
            udp_params->rd_protocol.str_len = protocol_len;
        }

        if(udp_params->wr_protocol.str_len == 0){
            strncpy(udp_params->wr_protocol.str, protocol_mark + 1, protocol_len );
            udp_params->wr_protocol.str_len = protocol_len;
        }
    }

    //Populate the parameters
    priv->params                    = udp_params;

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.dev  = this;
    this->muxable.vtable.ready      = udp_device_ready;
    this->muxable.fd                = -1;

    //Populate the descriptors
    priv->rd_fd = -1;
    priv->wr_fd = -1;

    return CAMIO_ENOERROR;
}


static void udp_destroy(camio_dev_t* this)
{
    DBG("Destorying udp device\n");
    udp_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

// Don't free these! The channel relies on them!!
//    if(priv->rd_fd)  { close(priv->rd_fd); }
//    if(priv->wr_fd)  { close(priv->wr_fd); }
//    DBG("Freed FD's\n");

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed device structure\n");
}

NEW_DEVICE_DEFINE(udp, udp_device_priv_t)

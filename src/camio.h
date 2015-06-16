/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: camio.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef CAMIO_H_
#define CAMIO_H_

#include <src/api/api.h>
#include <stdint.h>
#include <src/types/transport_state_vec.h>
#include <src/types/transport_params_vec.h>


//Container for all CamIO state
typedef struct camio_s {
    ch_bool is_initialized;                            //Has the CamIO state been initialized?
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state; //Container for all of the transports to be registered. For the moment
                                                       //TODO XXX: this is a vector, but it should be a hashmap for faster
                                                       //lookup times. It's not currently expected that transport lookup
                                                       //will be a major bottleneck in system performance since the number
                                                       //of transports will be relatively small (<1000) and the lookups to
                                                       //mappings between string and binary representations will be
                                                       //infrequent. Revisit if these assumptions change.
} camio_t;

extern camio_t __camio_state_container;

#define USE_CAMIO camio_t __camio_state_container = { 0 }

/**
 * Initialize the CamIO per-process system state
 */
camio_t* init_camio();



/**
 * Transports call this function to register themselves into the CamIO system. The transport registers itself using a short,
 * unique "scheme" name. For example, a UDP transport might use the scheme name "udp" and a text file transport might use
 * "txt". Registration includes passing a description of the options that the stream will take, and a description of the
 * offsets into the a stream specific options structure where those options can be found.
 */
camio_error_t register_new_transport(
    ch_ccstr scheme,
    ch_word scheme_len,
    ch_word param_struct_hier_offset,
    camio_construct_f construct,
    ch_word param_struct_size,
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params,
    ch_word global_store_size
);


/**
 * Construct and populate a transport specific parameters structure using a string URI representation of the transport. This
 * is an easy and flexible way to get parameters into the transport. Especially directly from the command line. If successful,
 * the pointer to "params" will point to a populated parameter structure of size "params_size" and the id_o parameter will
 * contain the scheme ID.
 * TODO XXX: Add features checking somewhere: camio_transport_features_t* features. Not sure where the right place for this
 * is. I think this function might be the right place, so that features required by the app can be checked against the
 * transport description as supplied by the uri string, but this means that the binary interface does not have it. Hmm.
 */
camio_error_t camio_transport_params_new( ch_cstr uri, void** params_o, ch_word* params_size_o, ch_word* id_o );


/**
 * Construct a new CamIO transport with the given ID, from the parameters structure as given. Return a connector object for
 * connecting to underlying data streams.
 */
camio_error_t camio_transport_constr(ch_word id, void** params, ch_word params_size, camio_connector_t** connector_o);


/**
 * Take a CamIO scheme name string and translate it into the scheme ID. This will be useful if you plan to manually construct
 * a CamIO transport opts structure.
 */
camio_error_t camio_transport_get_id( ch_cstr scheme_name, ch_word* id_o);


/**
 * Some transports will need to coordinate if there are multiple instances of the same transport instantiated. For these
 * transports, a global store is provided. Use this function to get a pointer to the global store given the scheme name.
 */
camio_error_t camio_transport_get_global(ch_ccstr scheme, void** global_store);


/**
 * Construct a new CamIO selector to multiplex different transports together. Provide a hint as to the optimization criterion
 * that should be applied. TODO, this should probably be generalized much the same as above so that selectors can dynamically
 * register themselves. Will do another time.
 */
typedef enum {
    CAMIO_MUX_HINT_PERFORMANCE, //Prefer higher performance, this may involve spinning the CPU
    CAMIO_MUX_HINT_ENERGY,      //Prefer lower energy use, this may involve sleeping the application
} camio_mux_hint_e;
camio_error_t camio_mux_new( camio_mux_hint_e hint, camio_mux_t** mux_o);

#endif /* CAMIO_H_ */

/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:    Jun 6, 2015
 *  File name:  transport_opts_vec.c
 *  Description:
 *  Vector description for transport options
 */

#include "transport_opts_vec.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_define_template.h"


define_ch_vector(CAMIO_TRANSPORT_OPT_VEC,camio_transport_opt_t)

//Don't bother to define this yet.
//define_ch_vector_compare(CAMIO_TRANSPORT_OPT_VEC,camio_transport_opt_t)
//{
//    return
//}


//TODO XXX!!! There is a confusion here between "parameter" and "option" should rename everything called "option" to
//"parameter"
//These applications of the macro expand out the actual option adder functions
define_add_opt(int64_t, CAMIO_TRANSPORT_OPT_TYPE_INT64)
define_add_opt(uint64_t, CAMIO_TRANSPORT_OPT_TYPE_UINT64)
define_add_opt(double, CAMIO_TRANSPORT_OPT_TYPE_DOUBLE)
define_add_opt(ch_cstr, CAMIO_TRANSPORT_OPT_TYPE_STRING)

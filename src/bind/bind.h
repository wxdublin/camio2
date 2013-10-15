// CamIO 2: bind.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef BIND_H_
#define BIND_H_

#include "../iostreams/ciostream.h"

/**
 * Associate two streams with each other. This means that CamIO will try to share buffers between the streams. This may not
 * always be possible. In these cases, CamIO will implicitly copy between buffers during a read operation. The return status
 * of bind will indicate if implicit copying is to be expected. Can be called for any two input streams, and any two output
 * streams. However, an input stream can only be directly associated with 1 output stream.  Return values:
 * - ENOERROR: Bind was successful, the two streams now share a buffer.
 * - EMUSTCOPY: Bind was unsuccessful
 * - ETOOMANY: This stream was already bound and cannot be bound again.
 *
 */

int bind_rr(ciostr* this, ciostr* that);
int bind_ww(ciostr* this, ciostr* that);
int bind_rw(ciostr* this, ciostr* that);

int unbind_rr(ciostr* this, ciostr* that);
int unbind_ww(ciostr* this, ciostr* that);
int unbind_rw(ciostr* this, ciostr* that);

#endif /* BIND_H_ */

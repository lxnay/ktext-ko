/*
 * ktext_object.h
 *
 * Copyright (C) 2011 Fabio Erculiani
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef KTEXT_OBJECT_H_
#define KTEXT_OBJECT_H_

#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include "ktext_config.h"

/**
 * struct ktext_object_node -	Linux list_head node object.
 * 								Opaque object.
 */
typedef struct ktext_object_node ktext_object_node_t;

/**
 * struct ktext_object -	the ktree FIFO object implemented with
 * 							Kernel lists. Opaque object.
 */
typedef struct ktext_object ktext_object_t;

/**
 * ktext_object_init() - initialize a previously allocated
 * 						 ktext_object_t.
 *
 * @k:	the ktext_object_t object
 *
 */
int __must_check
ktext_object_init(ktext_object_t **k);

/**
 * ktext_object_destroy() - deinitialize a previously initialized
 * 							ktext_object_t.
 *
 * @k:	the ktext_object_t object
 *
 */
void
ktext_object_destroy(ktext_object_t **k);

/**
 * ktext_object_node_init() -	initialize a previously allocated
 * 						 		ktext_object_node_t
 *
 * @n:		the ktext_object_node_t object
 * @text:	the text to attach to this ktext_object_node_t
 *
 */
void
ktext_object_node_init(ktext_object_node_t *n, char *text);

/**
 * ktext_object_node_destroy() -	deinitialize a previously
 * 									initialized ktext_object_node_t
 *
 * @n:	the ktext_object_node_t object
 *
 */
void
ktext_object_node_destroy(ktext_object_node_t *n);

/**
 * ktext_push_allowed() - is there space left on the FIFO?
 *
 * @k:				the ktext_object_t object
 * @max_elements:	maximum amount of elements allowed
 *
 * This function returns true if the ktext_object_t has
 * space for another text string.
 *
 */
int __must_check
ktext_push_allowed(ktext_object_t *k, int max_elements);

/**
 * ktext_push() - push a string to the FIFO
 *
 * @k:		the ktext_object_t object
 * @text:	the actual string
 * @count:	the size of the buffer at @text
 *
 * Push a single string to the FIFO at @k.
 * @count is the buffer size of @text (also accounting
 * the NULL termination).
 * Please note that ktext_push() shall be called only
 * after having checked text availability with
 * ktext_push_allowed().
 * You are responsible of avoiding the Test-And-Set race.
 *
 * Returns >0 for true, 0 for false, <0 for error.
 */
int __must_check
ktext_push(ktext_object_t *k, char *text, size_t count);

/**
 * ktext_pop() - extract one string from the FIFO
 *
 * @k: 		the ktext_object_t object
 * @text:	the text pointer to write to (NULL if nothing to write)
 *
 * Extract a single string from the FIFO.
 *
 */
int __must_check
ktext_pop(ktext_object_t *k, char **text);

/**
 * ktext_empty() - empty the FIFO, releasing all the text objects in it.
 *
 * @k: 	the ktext_object_t object
 *
 *
 */
void
ktext_empty(ktext_object_t *k);

/**
 * ktext_reader_trylock() - try to acquire a reader lock
 * 							on ktext_object_t
 *
 * @k: 	the ktext_object_t object
 *
 * Return 1 for success, 0 for failure.
 *
 */
int __must_check
ktext_reader_trylock(ktext_object_t *k);

/**
 * ktext_writer_trylock() - try to acquire a writer lock
 * 							on ktext_object_t
 *
 * @k: 	the ktext_object_t object
 *
 * Return 1 for success, 0 for failure.
 *
 */
int __must_check
ktext_writer_trylock(ktext_object_t *k);

/**
 * ktext_reader_lock() - acquire a reader lock
 * 						 (uninterruptible)
 *
 * @k: 	the ktext_object_t object
 *
 */
int __must_check
ktext_reader_lock(ktext_object_t *k);

/**
 * ktext_writer_lock() - acquire a writer lock
 * 						 (uninterruptible)
 *
 * @k: 	the ktext_object_t object
 *
 */
int __must_check
ktext_writer_lock(ktext_object_t *k);

/**
 * ktext_reader_unlock() - release a reader lock
 *
 * @k: 	the ktext_object_t object
 *
 */
void
ktext_reader_unlock(ktext_object_t *k);

/**
 * ktext_writer_unlock() - release a writer lock
 *
 * @k: 	the ktext_object_t object
 *
 */
void
ktext_writer_unlock(ktext_object_t *k);

#endif

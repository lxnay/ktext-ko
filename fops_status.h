/*
 * fops_status.h
 *
 * "Linux File Operations" status object (appended to file->private_data)
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

#ifndef FOPS_STATUS_H_
#define FOPS_STATUS_H_

/**
 * struct fops_status - 	object used for tracking a request status
 * 				from ktext_open() through ktext_read() or
 * 				ktext_write() to ktext_release()
 *
 * @text:			the actual text string being processed
 * @count:			the offset in @text
 * @read_text_strlen:		strlen(@text), used by readers
 * @total:			size of @text buffer
 *
 * This object is private to a single request. Given this
 * scope, it doesn't require any protection.
 */
typedef struct fops_status {
	char *text;
	loff_t count;
	loff_t read_text_strlen; /* only used by readers */
	size_t total;
} fops_status_t;


/**
 * fops_status_init() - initialize a fops_status_t object.
 *
 * @fs:		the fops_status_t object
 *
 * This is an internal function used to initialize
 * a new fops_status_t object, which is bound to a single
 * struct file (and its lifecycle from ktext_open() to
 * ktext_release()).
 */
int __must_check
fops_status_init(fops_status_t **fs, bool allocate_text);

/**
 * fops_status_destroy() - destroy a fops_status_t object.
 *
 * @fs:		the fops_status_t object
 *
 * This is an internal function used to destroy
 * a previously allocated fops_status_t object.
 *
 */
void
fops_status_destroy(fops_status_t *fs);

#endif

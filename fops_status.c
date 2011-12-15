/*
 * fops_status.c
 *
 * fops_status_t functions used by ktext_mod.c.
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

#include <linux/slab.h>
#include <linux/printk.h>

#include "ktext_config.h"
#include "fops_status.h"

/**
 * fops_status_init() - initialize a fops_status_t object.
 *
 * @fs:	the fops_status_t object
 *
 * This is an internal function used to initialize
 * a new fops_status_t object, which is bound to a single
 * struct file (and its lifecycle from ktext_open() to
 * ktext_release()).
 */
int __must_check
fops_status_init(fops_status_t **fs, bool allocate_text)
{
	int status;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "fops_status_init: fs: %p\n", *fs);
#endif

	status = 0;

	if (*fs != NULL) {
		BUG_ON(*fs);
		BUG();
	}

	*fs = kmalloc(sizeof(fops_status_t), GFP_KERNEL);
	if (*fs == NULL) {
		printk(KERN_NOTICE "ktext, fops_status_init: unable to allocate fops_status_t!\n");
		status = -ENOMEM;
		goto fops_status_init_quit;
	}

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "fops_status_init: allocated fs: %p\n", *fs);
#endif

	if (allocate_text) {
		(*fs)->text = (char *) kzalloc(sizeof(char) * KTEXT_SIZE, GFP_KERNEL);
		if ((*fs)->text == NULL) {
			printk(KERN_NOTICE "ktext, fops_status_init: unable to allocate text!\n");
			status = -ENOMEM;
			goto fops_status_init_dealloc_fs;
		}
	} else
		(*fs)->text = NULL;

	(*fs)->count = 0;

	(*fs)->total = KTEXT_SIZE;
	(*fs)->read_text_strlen = 0;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "fops_status_init: all done.\n");
#endif

	goto fops_status_init_quit;

fops_status_init_dealloc_fs:
	kfree(*fs);

fops_status_init_quit:
	return status;
}

/**
 * fops_status_destroy() - destroy a fops_status_t object.
 *
 * @fs:	the fops_status_t object
 *
 * This is an internal function used to destroy
 * a previously allocated fops_status_t object.
 *
 */
void
fops_status_destroy(fops_status_t *fs)
{
	if (fs == NULL)
		BUG();

	if (fs->text) {
		kfree(fs->text);
		fs->text = NULL;
	}
	kfree(fs);
	fs = NULL;
}

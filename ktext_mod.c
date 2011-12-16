/*
 * ktext.c
 * Stash and Suck short text to and from kernel memory (max size: PAGE_SIZE -1 -100).
 * Longer text will be truncated, that's it.
 * This module accepts a stream of characters in its input and once
 * gotten to EOF, the same is stashed into a FIFO queue.
 * At each read, the first element of the FIFO will be handed to
 * userspace stinky caller.
 * It is implemented using LIST_HEAD, nothing fancy in it.
 *
 * This implementation uses rw_semaphore, which, given LDD protects
 * writers from starvation (but could cause readers starvation):
 *
 * "An rwsem allows either one writer or an unlimited number of
 * readers to hold the semaphore. Writers get priority; as soon
 * as a writer tries to enter the critical section, no readers
 * will be allowed in until all writers have completed their work.
 * This implementation can lead to reader starvation—where readers
 * are denied access for a long time—if you have a large number
 * of writers contending for the semaphore. For this reason,
 * rwsems are best used when write access is required only rarely,
 * and writer access is held for short periods of time."
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/init.h>

#include "ktext_config.h"
#include "ktext_object.h"
#include "fops_status.h"

int max_elements = 0;
module_param(max_elements, int, 0);
MODULE_PARM_DESC(max_elements, "Maximum amount of FIFO elements");

/* global k_text object */
static ktext_object_t *ktext;

/**
 * ktext_open() - the file_operations.open function.
 *
 * @inode: the inode object
 * @filp: the file object
 *
 * Execute open() for given filp, determining if it's
 * read/write/append mode.
 * if KTEXT_NONBLOCK_SUPPORT macro is != 0,
 * blocking/non-blocking mode will be handled as
 * requested. Otherwise (default behavior), also
 * in order to avoid possible US process
 * deadlocks, the readers/writers semaphore
 * will be locked using _trylock() functions.
 *
 */
static int
ktext_open(struct inode *inode, struct file *filp)
{
	int status;
	int rwsem_acquired;
	int push_allowed;
	bool non_block;
	bool write_mode;
	bool read_mode;
	bool append;

	read_mode = filp->f_mode & FMODE_READ;
	write_mode = filp->f_mode & FMODE_WRITE;
	/* mumble mumble, shall i block or notz? */
#if KTEXT_NONBLOCK_SUPPORT
	/* if two writers call open() in the same process,
	 * in the same thread, it's the end of the world.
	 * Not even SIGKILL can come to the rescue.
	 * This is because there is no interruptible
	 * version of rw_semaphore.
	 */
	non_block = filp->f_flags & O_NONBLOCK;
#else
	non_block = true;
#endif
	append = filp->f_flags & O_APPEND;
	status = 0;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_open: inode: %p - file: %p - path: %s, "
			"read: %s, write: %s, non-blocking: %s, append: %s"
			"\n",
			inode, filp, filp->f_path.dentry->d_name.name,
			read_mode ? "true" : "false",
			write_mode ? "true" : "false",
			non_block ? "true" : "false",
			append ? "true" : "false");
#endif

	rwsem_acquired = 0;
	if (!write_mode) {
		/* try to acquire the read end */
		if (non_block)
			rwsem_acquired = ktext_reader_trylock(ktext);
		else
			/* CANBLOCK */
			ktext_reader_lock(ktext);
	} else {
		/* WRITE */
		if (non_block)
			rwsem_acquired = ktext_writer_trylock(ktext);
		else
			/* CANBLOCK */
			ktext_writer_lock(ktext);
	}

	if (non_block && !rwsem_acquired) {
#ifdef KTEXT_DEBUG
		printk(KERN_NOTICE "ktext_open: inode: %p - file: %p. "
				"nb mode, rwsem not acquired\n",
				inode, filp);
#endif
		status = -EAGAIN;
		goto ktext_open_quit;
	}

	if (!append && write_mode) {
#ifdef KTEXT_DEBUG
		printk(KERN_NOTICE "ktext_open: inode: %p - file: %p. "
				"Write mode (append: off)\n",
				inode, filp);
#endif
		push_allowed = ktext_push_allowed(ktext, max_elements);
		if (unlikely(!push_allowed)) {
			printk(KERN_NOTICE
					"ktext_open: max_elements limit reached (sorry)\n");
			status = -ENOSPC;
			goto ktext_open_quit_write_sem_up;
		} else if (unlikely(push_allowed < 0)) {
			printk(KERN_NOTICE
					"ktext_open: push_allowed interrupted!\n");
			status = push_allowed;
			goto ktext_open_quit_write_sem_up;
		}
	}

	/* "trust no one", private_data contains the whole text */
	filp->private_data = NULL;

	goto ktext_open_quit;

ktext_open_quit_write_sem_up:
	/* ktext_release is not called if we get here */
	ktext_writer_unlock(ktext);

ktext_open_quit:
	return status;
}

/**
 * ktext_release() - the file_operations.release function.
 *
 * @inode: the inode object
 * @filp: the file object
 *
 * Release all the resources associated to filp, as well
 * as the read or write end of the rw_semaphore used.
 *
 */
static int
ktext_release(struct inode *inode, struct file *filp)
{
	fops_status_t *fs;
	bool write_mode;
	int status;

	/* simmetric to ktext_open(), if write_mode, account
	 * the written data (to private_data) to the list.
	 * If not write_mode (read_mode ON), just clean up
	 * private_data.
	 */
	write_mode = filp->f_mode & FMODE_WRITE;
	status = 0;
	fs = (fops_status_t *) filp->private_data;

#ifdef KTEXT_DEBUG
	if (fs == NULL) {
		printk(KERN_NOTICE "ktext_release: inode: %p - file: %p. "
				"write: %s, private_data == NULL.\n",
				inode, filp, write_mode ? "true" : "false");
	}
#endif

	/* store private_data as a NULL terminated string
	 * to our list.
	 */
	if (write_mode && fs) {
		status = ktext_push(ktext, fs->text, fs->count);
#ifdef KTEXT_DEBUG
		printk(KERN_NOTICE "ktext_release: inode: %p - file: %p. "
				"write: true, pushing: %s, status: %d\n",
				inode, filp, fs->text, status);
#endif
	}

	if (fs) {
		fops_status_destroy(fs);
		fs = NULL;
		filp->private_data = NULL;
	}
	if (write_mode)
		ktext_writer_unlock(ktext);
	else
		ktext_reader_unlock(ktext);
	return status;
}

/**
 * ktext_read() - the file_operations.read function.
 *
 * @filp: 	the file object
 * @buf:	the userspace buffer
 * @count:	the buffer size
 * @f_pos:	userspace buffer cursor position
 *
 * Read one single string from the FIFO until the end.
 * On every ktext_open(), a new string will be popped
 * out from the FIFO and fed to stinky userspace, that's it.
 *
 */
static ssize_t
ktext_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	int status;
	fops_status_t *fs;
	size_t buf_len;
	size_t to_read_len;
	char *text;

	status = 0;
	text = NULL;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_read: file: %p. "
			"private_data: %p\n",
			filp, filp->private_data);
#endif

	fs = (fops_status_t *) filp->private_data;
	if (fs == NULL) {
		status = fops_status_init(&fs, false);
		if (status != 0)
			goto ktext_read_quit;

		if (fs->text != NULL) {
			BUG_ON(fs->text);
			BUG();
		}

		/* get the first string on the FIFO */
		status = ktext_pop(ktext, &text);
		if (status != 0)
			goto ktext_read_quit;

		/* ignore NULL pointer above, attach our string to fs below */
		fs->text = text;
		fs->count = 0;
		if (text)
			fs->read_text_strlen = strlen(fs->text);
		else
			fs->read_text_strlen = 0;

		filp->private_data = fs;
	}

	if (fs->text == NULL)
		/* nothing to read */
		goto ktext_read_quit;

	/* NOTE: do not account the NULL terminator on read,
	 * it doesn't look nice */
	buf_len = fs->read_text_strlen;
	to_read_len = buf_len - fs->count;
	if (to_read_len < 1)
		/* nothing to read */
		goto ktext_read_quit;

	if (to_read_len > count)
		to_read_len = count;

	status = simple_read_from_buffer(buf, to_read_len, &fs->count,
			fs->text, to_read_len);

ktext_read_quit:
	return status;
}

/**
 * ktext_write() - the file_operations.write function.
 *
 * @filp: 		the file object
 * @ubuf:		the userspace buffer
 * @orig_count:	the buffer size
 * @f_pos:		userspace buffer cursor position
 *
 * Stash some text on the internal buffer.
 * It will be processed later on and eventually
 * appended to the FIFO.
 *
 */
static ssize_t
ktext_write(struct file *filp, const char __user *ubuf,
		size_t orig_count, loff_t *f_pos)
{
	int status;
	size_t count;
	size_t free_buf;
	fops_status_t *fs;

	status = 0;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_write: file: %p. "
			"private_data: %p\n",
			filp, filp->private_data);
#endif

	fs = (fops_status_t *) filp->private_data;
	if (fs == NULL) {
		status = fops_status_init(&fs, true);
		if (status != 0)
			goto ktext_write_quit;
		filp->private_data = fs;
	}

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_write: file: %p. "
			"fs initialized: %p, private_data: %p\n",
			filp, fs, filp->private_data);
#endif

	free_buf = fs->total - fs->count - 1; /* keep \0 at the end */
	if (free_buf == 0) {
		/* no more space, ignore the rest of the data from user
		 * in other words, truncate -> ("yeah yeah, I've read it thanks") */
		status = orig_count;
		goto ktext_write_quit;
	}

	if (orig_count > free_buf)
		count = free_buf;
	else
		count = orig_count;

#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_write: file: %p. "
			"writing bytes: %zd\n", filp, count);
#endif

	return simple_write_to_buffer(fs->text, free_buf, &fs->count, ubuf, count);

ktext_write_quit:
	return status;
}

/* Structure that declares the usual file */
/* access functions */
static struct file_operations
ktext_fops = {
	read: ktext_read,
	write: ktext_write,
	open: ktext_open,
	release: ktext_release
};

static struct miscdevice ktext_device = {
  MISC_DYNAMIC_MINOR, "ktext", &ktext_fops
};

static int __init
ktext_init(void)
{
	int status;

	status = 0;

	/* check visit argument, validate value */
	if ((max_elements < 0) || (max_elements > 10000)) {
		printk(KERN_NOTICE "ktext: invalid max_elements= parameter (between 0 and 10000)\n");
		status = -EINVAL;
		goto ktext_init_quit;
	}
	printk(KERN_NOTICE "ktext_init: max_elements: %d, nbmode: %d\n",
			max_elements, KTEXT_NONBLOCK_SUPPORT);
	status = ktext_object_init(&ktext);
	if (status != 0)
		goto ktext_init_quit;

	status = misc_register(&ktext_device);

ktext_init_quit:
	return status;
}

static void __exit
ktext_cleanup(void)
{
	printk(KERN_NOTICE "ktext_cleanup: so long and thanks for all the fish.\n");
	misc_deregister(&ktext_device);
	ktext_object_destroy(&ktext);
}

module_init(ktext_init);
module_exit(ktext_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabio Erculiani");
MODULE_DESCRIPTION("Stash and Suck text from kernel memory");

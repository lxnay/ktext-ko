/*
 * ktext_object.c
 *
 * ktext_object_t functions used by ktext_mod.c.
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
#include <linux/list.h>
#include <linux/rwsem.h>

#include "ktext_config.h"
#include "ktext_object.h"

void
ktext_object_init(ktext_object_t *k)
{
	k->n_elem = 0;
	init_rwsem(&k->__ktext_rwsem);
	/* for mutual exclusion */
	mutex_init(&k->prot);
	INIT_LIST_HEAD(&k->head);
}


void
ktext_object_destroy(ktext_object_t *k)
{
    ktext_empty(k);
}

void
ktext_object_node_init(ktext_object_node_t *n, char *text) {
	n->text = text;
}

void
ktext_object_node_destroy(ktext_object_node_t *n) {
	if (n == NULL)
		BUG();
	if (n->text)
		kfree(n->text);
    kfree(n);
}

int __must_check
ktext_push_allowed(ktext_object_t *k, int max_elements)
{
	int allowed;
	/* CRIT:ON */
	allowed = mutex_lock_interruptible(&k->prot);
	if (allowed)
		return allowed;
	allowed = ((k->n_elem + 1) > max_elements && max_elements != 0) ? 0 : 1;
	/* CRIT:OFF */
	mutex_unlock(&k->prot);
	return allowed;
}

int __must_check
ktext_push(ktext_object_t *k, char *text, size_t count)
{
	char *own_text;
	ktext_object_node_t *n;
	int status;
	int lock_status;

	status = 0;

	/* CRIT:ON */
	lock_status = mutex_lock_interruptible(&k->prot);
	if (lock_status < 0) {
		/* interrupted */
		status = lock_status;
		goto ktext_push_quit_noalloc;
	}

	own_text = (char *) kzalloc((sizeof(char) * count), GFP_KERNEL);
	if (!own_text) {
		printk(KERN_NOTICE "ktext_push: cannot allocate memory (damn)\n");
		status = -ENOMEM;
		goto ktext_push_quit_noalloc;
	}
	strcpy(own_text, text);

	n = kmalloc(sizeof(ktext_object_node_t), GFP_KERNEL);
	if (!n) {
		printk(KERN_NOTICE "ktext_push: cannot allocate memory (damn 2)\n");
		status = -ENOMEM;
		goto ktext_push_quit_err_sem_up;
	}

	/* we already ensured that it's NULL terminated */
	ktext_object_node_init(n, own_text);
	list_add_tail(&n->kl, &k->head);
	k->n_elem++;
	goto ktext_push_quit_clean;

ktext_push_quit_err_sem_up:
	kfree(own_text);

ktext_push_quit_clean:
	/* CRIT:OFF */
	mutex_unlock(&k->prot);

ktext_push_quit_noalloc:
	return status;
}

int __must_check
ktext_pop(ktext_object_t *k, char **text)
{
	ktext_object_node_t *n;
	int status;

	status = mutex_lock_interruptible(&k->prot);
	if (status < 0) {
		/* interrupted */
		goto ktext_pop_quit_early;
	}
	/* CRIT:ON */

	*text = NULL;
	if (list_empty(&k->head)) {
		printk(KERN_NOTICE "ktext_pop: list is empty, elems: %zd!\n", k->n_elem);
		goto ktext_pop_quit;
	}

	n = list_first_entry(&k->head, ktext_object_node_t, kl);
	list_del(&n->kl);
	k->n_elem--;
	*text = n->text;
	kfree(n);

ktext_pop_quit:
	/* CRIT:OFF */
	mutex_unlock(&k->prot);

ktext_pop_quit_early:
	return status;
}

void
ktext_empty(ktext_object_t *k)
{

	struct list_head *lh, *q;
	ktext_object_node_t *n;

	mutex_lock(&k->prot);

	if (list_empty(&k->head)) {
		printk(KERN_NOTICE "ktext_empty: list is empty\n");
		goto ktext_empty_quit;
	}

    list_for_each_safe(lh, q, &k->head) {
        n = list_entry(lh, ktext_object_node_t, kl);
#ifdef KTEXT_DEBUG
		printk(KERN_NOTICE "ktext_empty: popping: %s\n", n->text);
#endif
        list_del(lh);
        ktext_object_node_destroy(n);
        n = NULL;
    }

ktext_empty_quit:
	mutex_unlock(&k->prot);
}

int __must_check
ktext_reader_trylock(ktext_object_t *k) {
	return down_read_trylock(&k->__ktext_rwsem);
}

int __must_check
ktext_writer_trylock(ktext_object_t *k) {
	return down_write_trylock(&k->__ktext_rwsem);
}

void
ktext_reader_lock(ktext_object_t *k) {
	down_read(&k->__ktext_rwsem);
}

void
ktext_writer_lock(ktext_object_t *k) {
	down_write(&k->__ktext_rwsem);
}

void
ktext_reader_unlock(ktext_object_t *k) {
	up_read(&k->__ktext_rwsem);
}

void
ktext_writer_unlock(ktext_object_t *k) {
	up_write(&k->__ktext_rwsem);
}

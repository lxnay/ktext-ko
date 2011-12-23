/*
 * ktext_object.c
 *
 * ktext_object functions used by ktext_mod.c.
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

#include "ktext_config.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#ifdef KTEXT_ALT_RW_STARV_PROT
#include <linux/semaphore.h>
#include <linux/mutex.h>
#else
#include <linux/rwsem.h>
#endif

#include "ktext_config.h"
#include "ktext_object.h"

/**
 * struct ktext_object -	the ktree FIFO object implemented with
 * 				Kernel lists.
 *
 * @n_elem:		number of elements in the FIFO
 * @head:		the list_head object
 * @ktext_rwsem:	the readers/writers semaphore
 * @prot:		the semaphore protecting against concurrent
 * 			access to the object
 */
struct ktext_object {
	size_t n_elem;
	struct list_head head;
#ifdef KTEXT_ALT_RW_STARV_PROT
	int __nbr;
	int __nbw;
	int __nr;
	int __nw;
	struct semaphore __priv_r;
	struct semaphore __priv_w;
	struct mutex __m;
#else
	struct rw_semaphore __ktext_rwsem;
#endif
	struct mutex prot;
};

/**
 * struct ktext_object_node -	Linux list_head node objectm
 *
 * @text:	the actual text (payload)
 * @kl:		the list_head object
 */
typedef struct ktext_object_node {
    char *text;
    struct list_head kl;
} ktext_object_node_t;

int __must_check
ktext_object_init(ktext_object_t **k)
{
	if (k == NULL)
		BUG();
	if (*k != NULL) {
		BUG_ON(k);
		BUG();
	}
	*k = kmalloc(sizeof(ktext_object_t), GFP_KERNEL);
	if (*k == NULL)
		return -ENOMEM;

	(*k)->n_elem = 0;
#ifdef KTEXT_ALT_RW_STARV_PROT
	(*k)->__nbr = 0;
	(*k)->__nbw = 0;
	(*k)->__nr = 0;
	(*k)->__nw = 0;
	sema_init(&(*k)->__priv_r, 0);
	sema_init(&(*k)->__priv_w, 0);
	mutex_init(&(*k)->__m);
#else
	init_rwsem(&(*k)->__ktext_rwsem);
#endif
	mutex_init(&(*k)->prot);
	INIT_LIST_HEAD(&(*k)->head);
	return 0;
}


void
ktext_object_destroy(ktext_object_t **k)
{
	if (k == NULL)
		BUG();
	if (*k == NULL) {
		BUG_ON(*k);
		BUG();
	}

	ktext_empty(*k);
	kfree(*k);
}

/**
 * ktext_object_node_init() -  initialize a previously allocated
 *                             ktext_object_node_t
 *
 * @n:         the ktext_object_node_t object
 * @text:      the text to attach to this ktext_object_node_t
 *
 */
static void
ktext_object_node_init(ktext_object_node_t *n, char *text) {
	n->text = text;
}

/**
 * ktext_object_node_destroy() -       deinitialize a previously
 *                                     initialized ktext_object_node_t
 *
 * @n: the ktext_object_node_t object
 *
 */
static void
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
	size_t zeroed_count;

	status = 0;

	/* CRIT:ON */
	lock_status = mutex_lock_interruptible(&k->prot);
	if (lock_status < 0) {
		/* interrupted */
		status = lock_status;
		goto ktext_push_quit_noalloc;
	}

	zeroed_count = count + 1;
#ifdef KTEXT_DEBUG
	printk(KERN_NOTICE "ktext_push: preparing to kzalloc: %zdb, for: %s\n",
			zeroed_count, text);
#endif
	own_text = (char *) kzalloc((sizeof(char) * zeroed_count), GFP_KERNEL);
	if (own_text == NULL) {
		printk(KERN_NOTICE "ktext_push: cannot allocate memory (damn)\n");
		status = -ENOMEM;
		goto ktext_push_quit_noalloc;
	}
	strcpy(own_text, text);

	n = kmalloc(sizeof(ktext_object_node_t), GFP_KERNEL);
	if (n == NULL) {
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
#ifdef KTEXT_ALT_RW_STARV_PROT
	if (!mutex_trylock(&k->__m))
		return 0;

	if (k->__nw > 0 || k->__nbw > 0)
		k->__nbr++;
	else {
		k->__nr++;
		up(&k->__priv_r);
	}
	mutex_unlock(&k->__m);
	down(&k->__priv_r); /* don't use trylock here */

	return 0;
#else
	return down_read_trylock(&k->__ktext_rwsem);
#endif
}

int __must_check
ktext_writer_trylock(ktext_object_t *k) {
#ifdef KTEXT_ALT_RW_STARV_PROT
	if (!mutex_trylock(&k->__m))
		return 0;

	if (k->__nr > 0 || k->__nw > 0)
		k->__nbw++;
	else {
		k->__nw++;
		up(&k->__priv_w);
	}
	mutex_unlock(&k->__m);
	down(&k->__priv_w); /* don't use trylock here */

	return 0;
#else
	return down_write_trylock(&k->__ktext_rwsem);
#endif
}

int __must_check
ktext_reader_lock(ktext_object_t *k) {
#ifdef KTEXT_ALT_RW_STARV_PROT
	int status;

	status = mutex_lock_interruptible(&k->__m);
	if (status)
		return status;

	if (k->__nw > 0 || k->__nbw > 0)
		k->__nbr++;
	else {
		k->__nr++;
		up(&k->__priv_r);
	}
	mutex_unlock(&k->__m);
	down(&k->__priv_r);

	return status;
#else
	down_read(&k->__ktext_rwsem);
	return 0;
#endif
}

int __must_check
ktext_writer_lock(ktext_object_t *k) {
#ifdef KTEXT_ALT_RW_STARV_PROT
	int status;

	status = mutex_lock_interruptible(&k->__m);
	if (status)
		return status;

	if (k->__nr > 0 || k->__nw > 0)
		k->__nbw++;
	else {
		k->__nw++;
		up(&k->__priv_w);
	}
	mutex_unlock(&k->__m);
	down(&k->__priv_w);

	return status;
#else
	down_write(&k->__ktext_rwsem);
	return 0;
#endif
}

void
ktext_reader_unlock(ktext_object_t *k) {
#ifdef KTEXT_ALT_RW_STARV_PROT
	mutex_lock(&k->__m);
	k->__nr--;
	if (k->__nbw > 0 && k->__nr == 0) {
		k->__nbw--;
		k->__nw++;
		up(&k->__priv_w);
	}
	mutex_unlock(&k->__m);
#else
	up_read(&k->__ktext_rwsem);
#endif
}

void
ktext_writer_unlock(ktext_object_t *k) {
#ifdef KTEXT_ALT_RW_STARV_PROT
	mutex_lock(&k->__m);
	k->__nw--;
	if (k->__nbr > 0) {
		while (k->__nbr > 0) {
			k->__nbr--;
			k->__nr++;
			up(&k->__priv_r);
		}
	} else if (k->__nbw > 0) {
		k->__nbw--;
		k->__nw++;
		up(&k->__priv_w);
	}
	mutex_unlock(&k->__m);
#else
	up_write(&k->__ktext_rwsem);
#endif
}

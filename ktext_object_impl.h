/*
 * ktext_object_impl.h
 * Implementation part of ktext_object
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

/* ktext_object_t won't be defined, defining here */
#define KTEXT_OBJECT_T

/**
 * struct ktext_object -	the ktree FIFO object implemented with
 * 							Kernel lists.
 *
 * @n_elem:				number of elements in the FIFO
 * @head:				the list_head object
 * @ktext_rwsem:		the readers/writers semaphore
 * @prot:				the semaphore protecting against concurrent
 * 						access to the object
 */
typedef struct ktext_object {
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
} ktext_object_t;

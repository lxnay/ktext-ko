/*
 * ktext_config.h
 *
 * General project variables and constants.
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

#ifndef KTEXT_CONFIG_H_
#define KTEXT_CONFIG_H_

/**
 * Enable debug output if defined.
 */
#define KTEXT_DEBUG

/**
 * If 0, non-blocking mode will be always enabled.
 * If open() can't acquire ktext_object_t resource
 * -EWOULDBLOCK will be raised. This also avoids
 * to block in uninterruptible state, since when using
 * rw_semaphore that's the only option.
 */
#define KTEXT_NONBLOCK_SUPPORT 1

/**
 * Implement an alternative readers/writers anti-starvation
 * protocol, as known as preventive signal().
 */
#define KTEXT_ALT_RW_STARV_PROT

/**
 * kmalloc doesn't work with large requests.
 * Since this is a very simple module, we just limit
 * the size of each request to a reasonable value.
 */
#define KTEXT_SIZE (size_t)(PAGE_SIZE - 1 - 100)

#endif

/*
 * sgen-major-scan-object.h: Object scanning in the major collectors.
 *
 * Copyright 2001-2003 Ximian, Inc
 * Copyright 2003-2010 Novell, Inc.
 * Copyright (C) 2012 Xamarin Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License 2.0 as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License 2.0 along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

extern long long stat_scan_object_called_major;

#ifdef FIXED_HEAP
#define PREFETCH_DYNAMIC_HEAP(addr)
#else
#define PREFETCH_DYNAMIC_HEAP(addr)	PREFETCH ((addr))
#endif

#ifdef SCAN_FOR_CONCURRENT_MARK
#define FOLLOW_OBJECT(addr)	(!sgen_ptr_in_nursery ((addr)))
#define ALWAYS_ADD_TO_GLOBAL_REMSET	1
#define CONCURRENT_NAME(x)	x ## _concurrent
#else
#define FOLLOW_OBJECT(addr)	1
#define ALWAYS_ADD_TO_GLOBAL_REMSET	0
#define CONCURRENT_NAME(x)	x
#endif

/*
 * FIXME: We use the same scanning function in the concurrent collector whether we scan
 * during the starting/finishing collection pause (with the world stopped) or from the
 * concurrent worker thread.
 *
 * As long as the world is stopped, we should just follow pointers into the nursery and
 * evict if possible.  In that case we also don't need the ALWAYS_ADD_TO_GLOBAL_REMSET case,
 * which only seems to make sense for when the world is stopped, in which case we only need
 * it because we don't follow into the nursery.
 */

#undef HANDLE_PTR
#define HANDLE_PTR(ptr,obj)	do {					\
		void *__old = *(ptr);					\
		SGEN_OBJECT_LAYOUT_STATISTICS_MARK_BITMAP ((obj), (ptr)); \
		if (__old && FOLLOW_OBJECT (__old)) {			\
			void *__copy;					\
			PREFETCH_DYNAMIC_HEAP (__old);			\
			CONCURRENT_NAME (major_copy_or_mark_object) ((ptr), __old, queue); \
			__copy = *(ptr);				\
			SGEN_COND_LOG (9, __old != __copy, "Overwrote field at %p with %p (was: %p)", (ptr), *(ptr), __old); \
			if (G_UNLIKELY (sgen_ptr_in_nursery (__copy) && !sgen_ptr_in_nursery ((ptr)))) \
				sgen_add_to_global_remset ((ptr), __copy);	\
		} else {						\
			if (ALWAYS_ADD_TO_GLOBAL_REMSET && G_UNLIKELY (sgen_ptr_in_nursery (__old) && !sgen_ptr_in_nursery ((ptr)))) \
				sgen_add_to_global_remset ((ptr), __old); \
		}							\
	} while (0)

static void
CONCURRENT_NAME (major_scan_object_no_mark) (char *start, mword desc, SgenGrayQueue *queue)
{
	SGEN_OBJECT_LAYOUT_STATISTICS_DECLARE_BITMAP;

#ifdef HEAVY_STATISTICS
	sgen_descriptor_count_scanned_object (desc);
#endif

#define SCAN_OBJECT_PROTOCOL
#include "sgen-scan-object.h"

	SGEN_OBJECT_LAYOUT_STATISTICS_COMMIT_BITMAP;
	HEAVY_STAT (++stat_scan_object_called_major);
}

static void
CONCURRENT_NAME (major_scan_object) (char *start, mword desc, SgenGrayQueue *queue)
{
	if (!sgen_ptr_in_nursery (start)) {
		if (SGEN_ALIGN_UP (sgen_safe_object_get_size ((MonoObject*)start)) <= SGEN_MAX_SMALL_OBJ_SIZE) {
			MSBlockInfo *block = MS_BLOCK_FOR_OBJ (start);
			int word, bit;
			MS_CALC_MARK_BIT (word, bit, start);
			if (MS_MARK_BIT (block, word, bit))
				return;
			MS_SET_MARK_BIT (block, word, bit);
			binary_protocol_mark (start, (gpointer)LOAD_VTABLE (start), sgen_safe_object_get_size ((MonoObject*)start));
		} else {
			if (sgen_los_object_is_pinned (start))
				return;
			sgen_los_pin_object (start);
		}
	}

	CONCURRENT_NAME (major_scan_object_no_mark) (start, desc, queue);
}

#ifdef SCAN_FOR_CONCURRENT_MARK
#ifdef SGEN_PARALLEL_MARK
#error concurrent and parallel mark not supported yet
#else
static void
CONCURRENT_NAME (major_scan_vtype) (char *start, mword desc, SgenGrayQueue *queue BINARY_PROTOCOL_ARG (size_t size))
{
	SGEN_OBJECT_LAYOUT_STATISTICS_DECLARE_BITMAP;

	/* The descriptors include info about the MonoObject header as well */
	start -= sizeof (MonoObject);

#define SCAN_OBJECT_NOVTABLE
#define SCAN_OBJECT_PROTOCOL
#include "sgen-scan-object.h"

	SGEN_OBJECT_LAYOUT_STATISTICS_COMMIT_BITMAP;
}
#endif
#endif

#undef PREFETCH_DYNAMIC_HEAP
#undef FOLLOW_OBJECT
#undef ALWAYS_ADD_TO_GLOBAL_REMSET
#undef CONCURRENT_NAME

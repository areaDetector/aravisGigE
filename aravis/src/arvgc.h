/* Aravis - Digital camera library
 *
 * Copyright © 2009-2010 Emmanuel Pacaud
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Emmanuel Pacaud <emmanuel@gnome.org>
 */

#ifndef ARV_GC_H
#define ARV_GC_H

#include <arvtypes.h>

G_BEGIN_DECLS

#define ARV_TYPE_GC             (arv_gc_get_type ())
#define ARV_GC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ARV_TYPE_GC, ArvGc))
#define ARV_GC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ARV_TYPE_GC, ArvGcClass))
#define ARV_IS_GC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ARV_TYPE_GC))
#define ARV_IS_GC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ARV_TYPE_GC))
#define ARV_GC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ARV_TYPE_GC, ArvGcClass))

typedef struct _ArvGcClass ArvGcClass;

struct _ArvGc {
	GObject	object;

	GHashTable *nodes;
	ArvDevice *device;
};

struct _ArvGcClass {
	GObjectClass parent_class;
};

GType arv_gc_get_type (void);

ArvGc * 		arv_gc_new 			(ArvDevice *device, const void *xml, size_t size);

gint64 			arv_gc_get_int64_from_value 	(ArvGc *genicam, GValue *value);
void 			arv_gc_set_int64_to_value 	(ArvGc *genicam, GValue *value, gint64 v_int64);
double 			arv_gc_get_double_from_value 	(ArvGc *genicam, GValue *value);
void 			arv_gc_set_double_to_value 	(ArvGc *genicam, GValue *value, double v_double);
ArvGcNode *		arv_gc_get_node			(ArvGc *genicam, const char *name);
ArvDevice *		arv_gc_get_device		(ArvGc *genicam);

G_END_DECLS

#endif

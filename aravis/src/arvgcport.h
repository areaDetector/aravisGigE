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

#ifndef ARV_GC_PORT_H
#define ARV_GC_PORT_H

#include <arvtypes.h>
#include <arvgcnode.h>

G_BEGIN_DECLS

#define ARV_TYPE_GC_PORT             (arv_gc_port_get_type ())
#define ARV_GC_PORT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ARV_TYPE_GC_PORT, ArvGcPort))
#define ARV_GC_PORT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ARV_TYPE_GC_PORT, ArvGcPortClass))
#define ARV_IS_GC_PORT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ARV_TYPE_GC_PORT))
#define ARV_IS_GC_PORT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ARV_TYPE_GC_PORT))
#define ARV_GC_PORT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ARV_TYPE_GC_PORT, ArvGcPortClass))

typedef struct _ArvGcPortClass ArvGcPortClass;

struct _ArvGcPort {
	ArvGcNode node;
};

struct _ArvGcPortClass {
	ArvGcNodeClass parent_class;
};

GType arv_gc_port_get_type (void);

ArvGcNode * 		arv_gc_port_new 	(void);

void 			arv_gc_port_read	(ArvGcPort *port, void *buffer, guint64 address, guint64 length);
void 			arv_gc_port_write	(ArvGcPort *port, void *buffer, guint64 address, guint64 length);

G_END_DECLS

#endif

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

#ifndef ARV_GC_REGISTER_H
#define ARV_GC_REGISTER_H

#include <arvtypes.h>
#include <arvgcnode.h>

G_BEGIN_DECLS

/**
 * ArvGcSign:
 * @ARV_GC_SIGN_SIGNED: signed integer
 * @ARV_GC_SIGN_UNSIGNED: unsigned integer
 */

typedef enum
{
	ARV_GC_SIGN_SIGNED,
	ARV_GC_SIGN_UNSIGNED
} ArvGcSign;

/**
 * ArvGcRegisterType:
 * @ARV_GC_REGISTER_TYPE_REGISTER: Register node
 * @ARV_GC_REGISTER_TYPE_INTEGER: IntReg node
 * @ARV_GC_REGISTER_TYPE_MASKED_INTEGER: MaskedIntReg node
 * @ARV_GC_REGISTER_TYPE_FLOAT: FloatReg node
 * @ARV_GC_REGISTER_TYPE_STRING: StringReg node
 */

typedef enum {
       ARV_GC_REGISTER_TYPE_REGISTER,
       ARV_GC_REGISTER_TYPE_INTEGER,
       ARV_GC_REGISTER_TYPE_MASKED_INTEGER,
       ARV_GC_REGISTER_TYPE_FLOAT,
       ARV_GC_REGISTER_TYPE_STRING
} ArvGcRegisterType;

#define ARV_TYPE_GC_REGISTER             (arv_gc_register_get_type ())
#define ARV_GC_REGISTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ARV_TYPE_GC_REGISTER, ArvGcRegister))
#define ARV_GC_REGISTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ARV_TYPE_GC_REGISTER, ArvGcRegisterClass))
#define ARV_IS_GC_REGISTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ARV_TYPE_GC_REGISTER))
#define ARV_IS_GC_REGISTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ARV_TYPE_GC_REGISTER))
#define ARV_GC_REGISTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ARV_TYPE_GC_REGISTER, ArvGcRegisterClass))

typedef struct _ArvGcRegisterClass ArvGcRegisterClass;

struct _ArvGcRegister {
	ArvGcNode	node;

	ArvGcRegisterType type;
	GType value_type;
	GSList *addresses;
	char *index;
	GValue index_offset;
	GValue length;
	ArvGcAccessMode access_mode;
	ArvGcCachable cachable;
	guint64 polling_time;
	char *port_name;
	guint endianess;
	ArvGcSign sign;
	guint msb;
	guint lsb;

	void *cache;
	size_t cache_size;
	gboolean is_cache_valid;

	char v_string[G_ASCII_DTOSTR_BUF_SIZE];

	GSList *invalidators;
};

struct _ArvGcRegisterClass {
	ArvGcNodeClass parent_class;
};

GType 		arv_gc_register_get_type 		(void);
ArvGcNode * 	arv_gc_register_new 			(void);
ArvGcNode * 	arv_gc_register_new_integer 		(void);
ArvGcNode * 	arv_gc_register_new_masked_integer 	(void);
ArvGcNode * 	arv_gc_register_new_float	 	(void);
ArvGcNode * 	arv_gc_register_new_string 		(void);
void 		arv_gc_register_get			(ArvGcRegister *gc_register, void *buffer, guint64 Length);
void 		arv_gc_register_set			(ArvGcRegister *gc_register, void *buffer, guint64 Length);
guint64 	arv_gc_register_get_address 		(ArvGcRegister *gc_register);
guint64 	arv_gc_register_get_length		(ArvGcRegister *gc_register);

G_END_DECLS

#endif

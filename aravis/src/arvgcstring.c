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

/**
 * SECTION: arvgcstring
 * @short_description: String interface
 */

#include <arvgcstring.h>
#include <arvtools.h>

static void
arv_gc_string_default_init (ArvGcStringInterface *gc_string_iface)
{
}

G_DEFINE_INTERFACE (ArvGcString, arv_gc_string, G_TYPE_OBJECT)

const char *
arv_gc_string_get_value (ArvGcString *gc_string)
{
	g_return_val_if_fail (ARV_IS_GC_STRING (gc_string), 0);

	return ARV_GC_STRING_GET_INTERFACE (gc_string)->get_value (gc_string);
}

void
arv_gc_string_set_value (ArvGcString *gc_string, const char *value)
{
	g_return_if_fail (ARV_IS_GC_STRING (gc_string));

	ARV_GC_STRING_GET_INTERFACE (gc_string)->set_value (gc_string, value);
}

gint64
arv_gc_string_get_max_length (ArvGcString *gc_string)
{
	ArvGcStringInterface *string_interface;

	g_return_val_if_fail (ARV_IS_GC_STRING (gc_string), 0);

	string_interface = ARV_GC_STRING_GET_INTERFACE (gc_string);

	if (string_interface->get_max_length != NULL)
		return string_interface->get_max_length (gc_string);
	else
		return 0;
}

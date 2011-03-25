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
 * SECTION: arvbuffer
 * @short_description: Buffer for storage of video frames
 *
 * #ArvBuffer provides a class for the instantiation of buffers used for the
 * storage of the separate images of the video stream. The actual data space
 * may either be allocated by #ArvBuffer during an object instatiation, of
 * preallocated. #ArvBuffer also allows the transmission of image metadata,
 * such as offsets and size of the transmitted region of interrest, pixel
 * format and time stamp.
 */

#include <arvbuffer.h>

static GObjectClass *parent_class = NULL;

/**
 * arv_buffer_new:
 * @size: payload size
 * @preallocated: (transfer none): preallocated memory buffer
 *
 * Creates a new buffer for the storage of the video stream images. 
 * The data space can be either preallocated, and the caller is responsible
 * for it's deallocation, or allocated by this function. If it is the case,
 * data memory will be freed when the buffer is destroyed.
 */

ArvBuffer *
arv_buffer_new (size_t size, void *preallocated)
{
	ArvBuffer *buffer;

	buffer = g_object_new (ARV_TYPE_BUFFER, NULL);
	buffer->size = size;

	if (preallocated != NULL) {
		buffer->is_preallocated = TRUE;
		buffer->data = preallocated;
	} else {
		buffer->is_preallocated = FALSE;
		buffer->data = g_malloc (size);
	}

	return buffer;
}

/**
 * arv_buffer_clear:
 * @buffer: a #ArvBuffer
 *
 * Clears the buffer status.
 */

void
arv_buffer_clear (ArvBuffer *buffer)
{
	g_return_if_fail (ARV_IS_BUFFER (buffer));

	buffer->status = ARV_BUFFER_STATUS_CLEARED;
}

static void
arv_buffer_init (ArvBuffer *buffer)
{
	buffer->status = ARV_BUFFER_STATUS_CLEARED;
}

static void
arv_buffer_finalize (GObject *object)
{
	ArvBuffer *buffer = ARV_BUFFER (object);

	if (!buffer->is_preallocated) {
		g_free (buffer->data);
		buffer->data = NULL;
		buffer->size = 0;
	}

	parent_class->finalize (object);
}

static void
arv_buffer_class_init (ArvBufferClass *node_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (node_class);

	parent_class = g_type_class_peek_parent (node_class);

	object_class->finalize = arv_buffer_finalize;
}

G_DEFINE_TYPE (ArvBuffer, arv_buffer, G_TYPE_OBJECT)


/*
 * Copyright © 2006 Eric Jonas <jonas@mit.edu>
 * Copyright © 2006 Antoine Tremblay <hexa00@gmail.com>
 * Copyright © 2010 United States Government, Joshua M. Doe <joshua.doe@us.army.mil>
 * Copyright © 2010-2011 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-aravissrc
 *
 * Source using the Aravis vision library
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v aravissrc ! video/x-raw-yuv,width=512,height=512,framerate=25/1 ! autovideosink
 * ]|
 * </refsect2>
 */

#include <gstaravis.h>
#include <time.h>
#include <string.h>

#define GST_ARAVIS_N_BUFFERS			50
#define GST_ARAVIS_BUFFER_TIMEOUT_DEFAULT	2000000

GST_DEBUG_CATEGORY_STATIC (aravis_debug);
#define GST_CAT_DEFAULT aravis_debug

enum
{
  PROP_0,
  PROP_CAMERA_NAME,
  PROP_GAIN,
  PROP_EXPOSURE,
  PROP_H_BINNING,
  PROP_V_BINNING
};

GST_BOILERPLATE (GstAravis, gst_aravis, GstPushSrc, GST_TYPE_PUSH_SRC);

static GstStaticPadTemplate aravis_src_template = GST_STATIC_PAD_TEMPLATE ("src",
									   GST_PAD_SRC,
									   GST_PAD_ALWAYS,
									   GST_STATIC_CAPS ("ANY"));

static GstCaps *
gst_aravis_get_all_camera_caps (GstAravis *gst_aravis)
{
	GstCaps *caps;
	ArvPixelFormat *pixel_formats;
	double frame_rate;
	int min_height, min_width;
	int max_height, max_width;
	unsigned int n_pixel_formats, i;

	g_return_val_if_fail (GST_IS_ARAVIS (gst_aravis), NULL);

	if (!ARV_IS_CAMERA (gst_aravis->camera))
		return NULL;

	GST_LOG_OBJECT (gst_aravis, "Get all camera caps");

	arv_camera_get_width_bounds (gst_aravis->camera, &min_width, &max_width);
	arv_camera_get_height_bounds (gst_aravis->camera, &min_height, &max_height);
	pixel_formats = arv_camera_get_available_pixel_formats (gst_aravis->camera, &n_pixel_formats);

	caps = gst_caps_new_empty ();
	for (i = 0; i < n_pixel_formats; i++) {
		const char *caps_string;

		caps_string = arv_pixel_format_to_gst_caps_string (pixel_formats[i]);

		if (caps_string != NULL) {
			GstStructure *structure;

			structure = gst_structure_from_string (caps_string, NULL);
			gst_structure_set (structure,
					   "width", GST_TYPE_INT_RANGE, min_width, max_width,
					   "height", GST_TYPE_INT_RANGE, min_height, max_height,
					   "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
					   NULL);
			gst_caps_append_structure (caps, structure);
		}
	}

	g_free (pixel_formats);

	return caps;
}

static GstCaps *
gst_aravis_get_caps (GstBaseSrc * src)
{
	GstAravis* gst_aravis = GST_ARAVIS(src);
	GstCaps *caps;

	if (gst_aravis->all_caps != NULL)
		caps = gst_caps_copy (gst_aravis->all_caps);
	else
		caps = gst_caps_new_any ();

	GST_LOG_OBJECT (gst_aravis, "Available caps = %" GST_PTR_FORMAT, caps);

	return caps;
}

static gboolean
gst_aravis_set_caps (GstBaseSrc *src, GstCaps *caps)
{
	GstAravis* gst_aravis = GST_ARAVIS(src);
	GstStructure *structure;
	ArvPixelFormat pixel_format;
	int height, width;
	int bpp, depth;
	const GValue *frame_rate;
	const char *caps_string;
	unsigned int i;
	guint32 fourcc;

	GST_LOG_OBJECT (gst_aravis, "Requested caps = %" GST_PTR_FORMAT, caps);

	arv_camera_stop_acquisition (gst_aravis->camera);

	if (gst_aravis->stream != NULL)
		g_object_unref (gst_aravis->stream);

	structure = gst_caps_get_structure (caps, 0);

	gst_structure_get_int (structure, "width", &width);
	gst_structure_get_int (structure, "height", &height);
	frame_rate = gst_structure_get_value (structure, "framerate");
	gst_structure_get_fourcc (structure, "format", &fourcc);
	gst_structure_get_int (structure, "bpp", &bpp);
	gst_structure_get_int (structure, "depth", &depth);

	pixel_format = arv_pixel_format_from_gst_caps (gst_structure_get_name (structure), bpp, depth, fourcc);

	arv_camera_set_region (gst_aravis->camera, 0, 0, width, height);
	arv_camera_set_binning (gst_aravis->camera, gst_aravis->h_binning, gst_aravis->v_binning);
	arv_camera_set_pixel_format (gst_aravis->camera, pixel_format);

	if (frame_rate != NULL) {
		double dbl_frame_rate;

		dbl_frame_rate = (double) gst_value_get_fraction_numerator (frame_rate) /
			(double) gst_value_get_fraction_denominator (frame_rate);

		GST_DEBUG_OBJECT (gst_aravis, "Frame rate = %g Hz", dbl_frame_rate);
		arv_camera_set_frame_rate (gst_aravis->camera, dbl_frame_rate);

		if (dbl_frame_rate > 0.0)
			gst_aravis->buffer_timeout_us = MAX (GST_ARAVIS_BUFFER_TIMEOUT_DEFAULT,
							     3e6 / dbl_frame_rate);
		else
			gst_aravis->buffer_timeout_us = GST_ARAVIS_BUFFER_TIMEOUT_DEFAULT;
	} else
		gst_aravis->buffer_timeout_us = GST_ARAVIS_BUFFER_TIMEOUT_DEFAULT;

	GST_DEBUG_OBJECT (gst_aravis, "Buffer timeout = %Ld µs", gst_aravis->buffer_timeout_us);

	GST_DEBUG_OBJECT (gst_aravis, "Actual frame rate = %g Hz", arv_camera_get_frame_rate (gst_aravis->camera));

	GST_DEBUG_OBJECT (gst_aravis, "Gain       = %d", gst_aravis->gain);
	arv_camera_set_gain (gst_aravis->camera, gst_aravis->gain);
	GST_DEBUG_OBJECT (gst_aravis, "Actual gain       = %d", arv_camera_get_gain (gst_aravis->camera));

	GST_DEBUG_OBJECT (gst_aravis, "Exposure   = %g µs", gst_aravis->exposure_time_us);
	arv_camera_set_exposure_time (gst_aravis->camera, gst_aravis->exposure_time_us);
	GST_DEBUG_OBJECT (gst_aravis, "Actual exposure   = %g µs", arv_camera_get_exposure_time (gst_aravis->camera));

	if (gst_aravis->fixed_caps != NULL)
		gst_caps_unref (gst_aravis->fixed_caps);

	caps_string = arv_pixel_format_to_gst_caps_string (pixel_format);
	if (caps_string != NULL) {
		GstStructure *structure;
		GstCaps *caps;

		caps = gst_caps_new_empty ();
		structure = gst_structure_from_string (caps_string, NULL);
		gst_structure_set (structure,
				   "width", G_TYPE_INT, width,
				   "height", G_TYPE_INT, height,
				   NULL);

		if (frame_rate != NULL)
			gst_structure_set_value (structure, "framerate", frame_rate);

		gst_caps_append_structure (caps, structure);

		gst_aravis->fixed_caps = caps;
	} else
		gst_aravis->fixed_caps = NULL;

	gst_aravis->payload = arv_camera_get_payload (gst_aravis->camera);
	gst_aravis->stream = arv_camera_create_stream (gst_aravis->camera, NULL, NULL);

	for (i = 0; i < GST_ARAVIS_N_BUFFERS; i++)
		arv_stream_push_buffer (gst_aravis->stream,
					arv_buffer_new (gst_aravis->payload, NULL));

	GST_LOG_OBJECT (gst_aravis, "Start acquisition");
	arv_camera_start_acquisition (gst_aravis->camera);

	gst_aravis->timestamp_offset = 0;
	gst_aravis->last_timestamp = 0;

	return TRUE;
}

static gboolean
gst_aravis_start (GstBaseSrc *src)
{
	GstAravis* gst_aravis = GST_ARAVIS(src);

	GST_LOG_OBJECT (gst_aravis, "Open camera '%s'", gst_aravis->camera_name);

	if (gst_aravis->camera != NULL)
		g_object_unref (gst_aravis->camera);

	gst_aravis->camera = arv_camera_new (gst_aravis->camera_name);
	gst_aravis->all_caps = gst_aravis_get_all_camera_caps (gst_aravis);

	return TRUE;
}


gboolean gst_aravis_stop( GstBaseSrc * src )
{
        GstAravis* gst_aravis = GST_ARAVIS(src);

	arv_camera_stop_acquisition (gst_aravis->camera);

	if (gst_aravis->stream != NULL) {
		g_object_unref (gst_aravis->stream);
		gst_aravis->stream = NULL;

	}
	if (gst_aravis->camera != NULL) {
		g_object_unref (gst_aravis->camera);
		gst_aravis->camera = NULL;
	}
	if (gst_aravis->all_caps != NULL) {
		gst_caps_unref (gst_aravis->all_caps);
		gst_aravis->all_caps = NULL;
	}

        GST_DEBUG_OBJECT (gst_aravis, "Stop acquisition");

        return TRUE;
}

static void
gst_aravis_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
		      GstClockTime * start, GstClockTime * end)
{
	if (gst_base_src_is_live (basesrc)) {
		GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

		if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
			GstClockTime duration = GST_BUFFER_DURATION (buffer);

			if (GST_CLOCK_TIME_IS_VALID (duration)) {
				*end = timestamp + duration;
			}
			*start = timestamp;
		}
	} else {
		*start = -1;
		*end = -1;
	}
}

static GstFlowReturn
gst_aravis_create (GstPushSrc * push_src, GstBuffer ** buffer)
{
	GstAravis *gst_aravis;
	ArvBuffer *arv_buffer;

	gst_aravis = GST_ARAVIS (push_src);

	do {
		arv_buffer = arv_stream_timed_pop_buffer (gst_aravis->stream, gst_aravis->buffer_timeout_us);
		if (arv_buffer != NULL && arv_buffer->status != ARV_BUFFER_STATUS_SUCCESS)
			arv_stream_push_buffer (gst_aravis->stream, arv_buffer);
	} while (arv_buffer != NULL && arv_buffer->status != ARV_BUFFER_STATUS_SUCCESS);

	if (arv_buffer == NULL)
		return GST_FLOW_ERROR;

	*buffer = gst_buffer_new ();

	GST_BUFFER_DATA (*buffer) = arv_buffer->data;
	GST_BUFFER_MALLOCDATA (*buffer) = NULL;
	GST_BUFFER_SIZE (*buffer) = gst_aravis->payload;

	if (gst_aravis->timestamp_offset == 0) {
		gst_aravis->timestamp_offset = arv_buffer->timestamp_ns;
		gst_aravis->last_timestamp = arv_buffer->timestamp_ns;
	}

	GST_BUFFER_TIMESTAMP (*buffer) = arv_buffer->timestamp_ns - gst_aravis->timestamp_offset;
	GST_BUFFER_DURATION (*buffer) = arv_buffer->timestamp_ns - gst_aravis->last_timestamp;

	gst_aravis->last_timestamp = arv_buffer->timestamp_ns;

	arv_stream_push_buffer (gst_aravis->stream, arv_buffer);

	gst_buffer_set_caps (*buffer, gst_aravis->fixed_caps);

	return GST_FLOW_OK;
}

static void
gst_aravis_fixate_caps (GstPad * pad, GstCaps * caps)
{
	GstAravis *gst_aravis = GST_ARAVIS (gst_pad_get_parent_element (pad));
	GstStructure *structure;
	gint width;
	gint height;
	double frame_rate;

	g_return_if_fail (GST_IS_ARAVIS (gst_aravis));

	arv_camera_get_region (gst_aravis->camera, NULL, NULL, &width, &height);
	frame_rate = arv_camera_get_frame_rate (gst_aravis->camera);

	structure = gst_caps_get_structure (caps, 0);

	gst_structure_fixate_field_nearest_int (structure, "width", width);
	gst_structure_fixate_field_nearest_int (structure, "height", height);
	gst_structure_fixate_field_nearest_fraction (structure, "framerate", (double) (0.5 + frame_rate), 1);

	GST_LOG_OBJECT (gst_aravis, "Fixate caps");

	g_object_unref (gst_aravis);
}

static void
gst_aravis_init (GstAravis *gst_aravis, GstAravisClass *g_class)
{
	GstPad *pad = GST_BASE_SRC_PAD (gst_aravis);

	gst_pad_set_fixatecaps_function (pad, gst_aravis_fixate_caps);

	gst_base_src_set_live (GST_BASE_SRC (gst_aravis), TRUE);

	gst_aravis->camera_name = NULL;

	gst_aravis->gain = -1;
	gst_aravis->exposure_time_us = -1;
	gst_aravis->h_binning = -1;
	gst_aravis->v_binning = -1;
	gst_aravis->payload = 0;

	gst_aravis->buffer_timeout_us = GST_ARAVIS_BUFFER_TIMEOUT_DEFAULT;

	gst_aravis->camera = NULL;
	gst_aravis->stream = NULL;

	gst_aravis->all_caps = NULL;
	gst_aravis->fixed_caps = NULL;
}

static void
gst_aravis_finalize (GObject * object)
{
        GstAravis *gst_aravis = GST_ARAVIS (object);

	if (gst_aravis->camera != NULL) {
		g_object_unref (gst_aravis->camera);
		gst_aravis->camera = NULL;
	}
	if (gst_aravis->stream != NULL) {
		g_object_unref (gst_aravis->stream);
		gst_aravis->stream = NULL;
	}
	if (gst_aravis->all_caps != NULL) {
		gst_caps_unref (gst_aravis->all_caps);
		gst_aravis->all_caps = NULL;
	}
	if (gst_aravis->fixed_caps != NULL) {
		gst_caps_unref (gst_aravis->fixed_caps);
		gst_aravis->fixed_caps = NULL;
	}

	g_free (gst_aravis->camera_name);
	gst_aravis->camera_name = NULL;

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_aravis_set_property (GObject * object, guint prop_id,
			 const GValue * value, GParamSpec * pspec)
{
	GstAravis *gst_aravis = GST_ARAVIS (object);

	switch (prop_id) {
		case PROP_CAMERA_NAME:
			g_free (gst_aravis->camera_name);
			gst_aravis->camera_name = g_strdup (g_value_get_string (value));

			GST_LOG_OBJECT (gst_aravis, "Set camera name to %s", gst_aravis->camera_name);

			break;
		case PROP_GAIN:
			gst_aravis->gain = g_value_get_int (value);
			break;
		case PROP_EXPOSURE:
			gst_aravis->exposure_time_us = g_value_get_double (value);
			break;
		case PROP_H_BINNING:
			gst_aravis->h_binning = g_value_get_int (value);
			break;
		case PROP_V_BINNING:
			gst_aravis->v_binning = g_value_get_int (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_aravis_get_property (GObject * object, guint prop_id, GValue * value,
			 GParamSpec * pspec)
{
	GstAravis *gst_aravis = GST_ARAVIS (object);

	switch (prop_id) {
		case PROP_CAMERA_NAME:
			g_value_set_string (value, gst_aravis->camera_name);
			break;
		case PROP_GAIN:
			g_value_set_int (value, gst_aravis->gain);
			break;
		case PROP_EXPOSURE:
			g_value_set_double (value, gst_aravis->exposure_time_us);
			break;
		case PROP_H_BINNING:
			g_value_set_int (value, gst_aravis->h_binning);
			break;
		case PROP_V_BINNING:
			g_value_set_int (value, gst_aravis->v_binning);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_aravis_base_init (gpointer g_class)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_set_details_simple (element_class,
					      "Aravis Video Source",
					      "Source/Video",
					      "Aravis based source",
					      "Emmanuel Pacaud <emmanuel@gnome.org>");
	gst_element_class_add_pad_template (element_class,
					    gst_static_pad_template_get (&aravis_src_template));
}

static void
gst_aravis_class_init (GstAravisClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
	GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

	gobject_class->finalize = gst_aravis_finalize;
	gobject_class->set_property = gst_aravis_set_property;
	gobject_class->get_property = gst_aravis_get_property;

	g_object_class_install_property
		(gobject_class,
		 PROP_CAMERA_NAME,
		 g_param_spec_string ("camera-name",
				      "Camera name",
				      "Name of the camera",
				      NULL,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property
		(gobject_class,
		 PROP_GAIN,
		 g_param_spec_int ("gain",
				   "Gain",
				   "Gain (dB)",
				   0, 500, 0,
				   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property
		(gobject_class,
		 PROP_EXPOSURE,
		 g_param_spec_double ("exposure",
				      "Exposure",
				      "Exposure time (µs)",
				      0.0, 100000000.0, 500.0,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property
		(gobject_class,
		 PROP_H_BINNING,
		 g_param_spec_int ("h-binning",
				   "Horizontal binning",
				   "CCD horizontal binning",
				   1, G_MAXINT, 1,
				   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property
		(gobject_class,
		 PROP_V_BINNING,
		 g_param_spec_int ("v-binning",
				   "Vertical binning",
				   "CCD vertical binning",
				   1, G_MAXINT, 1,
				   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        GST_DEBUG_CATEGORY_INIT (aravis_debug, "aravissrc", 0, "Aravis interface");

	gstbasesrc_class->get_caps = gst_aravis_get_caps;
	gstbasesrc_class->set_caps = gst_aravis_set_caps;
	gstbasesrc_class->start = gst_aravis_start;
	gstbasesrc_class->stop = gst_aravis_stop;

	gstbasesrc_class->get_times = gst_aravis_get_times;

	gstpushsrc_class->create = gst_aravis_create;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
        return gst_element_register (plugin, "aravissrc", GST_RANK_NONE, GST_TYPE_ARAVIS);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		   GST_VERSION_MINOR,
		   "aravissrc",
		   "Aravis Video Source",
		   plugin_init,
		   VERSION,
		   "LGPL",
		   PACKAGE_NAME,
		   "http://blogs.gnome.org/emmanuel")


/* Generated data (by glib-mkenums) */

#include "arvenumtypes.h"
#include "arvenums.h"

GType
arv_gc_name_space_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_NAME_SPACE_STANDARD,
			  "ARV_GC_NAME_SPACE_STANDARD",
			  "standard" },
			{ ARV_GC_NAME_SPACE_CUSTOM,
			  "ARV_GC_NAME_SPACE_CUSTOM",
			  "custom" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcNameSpace"),
				values);
	}
	return the_type;
}

GType
arv_gc_access_mode_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_ACCESS_MODE_RO,
			  "ARV_GC_ACCESS_MODE_RO",
			  "ro" },
			{ ARV_GC_ACCESS_MODE_WO,
			  "ARV_GC_ACCESS_MODE_WO",
			  "wo" },
			{ ARV_GC_ACCESS_MODE_RW,
			  "ARV_GC_ACCESS_MODE_RW",
			  "rw" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcAccessMode"),
				values);
	}
	return the_type;
}

GType
arv_gc_cachable_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_CACHABLE_NO_CACHE,
			  "ARV_GC_CACHABLE_NO_CACHE",
			  "no-cache" },
			{ ARV_GC_CACHABLE_WRITE_TRHOUGH,
			  "ARV_GC_CACHABLE_WRITE_TRHOUGH",
			  "write-trhough" },
			{ ARV_GC_CACHABLE_WRITE_AROUND,
			  "ARV_GC_CACHABLE_WRITE_AROUND",
			  "write-around" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcCachable"),
				values);
	}
	return the_type;
}

GType
arv_acquisition_mode_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_ACQUISITION_MODE_CONTINUOUS,
			  "ARV_ACQUISITION_MODE_CONTINUOUS",
			  "continuous" },
			{ ARV_ACQUISITION_MODE_SINGLE_FRAME,
			  "ARV_ACQUISITION_MODE_SINGLE_FRAME",
			  "single-frame" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvAcquisitionMode"),
				values);
	}
	return the_type;
}

GType
arv_pixel_format_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_PIXEL_FORMAT_MONO_8,
			  "ARV_PIXEL_FORMAT_MONO_8",
			  "mono-8" },
			{ ARV_PIXEL_FORMAT_MONO_10,
			  "ARV_PIXEL_FORMAT_MONO_10",
			  "mono-10" },
			{ ARV_PIXEL_FORMAT_MONO_12,
			  "ARV_PIXEL_FORMAT_MONO_12",
			  "mono-12" },
			{ ARV_PIXEL_FORMAT_MONO_12_PACKED,
			  "ARV_PIXEL_FORMAT_MONO_12_PACKED",
			  "mono-12-packed" },
			{ ARV_PIXEL_FORMAT_MONO_16,
			  "ARV_PIXEL_FORMAT_MONO_16",
			  "mono-16" },
			{ ARV_PIXEL_FORMAT_BAYER_GR_8,
			  "ARV_PIXEL_FORMAT_BAYER_GR_8",
			  "bayer-gr-8" },
			{ ARV_PIXEL_FORMAT_BAYER_RG_8,
			  "ARV_PIXEL_FORMAT_BAYER_RG_8",
			  "bayer-rg-8" },
			{ ARV_PIXEL_FORMAT_BAYER_GB_8,
			  "ARV_PIXEL_FORMAT_BAYER_GB_8",
			  "bayer-gb-8" },
			{ ARV_PIXEL_FORMAT_BAYER_BG_8,
			  "ARV_PIXEL_FORMAT_BAYER_BG_8",
			  "bayer-bg-8" },
			{ ARV_PIXEL_FORMAT_BAYER_GR_10,
			  "ARV_PIXEL_FORMAT_BAYER_GR_10",
			  "bayer-gr-10" },
			{ ARV_PIXEL_FORMAT_BAYER_RG_10,
			  "ARV_PIXEL_FORMAT_BAYER_RG_10",
			  "bayer-rg-10" },
			{ ARV_PIXEL_FORMAT_BAYER_GB_10,
			  "ARV_PIXEL_FORMAT_BAYER_GB_10",
			  "bayer-gb-10" },
			{ ARV_PIXEL_FORMAT_BAYER_BG_10,
			  "ARV_PIXEL_FORMAT_BAYER_BG_10",
			  "bayer-bg-10" },
			{ ARV_PIXEL_FORMAT_BAYER_GR_12,
			  "ARV_PIXEL_FORMAT_BAYER_GR_12",
			  "bayer-gr-12" },
			{ ARV_PIXEL_FORMAT_BAYER_RG_12,
			  "ARV_PIXEL_FORMAT_BAYER_RG_12",
			  "bayer-rg-12" },
			{ ARV_PIXEL_FORMAT_BAYER_GB_12,
			  "ARV_PIXEL_FORMAT_BAYER_GB_12",
			  "bayer-gb-12" },
			{ ARV_PIXEL_FORMAT_BAYER_BG_12,
			  "ARV_PIXEL_FORMAT_BAYER_BG_12",
			  "bayer-bg-12" },
			{ ARV_PIXEL_FORMAT_RGB_8_PACKED,
			  "ARV_PIXEL_FORMAT_RGB_8_PACKED",
			  "rgb-8-packed" },
			{ ARV_PIXEL_FORMAT_BGR_8_PACKED,
			  "ARV_PIXEL_FORMAT_BGR_8_PACKED",
			  "bgr-8-packed" },
			{ ARV_PIXEL_FORMAT_RGBA_8_PACKED,
			  "ARV_PIXEL_FORMAT_RGBA_8_PACKED",
			  "rgba-8-packed" },
			{ ARV_PIXEL_FORMAT_BGRA_8_PACKED,
			  "ARV_PIXEL_FORMAT_BGRA_8_PACKED",
			  "bgra-8-packed" },
			{ ARV_PIXEL_FORMAT_RGB_10_PACKED,
			  "ARV_PIXEL_FORMAT_RGB_10_PACKED",
			  "rgb-10-packed" },
			{ ARV_PIXEL_FORMAT_BGR_10_PACKED,
			  "ARV_PIXEL_FORMAT_BGR_10_PACKED",
			  "bgr-10-packed" },
			{ ARV_PIXEL_FORMAT_RGB_12_PACKED,
			  "ARV_PIXEL_FORMAT_RGB_12_PACKED",
			  "rgb-12-packed" },
			{ ARV_PIXEL_FORMAT_BGR_12_PACKED,
			  "ARV_PIXEL_FORMAT_BGR_12_PACKED",
			  "bgr-12-packed" },
			{ ARV_PIXEL_FORMAT_YUV_411_PACKED,
			  "ARV_PIXEL_FORMAT_YUV_411_PACKED",
			  "yuv-411-packed" },
			{ ARV_PIXEL_FORMAT_YUV_422_PACKED,
			  "ARV_PIXEL_FORMAT_YUV_422_PACKED",
			  "yuv-422-packed" },
			{ ARV_PIXEL_FORMAT_YUV_444_PACKED,
			  "ARV_PIXEL_FORMAT_YUV_444_PACKED",
			  "yuv-444-packed" },
			{ ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED,
			  "ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED",
			  "bayer-gr-12-packed" },
			{ ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED,
			  "ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED",
			  "bayer-rg-12-packed" },
			{ ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED,
			  "ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED",
			  "bayer-bg-12-packed" },
			{ ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED,
			  "ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED",
			  "bayer-gb-12-packed" },
			{ ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED,
			  "ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED",
			  "yuv-422-yuyv-packed" },
			{ ARV_PIXEL_FORMAT_BAYER_GR_16,
			  "ARV_PIXEL_FORMAT_BAYER_GR_16",
			  "bayer-gr-16" },
			{ ARV_PIXEL_FORMAT_BAYER_RG_16,
			  "ARV_PIXEL_FORMAT_BAYER_RG_16",
			  "bayer-rg-16" },
			{ ARV_PIXEL_FORMAT_BAYER_GB_16,
			  "ARV_PIXEL_FORMAT_BAYER_GB_16",
			  "bayer-gb-16" },
			{ ARV_PIXEL_FORMAT_BAYER_BG_16,
			  "ARV_PIXEL_FORMAT_BAYER_BG_16",
			  "bayer-bg-16" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvPixelFormat"),
				values);
	}
	return the_type;
}

#include "arvcamera.h"

GType
arv_camera_vendor_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_CAMERA_VENDOR_UNKNOWN,
			  "ARV_CAMERA_VENDOR_UNKNOWN",
			  "unknown" },
			{ ARV_CAMERA_VENDOR_BASLER,
			  "ARV_CAMERA_VENDOR_BASLER",
			  "basler" },
			{ ARV_CAMERA_VENDOR_PROSILICA,
			  "ARV_CAMERA_VENDOR_PROSILICA",
			  "prosilica" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvCameraVendor"),
				values);
	}
	return the_type;
}

#include "arvgcregister.h"

GType
arv_gc_sign_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_SIGN_SIGNED,
			  "ARV_GC_SIGN_SIGNED",
			  "signed" },
			{ ARV_GC_SIGN_UNSIGNED,
			  "ARV_GC_SIGN_UNSIGNED",
			  "unsigned" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcSign"),
				values);
	}
	return the_type;
}

GType
arv_gc_register_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GC_REGISTER_TYPE_REGISTER,
			  "ARV_GC_REGISTER_TYPE_REGISTER",
			  "register" },
			{ ARV_GC_REGISTER_TYPE_INTEGER,
			  "ARV_GC_REGISTER_TYPE_INTEGER",
			  "integer" },
			{ ARV_GC_REGISTER_TYPE_MASKED_INTEGER,
			  "ARV_GC_REGISTER_TYPE_MASKED_INTEGER",
			  "masked-integer" },
			{ ARV_GC_REGISTER_TYPE_FLOAT,
			  "ARV_GC_REGISTER_TYPE_FLOAT",
			  "float" },
			{ ARV_GC_REGISTER_TYPE_STRING,
			  "ARV_GC_REGISTER_TYPE_STRING",
			  "string" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGcRegisterType"),
				values);
	}
	return the_type;
}

#include "arvstream.h"

GType
arv_stream_callback_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_STREAM_CALLBACK_TYPE_INIT,
			  "ARV_STREAM_CALLBACK_TYPE_INIT",
			  "init" },
			{ ARV_STREAM_CALLBACK_TYPE_EXIT,
			  "ARV_STREAM_CALLBACK_TYPE_EXIT",
			  "exit" },
			{ ARV_STREAM_CALLBACK_TYPE_START_BUFFER,
			  "ARV_STREAM_CALLBACK_TYPE_START_BUFFER",
			  "start-buffer" },
			{ ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE,
			  "ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE",
			  "buffer-done" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvStreamCallbackType"),
				values);
	}
	return the_type;
}

#include "arvbuffer.h"

GType
arv_buffer_status_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_BUFFER_STATUS_SUCCESS,
			  "ARV_BUFFER_STATUS_SUCCESS",
			  "success" },
			{ ARV_BUFFER_STATUS_CLEARED,
			  "ARV_BUFFER_STATUS_CLEARED",
			  "cleared" },
			{ ARV_BUFFER_STATUS_MISSING_BLOCKS,
			  "ARV_BUFFER_STATUS_MISSING_BLOCKS",
			  "missing-blocks" },
			{ ARV_BUFFER_STATUS_SIZE_MISMATCH,
			  "ARV_BUFFER_STATUS_SIZE_MISMATCH",
			  "size-mismatch" },
			{ ARV_BUFFER_STATUS_FILLING,
			  "ARV_BUFFER_STATUS_FILLING",
			  "filling" },
			{ ARV_BUFFER_STATUS_ABORTED,
			  "ARV_BUFFER_STATUS_ABORTED",
			  "aborted" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvBufferStatus"),
				values);
	}
	return the_type;
}

#include "arvgvcp.h"

GType
arv_gvcp_packet_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVCP_PACKET_TYPE_ACK,
			  "ARV_GVCP_PACKET_TYPE_ACK",
			  "ack" },
			{ ARV_GVCP_PACKET_TYPE_RESEND,
			  "ARV_GVCP_PACKET_TYPE_RESEND",
			  "resend" },
			{ ARV_GVCP_PACKET_TYPE_CMD,
			  "ARV_GVCP_PACKET_TYPE_CMD",
			  "cmd" },
			{ ARV_GVCP_PACKET_TYPE_ERROR,
			  "ARV_GVCP_PACKET_TYPE_ERROR",
			  "error" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvcpPacketType"),
				values);
	}
	return the_type;
}

GType
arv_gvcp_command_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVCP_COMMAND_DISCOVERY_CMD,
			  "ARV_GVCP_COMMAND_DISCOVERY_CMD",
			  "discovery-cmd" },
			{ ARV_GVCP_COMMAND_DISCOVERY_ACK,
			  "ARV_GVCP_COMMAND_DISCOVERY_ACK",
			  "discovery-ack" },
			{ ARV_GVCP_COMMAND_BYE_CMD,
			  "ARV_GVCP_COMMAND_BYE_CMD",
			  "bye-cmd" },
			{ ARV_GVCP_COMMAND_BYE_ACK,
			  "ARV_GVCP_COMMAND_BYE_ACK",
			  "bye-ack" },
			{ ARV_GVCP_COMMAND_PACKET_RESEND_CMD,
			  "ARV_GVCP_COMMAND_PACKET_RESEND_CMD",
			  "packet-resend-cmd" },
			{ ARV_GVCP_COMMAND_PACKET_RESEND_ACK,
			  "ARV_GVCP_COMMAND_PACKET_RESEND_ACK",
			  "packet-resend-ack" },
			{ ARV_GVCP_COMMAND_READ_REGISTER_CMD,
			  "ARV_GVCP_COMMAND_READ_REGISTER_CMD",
			  "read-register-cmd" },
			{ ARV_GVCP_COMMAND_READ_REGISTER_ACK,
			  "ARV_GVCP_COMMAND_READ_REGISTER_ACK",
			  "read-register-ack" },
			{ ARV_GVCP_COMMAND_WRITE_REGISTER_CMD,
			  "ARV_GVCP_COMMAND_WRITE_REGISTER_CMD",
			  "write-register-cmd" },
			{ ARV_GVCP_COMMAND_WRITE_REGISTER_ACK,
			  "ARV_GVCP_COMMAND_WRITE_REGISTER_ACK",
			  "write-register-ack" },
			{ ARV_GVCP_COMMAND_READ_MEMORY_CMD,
			  "ARV_GVCP_COMMAND_READ_MEMORY_CMD",
			  "read-memory-cmd" },
			{ ARV_GVCP_COMMAND_READ_MEMORY_ACK,
			  "ARV_GVCP_COMMAND_READ_MEMORY_ACK",
			  "read-memory-ack" },
			{ ARV_GVCP_COMMAND_WRITE_MEMORY_CMD,
			  "ARV_GVCP_COMMAND_WRITE_MEMORY_CMD",
			  "write-memory-cmd" },
			{ ARV_GVCP_COMMAND_WRITE_MEMORY_ACK,
			  "ARV_GVCP_COMMAND_WRITE_MEMORY_ACK",
			  "write-memory-ack" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvcpCommand"),
				values);
	}
	return the_type;
}

#include "arvgvsp.h"

GType
arv_gvsp_packet_type_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GVSP_PACKET_TYPE_DATA_LEADER,
			  "ARV_GVSP_PACKET_TYPE_DATA_LEADER",
			  "leader" },
			{ ARV_GVSP_PACKET_TYPE_DATA_TRAILER,
			  "ARV_GVSP_PACKET_TYPE_DATA_TRAILER",
			  "trailer" },
			{ ARV_GVSP_PACKET_TYPE_DATA_BLOCK,
			  "ARV_GVSP_PACKET_TYPE_DATA_BLOCK",
			  "block" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvspPacketType"),
				values);
	}
	return the_type;
}

#include "arvgvstream.h"

GType
arv_gv_stream_option_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GV_STREAM_OPTION_SOCKET_BUFFER_FIXED,
			  "ARV_GV_STREAM_OPTION_SOCKET_BUFFER_FIXED",
			  "fixed" },
			{ ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO,
			  "ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO",
			  "auto" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvStreamOption"),
				values);
	}
	return the_type;
}

GType
arv_gv_stream_packet_resend_get_type (void)
{
	static GType the_type = 0;

	if (the_type == 0)
	{
		static const GEnumValue values[] = {
			{ ARV_GV_STREAM_PACKET_RESEND_NEVER,
			  "ARV_GV_STREAM_PACKET_RESEND_NEVER",
			  "never" },
			{ ARV_GV_STREAM_PACKET_RESEND_ALWAYS,
			  "ARV_GV_STREAM_PACKET_RESEND_ALWAYS",
			  "always" },
			{ 0, NULL, NULL }
		};
		the_type = g_enum_register_static (
				g_intern_static_string ("ArvGvStreamPacketResend"),
				values);
	}
	return the_type;
}


/* Generated data ends here */


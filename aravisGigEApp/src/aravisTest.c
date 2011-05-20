#include <arv.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define NRAW 5
#define WIDTH 320
#define HEIGHT 240
static ArvCamera *camera;
static ArvStream *stream;
GMutex *mutex;
static int store_payload = 0;

ArvBuffer *
allocBuffer() {
	unsigned int payload;	
	payload = arv_camera_get_payload (camera);
	if (payload != store_payload) {
		g_printerr("Old Payload: %d, new payload: %d\n", store_payload, payload);
		store_payload = payload;
	}
	return arv_buffer_new (payload, NULL);
}

void
arv_viewer_new_buffer_cb (ArvStream *stream, void *nothing)
{
	ArvBuffer *arv_buffer;
	unsigned int y, index;

	arv_buffer = arv_stream_pop_buffer (stream);
	if (arv_buffer == NULL)
		return;

	if (arv_buffer->status == ARV_BUFFER_STATUS_SUCCESS) {
		/* Do stuff with buffer here
		 * In reality I have user data attached to buffer
		 * and do my own memory management, but I won't put that in here */
		/* print out 640*480 buffer to pass to mplayer
		 * aravisTest | mplayer -demuxer rawvideo -rawvideo w=320:h=240:format=y8:fps=15*/
		if (arv_buffer->width * arv_buffer->height != arv_buffer->size) {
			g_printerr("Frame w:%d h:%d size:%d\n", arv_buffer->width, arv_buffer->height, arv_buffer->size );
		} else {
			/*for (y = 0; y < HEIGHT; y++) {
				index = (arv_buffer->height - HEIGHT + y)*arv_buffer->width + arv_buffer->width - WIDTH;
				fwrite(((char *) arv_buffer->data) + index, 1, WIDTH, stdout);
			}*/
		}
		g_object_unref(arv_buffer);
		g_mutex_lock(mutex);		
		arv_buffer = allocBuffer();
		g_mutex_unlock(mutex);
	}

	arv_stream_push_buffer (stream, arv_buffer);
}

/* set dimensions to w x h then get frames for 5 seconds */
void getFramesAttempt1(int w, int h) {
	int i;
	guint64 n_completed_buffers, n_failures, n_underruns;
    g_mutex_lock(mutex);
    g_printerr("getFramesAttempt1: Setting region to %d x %d\n", w, h);
    arv_camera_set_region(camera, 0, 0, w, h);
    g_mutex_unlock(mutex);
    for (i = 0; i<10; i++) {
    	g_mutex_lock(mutex);
    	arv_stream_get_statistics(stream, &n_completed_buffers, &n_failures, &n_underruns);
    	g_printerr("Completed: %llu, Failures: %llu, Underruns: %llu\n", n_completed_buffers, n_failures, n_underruns);
    	g_mutex_unlock(mutex);
    	g_usleep(1000000);
    }
}

/* set dimensions to w x h then get frames for 5 seconds */
void getFramesAttempt2(int w, int h) {
	int i;
	guint64 n_completed_buffers, n_failures, n_underruns;
	ArvBuffer *buffer;
    g_mutex_lock(mutex);
    g_printerr("getFramesAttempt2: Setting region to %d x %d\n", w, h);
    arv_camera_stop_acquisition(camera);
    arv_camera_set_region(camera, 0, 0, w, h);
	buffer = arv_stream_pop_input_buffer (stream);
	while (buffer != NULL) {
		g_object_unref(buffer);
		buffer = arv_stream_pop_input_buffer (stream);
	}
    /* fill the queue */
    for (i=0; i<NRAW; i++) {
    	arv_stream_push_buffer (stream, allocBuffer());
    }
    arv_camera_start_acquisition(camera);
    g_mutex_unlock(mutex);
    for (i = 0; i<10; i++) {
    	g_mutex_lock(mutex);
    	arv_stream_get_statistics(stream, &n_completed_buffers, &n_failures, &n_underruns);
    	g_printerr("Completed: %llu, Failures: %llu, Underruns: %llu\n", n_completed_buffers, n_failures, n_underruns);
    	g_mutex_unlock(mutex);
    	g_usleep(1000000);
    }
}

int
main (int argc,char *argv[])
{
	int i;
    g_thread_init (NULL);
    g_type_init ();
    mutex = g_mutex_new ();
    g_mutex_lock(mutex);
    camera = arv_camera_new("Prosilica-02-2166A-06844");
	/* create the stream */
	stream = arv_camera_create_stream (camera, NULL, NULL);
	arv_stream_set_emit_signals (stream, TRUE);
	g_signal_connect (stream, "new-buffer", G_CALLBACK (arv_viewer_new_buffer_cb), NULL);

    /* fill the queue */
    for (i=0; i<NRAW; i++) {
    	arv_stream_push_buffer (stream, allocBuffer());
    }

    /* start the camera */
    arv_camera_start_acquisition(camera);
    g_mutex_unlock(mutex);

    /* do test */
    getFramesAttempt1(800, 600);
    getFramesAttempt1(1024, 768);
    getFramesAttempt1(800, 600);
    getFramesAttempt1(1024, 768);
    
    getFramesAttempt2(800, 600);
    getFramesAttempt2(1024, 768);
    getFramesAttempt2(800, 600);
    getFramesAttempt2(1024, 768);

    /* stop */
    arv_camera_stop_acquisition(camera);
    g_object_unref(stream);
    g_object_unref(camera);
    return 0;
}

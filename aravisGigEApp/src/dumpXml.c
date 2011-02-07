#include <arv.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
	unsigned int n_devices;
	const char *xml;
	ArvDevice *device;
	size_t size;
	const char *device_id;	
	char filename[256];
	FILE *fd;

	g_type_init ();

	arv_update_device_list ();
	n_devices = arv_get_n_devices ();
	if (n_devices < 1)
		printf ("No device found\n");
	else {
		unsigned int i;
/*		guint j;*/

		for (i = 0; i < n_devices; i++) {
			device_id = arv_get_device_id (i);
			if (device_id != NULL && strcmp(device_id, "Fake_1") != 0) {
				device = ARV_DEVICE(arv_open_device(device_id));
/*				ArvGc *genicam;
				genicam = arv_device_get_genicam(ARV_DEVICE(gv_device));
				GList *keys;
				keys = g_hash_table_get_keys(genicam->nodes);
				for (j = 0; j<g_list_length(keys); j++) {
					printf("%s\n", g_list_nth_data(keys, j));
				}*/
				xml = arv_device_get_genicam_xml(device, &size);
				if (xml != NULL) {
					sprintf(filename, "./%s.xml", device_id);
					printf("Writing xml file '%s' ... ",  filename);
					fd = fopen(filename, "w");
					size = fwrite(xml, 1, size, fd);
					fclose(fd);
					printf("Done\n");					
				}
			}
		}
	}

	return EXIT_SUCCESS;
}	

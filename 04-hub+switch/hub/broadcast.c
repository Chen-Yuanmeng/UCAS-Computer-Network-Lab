#include "base.h"
#include <stdio.h>

extern ustack_t *instance;

// the memory of ``packet'' will be free'd in handle_packet().
void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: broadcast packet 
	// instance saves all the interfaces in instance->iface_list
	iface_info_t *pos = NULL;

	list_for_each_entry(pos, &instance->iface_list, list) {
		if (pos != iface) {
			iface_send_packet(pos, packet, len);
		}
	}
}

#include "mac.h"
#include "log.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

mac_port_map_t mac_port_map;

u8 hash_mac(u8 mac[ETH_ALEN])
{
	int hash =  mac[0] ^ (mac[1] << 1) ^ (mac[2] << 2) ^ (mac[3] << 3) ^ (mac[4] << 4) ^ (mac[5] << 5);
	return hash & (HASH_8BITS - 1);
}

// initialize mac_port table
void init_mac_port_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	for (int i = 0; i < HASH_8BITS; i++) {
		init_list_head(&mac_port_map.hash_table[i]);
	}

	pthread_mutex_init(&mac_port_map.lock, NULL);

	pthread_create(&mac_port_map.thread, NULL, sweeping_mac_port_thread, NULL);
}

// destroy mac_port table
void destory_mac_port_table()
{
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *entry, *q;
	for (int i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
			list_delete_entry(&entry->list);
			free(entry);
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

// lookup the mac address in mac_port table
iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{
	// TODO: implement the lookup process here
	// fprintf(stdout, "TODO: implement the lookup process here.\n");

	u8 hash = hash_mac(mac);
	mac_port_entry_t *pos = NULL;

	pthread_mutex_lock(&mac_port_map.lock);

	list_for_each_entry(pos, &mac_port_map.hash_table[hash], list) {
		if (memcmp(pos->mac, mac, ETH_ALEN) == 0) {
			pthread_mutex_unlock(&mac_port_map.lock);
			return pos->iface;
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
	return NULL;
}

// insert the mac -> iface mapping into mac_port table
void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	// TODO: implement the insertion process here
	// fprintf(stdout, "TODO: implement the insertion process here.\n");

	u8 hash = hash_mac(mac);

	pthread_mutex_lock(&mac_port_map.lock);

	mac_port_entry_t *pos = NULL;
	list_for_each_entry(pos, &mac_port_map.hash_table[hash], list) {
		if (memcmp(pos->mac, mac, ETH_ALEN) == 0) {
			pos->iface = iface;
			pos->visited = time(NULL);
			pthread_mutex_unlock(&mac_port_map.lock);
			return;
		}
	}

	mac_port_entry_t *new_entry = malloc(sizeof(mac_port_entry_t));
	if (!new_entry) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memcpy(new_entry->mac, mac, ETH_ALEN);
	new_entry->iface = iface;
	new_entry->visited = time(NULL);
	list_add_head(&new_entry->list, &mac_port_map.hash_table[hash]);
	pthread_mutex_unlock(&mac_port_map.lock);
}

// dumping mac_port table
void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry(entry, &mac_port_map.hash_table[i], list) {
			fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac), \
					entry->iface->name, (int)(now - entry->visited));
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

// sweeping mac_port table, remove the entry which has not been visited in the
// last 30 seconds.
int sweep_aged_mac_port_entry()
{
	// TODO: implement the sweeping process here
	// fprintf(stdout, "TODO: implement the sweeping process here.\n");

	time_t now = time(NULL);
	int removed_count = 0;
	pthread_mutex_lock(&mac_port_map.lock);

	for (int hash = 0; hash < HASH_8BITS; hash++) {
		mac_port_entry_t *pos, *q;
		list_for_each_entry_safe(pos, q, &mac_port_map.hash_table[hash], list) {
			if ((int)(now - pos->visited) >= MAC_PORT_TIMEOUT) {
				list_delete_entry(&pos->list);
				free(pos);
				removed_count++;
			}
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
	return removed_count;
}

// sweeping mac_port table periodically, by calling sweep_aged_mac_port_entry
void *sweeping_mac_port_thread(void *nil)
{
	while (1) {
		sleep(1);
		int n = sweep_aged_mac_port_entry();

		if (n > 0)
			log(DEBUG, "%d aged entries in mac_port table are removed.", n);
	}

	return NULL;
}

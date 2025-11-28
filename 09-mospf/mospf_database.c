#include "mospf_database.h"
#include "ip.h"
#include "rtable.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "types.h"
#include "list.h"

#include "log.h"

#define BIG_INT 0x3fffffff

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>

struct list_head mospf_db;

int node_num;

static u32 *rid_arr;
static mospf_db_entry_t **db_entry_arr;
static int rid_arr_num;
static int **graph;
static int *dist;
static int *prev;
static bool *visited;


void init_mospf_db()
{
	init_list_head(&mospf_db);
	node_num = 0;
}


// Find rid in rid_arr, return its index, or -1 if not found
static int find_rid_index(u32 rid)
{
	for (int i = 0; i < rid_arr_num; i++) {
		if (rid_arr[i] == rid) {
			return i;
		}
	}
	return -1;
}

static void init_mospf_graph()
{
	rid_arr = (u32 *)malloc(sizeof(u32) * node_num);
	db_entry_arr = (mospf_db_entry_t **)malloc(sizeof(mospf_db_entry_t *) * node_num);

	mospf_db_entry_t *entry = NULL;
	rid_arr_num = 0;
	list_for_each_entry(entry, &mospf_db, list) {
		rid_arr[rid_arr_num] = entry->rid;
		db_entry_arr[rid_arr_num] = entry;
		rid_arr_num++;
	}

	graph = (int **)malloc(sizeof(int *) * rid_arr_num);

	for (int i = 0; i < rid_arr_num; i++) {
		graph[i] = (int *)malloc(sizeof(int) * rid_arr_num);
		for (int j = 0; j < rid_arr_num; j++) {
			graph[i][j] = (i == j) ? 0 : BIG_INT;
		}
	}

	// Fill the graph adjacency matrix
	mospf_db_entry_t *src_entry = NULL;
	list_for_each_entry(src_entry, &mospf_db, list) {
		int src_index = find_rid_index(src_entry->rid);
		if (src_index == -1) continue;

		for (int i = 0; i < src_entry->nadv; i++) {
			u32 neighbor_rid = src_entry->array[i].rid;
			int neighbor_index = find_rid_index(neighbor_rid);
			if (neighbor_index != -1) {
				graph[src_index][neighbor_index] = 1; // Assuming cost of 1 for each link
			}
		}
	}

	dist = (int *)malloc(sizeof(int) * rid_arr_num);
	prev = (int *)malloc(sizeof(int) * rid_arr_num);
	visited = (bool *)malloc(sizeof(bool) * rid_arr_num);

	memset(visited, false, sizeof(bool) * rid_arr_num);
	for (int i = 0; i < rid_arr_num; i++) {
		dist[i] = BIG_INT;
		prev[i] = -1;
	}
}

static void destroy_mospf_graph()
{
	free(rid_arr);
	free(db_entry_arr);

	for (int i = 0; i < node_num; i++) {
		free(graph[i]);
	}
	free(graph);

	free(dist);
	free(prev);
	free(visited);
}


static int find_min_dist_index(bool cond)
{
	int min_index = -1;
	int min_dist = BIG_INT;

	for (int i = 0; i < rid_arr_num; i++) {
		if (visited[i] == cond && dist[i] < min_dist) {
			min_dist = dist[i];
			min_index = i;
		}
	}

	return min_index;
}

static void dijkstra(int src_index)
{
	for (int i = 0; i < rid_arr_num; i++) {
		dist[i] = BIG_INT;
		prev[i] = -1;
		visited[i] = false;
	}

	dist[src_index] = 0;

	for (int i = 0; i < rid_arr_num; i++) {
		int u = find_min_dist_index(false);
		if (u == -1) {
			break; // All reachable nodes have been visited
		}
		visited[u] = true;

		for (int v = 0; v < rid_arr_num; v++) {
			int alt = dist[u] + graph[u][v];
			if (!visited[v] && alt < dist[v]) {
				dist[v] = alt;
				prev[v] = u;
			}
		}
	}
}

static void print_mospf_graph()
{
	for (int i = 0; i < rid_arr_num; i++) {
		char line[256] = {0};
		size_t idx = 0;
		for (int j = 0; j < rid_arr_num; j++) {
			if (graph[i][j] == BIG_INT) {
				snprintf(line + idx, sizeof(line) - idx, " INF");
				idx += 4;
			} else {
				snprintf(line + idx, sizeof(line) - idx, "%3d ", graph[i][j]);
				idx += 4;
			}
		}
	}
}

static int get_first_hop(int i) {
	while (i >= 0 && dist[i] > 1) {
		i = prev[i];
	}
	return i;
}

static u32 get_iface_ip_of_nbr(u32 nbr_rid, iface_info_t **iface)
{
	iface_info_t *iface_info = NULL;
	list_for_each_entry(iface_info, &instance->iface_list, list) {
		mospf_nbr_t *nbr = NULL;
		list_for_each_entry(nbr, &iface_info->nbr_list, list) {
			if (nbr->nbr_id == nbr_rid) {
				*iface = iface_info;
				return nbr->nbr_ip;

			}
		}
	}
	return 0; // Not found
}

static void set_rtable_with_path(int source) {
	while (1) {
		int dst = find_min_dist_index(true);

		if (dst < 0) {
			break;
		}

		visited[dst] = false;

		if (dst == source) {
			continue;
		}

		if (dist[dst] == BIG_INT) {
			continue;
		}

		u32 dst_rid = rid_arr[dst];
		mospf_db_entry_t *dst_entry = db_entry_arr[dst];

		int hop = get_first_hop(dst);
		if (hop < 0) {
			continue; // No first hop found
		}

		u32 next_hop_rid = rid_arr[hop];

		iface_info_t *hop_iface = NULL;
		u32 next_hop_ip = get_iface_ip_of_nbr(next_hop_rid, &hop_iface);
		if (next_hop_ip == 0 || hop_iface == NULL) continue; // Next hop not found

		for (int i = 0; i < dst_entry->nadv; i++) {
			struct mospf_lsa *lsa = dst_entry->array + i;
			try_add_rt_entry(lsa->network, lsa->mask, next_hop_ip, hop_iface);
		}
	}
}


void update_routing_table_from_database()
{
	// fprintf(stdout, "TODO: update routing table from mOSPF database.\n");
	
	init_mospf_graph();

	int src_index = find_rid_index(instance->router_id);
	if (src_index == -1) {
		return;
	}

	dijkstra(src_index);  // Calculate shortest paths

	// Update routing table based on shortest paths
	pthread_mutex_lock(&rtable_mutex);

	clear_rtable();
	
	load_rtable_from_kernel();

	set_rtable_with_path(src_index);

	print_rtable();

	pthread_mutex_unlock(&rtable_mutex);

	destroy_mospf_graph();

	print_mospf_db();
}

void check_mospf_db_timeout()
{
	// fprintf(stdout, "TODO: check mOSPF database entry timeout.\n");

	// Iterate all entries in the link state database
	// If any entry times out, remove it from the database
	int updated = 0;

	mospf_db_entry_t *entry = NULL, *nxt = NULL;
	list_for_each_entry_safe(entry, nxt, &mospf_db, list) {
		entry->alive += 1;
		if (entry->alive >= MOSPF_DATABASE_TIMEOUT) {
			// Remove this entry from the link state database
			list_delete_entry(&entry->list);
			free(entry->array);
			free(entry);
			node_num--;

			updated = 1;
		}
	}

	if (updated) {
		update_routing_table_from_database();
	}
}

int update_mospf_db(char *lsu_msg)
{
	// fprintf(stdout, "TODO: update mOSPF database from received LSU message.\n");

	struct mospf_hdr *hdr = (struct mospf_hdr *)lsu_msg;
	struct mospf_lsu *lsu = (struct mospf_lsu *)(lsu_msg + MOSPF_HDR_SIZE);
	struct mospf_lsa *lsa = (struct mospf_lsa *)((char *)lsu + MOSPF_LSU_SIZE);

	u32 rid = ntohl(hdr->rid);
	u16 seq = ntohs(lsu->seq);
	u32 nadv = ntohl(lsu->nadv);

	mospf_db_entry_t *entry = NULL, *match = NULL;

	list_for_each_entry(entry, &mospf_db, list) {
		if (entry->rid == rid) {
			match = entry;
			break;
		}
	}

	if (!match) {
		match = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		list_add_tail(&match->list, &mospf_db);
		match->rid = rid;
		node_num++;
	}
	else if (seq > match->seq) {
		if (match->array) {
			free(match->array);
		}
	}
	else {
		return 0;  // No need to update
	}

	match->seq = seq;
	match->alive = 0;
	match->nadv = nadv;

	size_t arr_size = MOSPF_LSA_SIZE * nadv;
	match->array = (struct mospf_lsa *)malloc(arr_size);
	for (int i = 0; i < nadv; i++) {
		match->array[i].mask = ntohl(lsa[i].mask);
		match->array[i].network = ntohl(lsa[i].network);
		match->array[i].rid = ntohl(lsa[i].rid);
	}
	return 1;  // Updated
}

// print the mOSPF database for debugging after updating
// For each entry, print RID, network, mask, and neighbor RID
void print_mospf_db()
{
	mospf_db_entry_t *entry = NULL;

	printf("mOSPF Database entries:\n");
	printf("RID\t\tNetwork\t\tMask\t\tNeighbor RID\n");
	list_for_each_entry(entry, &mospf_db, list) {
		if (entry->rid == instance->router_id) {
			continue; // Skip self entry
		}
		for (int i = 0; i < entry->nadv; i++) {
			struct mospf_lsa *lsa = &entry->array[i];
			printf("" IP_FMT "\t" IP_FMT "\t" IP_FMT "\t" IP_FMT "\n",
					HOST_IP_FMT_STR(entry->rid),
					HOST_IP_FMT_STR(lsa->network),
					HOST_IP_FMT_STR(lsa->mask),
					HOST_IP_FMT_STR(lsa->rid));
		}
	}
}

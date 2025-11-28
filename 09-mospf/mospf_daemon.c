#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;
pthread_cond_t lsu_send_cond;  // Send LSU message at 1. nbr change 2. timeout 30s

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);
	pthread_cond_init(&lsu_send_cond, NULL);
	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	while (1)
	{
		sleep(MOSPF_DEFAULT_HELLOINT);

		iface_info_t *iface = NULL;

		// Iterate over all interfaces and send mOSPF Hello packet
		list_for_each_entry(iface, &instance->iface_list, list) {
			char *pkt = NULL;
			int pkt_len = prepare_mospf_hello(iface, &pkt);
			if (pkt_len <= 0) {
				log(ERROR, "failed to prepare mOSPF Hello packet.");
				return NULL;
			}
			iface_send_packet(iface, pkt, pkt_len);
		}
	}

	return NULL;
}

void *checking_nbr_thread(void *param)
{
	// fprintf(stdout, "TODO: neighbor list timeout operation.\n");

	while (1) {
		sleep(1);

		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface = NULL;

		int updated = 0;

		// Iterate all interfaces
		list_for_each_entry(iface, &instance->iface_list, list) {
			mospf_nbr_t *nbr = NULL, *nxt = NULL;

			// Iterate all neighbors in this interface
			list_for_each_entry_safe(nbr, nxt, &iface->nbr_list, list) {
				nbr->alive += 1;
				if (nbr->alive >= 3 * iface->helloint) {
					// Remove this neighbor from the neighbor list
					list_delete_entry(&nbr->list);
					free(nbr);
					iface->num_nbr -= 1;
					updated = 1;
				}
			}
		}
		
		if (updated) {
			pthread_cond_signal(&lsu_send_cond);
		}

		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void *checking_database_thread(void *param)
{
	// fprintf(stdout, "TODO: link state database timeout operation.\n");

	while (1) {
		sleep(1);

		pthread_mutex_lock(&mospf_lock);

		check_mospf_db_timeout();

		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr((char *)packet);
	struct mospf_hdr *mospf_hdr = (struct mospf_hdr *)IP_DATA(ip_hdr);
	struct mospf_hello *mospf_hello = (struct mospf_hello *)((char *)mospf_hdr + MOSPF_HDR_SIZE);

	u32 nid = ntohl(mospf_hdr->rid);
	u32 nmask = ntohl(mospf_hello->mask);
	u32 nip = ntohl(ip_hdr->saddr);

	pthread_mutex_lock(&mospf_lock);

	// Iterate the neighbor list. If the received packet is not in the list, 
	// add it; else update its arrival time.

	mospf_nbr_t *nbr = NULL, *match = NULL;

	list_for_each_entry(nbr, &iface->nbr_list, list) {
		if (nbr->nbr_id == nid) {
			match = nbr;
			break;
		}
	}

	if (match) { // Found
		match->alive = 0;
	}
	else { // Not found
		mospf_nbr_t *new_nbr = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
		new_nbr->nbr_id = nid;
		new_nbr->nbr_ip = nip;
		new_nbr->nbr_mask = nmask;
		new_nbr->alive = 0;
		list_add_tail(&new_nbr->list, &iface->nbr_list);
		iface->num_nbr += 1;

		pthread_cond_signal(&lsu_send_cond);  // Notify LSU sending thread
	}

	pthread_mutex_unlock(&mospf_lock);
}

static void send_mospf_lsu_msg(const char *msg, int msg_len, iface_info_t *ignore) {
	iface_info_t *iface = NULL;

	list_for_each_entry(iface, &instance->iface_list, list) {
		if (iface == ignore) {
			// Don't send to the ignored interface
			continue;
		}
		
		mospf_nbr_t *nbr = NULL;

		list_for_each_entry(nbr, &iface->nbr_list, list) {
			int pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + msg_len;
			char *pkt = (char *)malloc(pkt_len);

			struct iphdr *ip = packet_to_ip_hdr(pkt);
			ip_init_hdr(ip, iface->ip, nbr->nbr_ip, IP_BASE_HDR_SIZE + msg_len, IPPROTO_MOSPF);
			memcpy(IP_DATA(ip), msg, msg_len);
			ip_send_packet(pkt, pkt_len);
		}
	}
}

void *sending_mospf_lsu_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");

	// Send LSU at 1. signal 2. timeout 30s
	pthread_mutex_lock(&mospf_lock);

	while (1) {
		struct timespec end_time;
		clock_gettime(CLOCK_REALTIME, &end_time);
		end_time.tv_sec += MOSPF_DEFAULT_LSUINT;
		pthread_cond_timedwait(&lsu_send_cond, &mospf_lock, &end_time);

		char *pkt = NULL;
		int pkt_len = prepare_lsu_msg(&pkt);
		update_mospf_db(pkt);
		if (pkt_len < 0) {
			log(ERROR, "falied to prepare LSU packet");
		}
		else {
			send_mospf_lsu_msg(pkt, pkt_len, NULL);
			free(pkt);
		}

		instance->sequence_num += 1;  // LSU sequence number increase
	}

	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr((char *)packet);
	struct mospf_hdr *mospf_hdr = (struct mospf_hdr *)IP_DATA(ip_hdr);
	struct mospf_lsu *mospf_lsu = (struct mospf_lsu *)((char *)mospf_hdr + MOSPF_HDR_SIZE);

	pthread_mutex_lock(&mospf_lock);

	if (update_mospf_db((char *)mospf_hdr)) {
		update_routing_table_from_database();

		// Transfer LSU message to other neighbors except the one it comes from
		mospf_lsu->ttl -= 1;
		mospf_hdr->checksum = mospf_checksum(mospf_hdr);
		if (mospf_lsu->ttl > 0) {
			int lsu_msg_len = len - ETHER_HDR_SIZE - IP_HDR_SIZE(ip_hdr);
			send_mospf_lsu_msg((char *)mospf_hdr, lsu_msg_len, iface);
		}
	}

	pthread_mutex_unlock(&mospf_lock);
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}

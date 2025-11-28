#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "base.h"
#include "ip.h"
#include "log.h"
#include "ether.h"
#include <stdlib.h>
#include <arpa/inet.h>

extern ustack_t *instance;

void mospf_init_hdr(struct mospf_hdr *mospf, u8 type, u16 len, u32 rid, u32 aid)
{
	mospf->version = MOSPF_VERSION;
	mospf->type = type;
	mospf->len = htons(len);
	mospf->rid = htonl(rid);
	mospf->aid = htonl(aid);
	mospf->padding = 0;
}

void mospf_init_hello(struct mospf_hello *hello, u32 mask)
{
	hello->mask = htonl(mask);
	hello->helloint = htons(MOSPF_DEFAULT_HELLOINT);
	hello->padding = 0;
}

void mospf_init_lsu(struct mospf_lsu *lsu, u32 nadv)
{
	lsu->seq = htons(instance->sequence_num);
	lsu->unused = 0;
	lsu->ttl = MOSPF_MAX_LSU_TTL;
	lsu->nadv = htonl(nadv);
}

int prepare_mospf_hello(iface_info_t *iface, char **packet)
{
	int pkt_len = MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;   // mOSPF packet size
	int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + pkt_len;  // full packet size

	*packet = malloc(len);
	if (!*packet) {
		log(ERROR, "malloc failed in prepare_mospf_hello.");
		return -1;
	}

	struct ether_header *eth_header = (struct ether_header *)(*packet);
	struct iphdr *ip_header = packet_to_ip_hdr(*packet);
	struct mospf_hdr *mospf_header = (struct mospf_hdr *)((char *)ip_header + IP_BASE_HDR_SIZE);
	struct mospf_hello *mospf_hello = (struct mospf_hello *)((char *)mospf_header + MOSPF_HDR_SIZE);

	// Ethernet header init
	memcpy(eth_header->ether_shost, iface->mac, ETH_ALEN);
	u8 dst_mac[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05}; // 01:00:5E:00:00:05
	memcpy(eth_header->ether_dhost, dst_mac, ETH_ALEN);
	eth_header->ether_type = htons(ETH_P_IP);

	// IP header init
	ip_init_hdr(ip_header, iface->ip, MOSPF_ALLSPFRouters, IP_BASE_HDR_SIZE + pkt_len, IPPROTO_MOSPF);
	ip_header->ttl = 1;  // Only neighbors can receive the Hello packet
	ip_header->checksum = ip_checksum(ip_header);  // Recalculate checksum

	// mOSPF header and Hello message init
	mospf_init_hdr(mospf_header, MOSPF_TYPE_HELLO, pkt_len, instance->router_id, instance->area_id);
	mospf_init_hello(mospf_hello, iface->mask);
	mospf_header->checksum = mospf_checksum(mospf_header);

	return len;
}

// Prepare mOSPF LSU message, return message length
int prepare_lsu_msg(char **msg)
{
	// fprintf(stdout, "TODO: prepare mOSPF LSU message.\n");
	int n_nbr = 0;

	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		n_nbr += (iface->num_nbr > 0) ? iface->num_nbr : 1;  // Blank LSA if no neighbor
	}

	int len = MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + MOSPF_LSA_SIZE * n_nbr;

	*msg = (char *)malloc(len);

	struct mospf_hdr *mospf_hdr = (struct mospf_hdr *)(*msg);
	struct mospf_lsu *mospf_lsu = (struct mospf_lsu *)((char *)mospf_hdr + MOSPF_HDR_SIZE);
	struct mospf_lsa *mospf_lsa = (struct mospf_lsa *)((char *)mospf_lsu + MOSPF_LSU_SIZE);

	mospf_init_hdr(mospf_hdr, MOSPF_TYPE_LSU, len, instance->router_id, instance->area_id);
	mospf_init_lsu(mospf_lsu, n_nbr);

	int index_lsa = 0;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (iface->num_nbr == 0) {
			// Blank LSA
			mospf_lsa[index_lsa].network = htonl(iface->ip & iface->mask);
			mospf_lsa[index_lsa].mask = htonl(iface->mask);
			mospf_lsa[index_lsa].rid = htonl(0);
			index_lsa += 1;
		}
		else {
			mospf_nbr_t *nbr = NULL;
			list_for_each_entry(nbr, &iface->nbr_list, list) {
				mospf_lsa[index_lsa].network = htonl(nbr->nbr_ip & nbr->nbr_mask);
				mospf_lsa[index_lsa].mask = htonl(nbr->nbr_mask);
				mospf_lsa[index_lsa].rid = htonl(nbr->nbr_id);
				index_lsa += 1;
			}
		}
	}

	mospf_hdr->checksum = mospf_checksum(mospf_hdr);

	return len;
}

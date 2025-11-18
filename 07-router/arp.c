#include "arp.h"
#include "base.h"
#include "types.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	// fprintf(stderr, "TODO: send arp request when lookup failed in arpcache.\n");

	// Malloc space for ethernet header + arp header
	char* packet = malloc(ETHER_HDR_SIZE + ETHER_ARP_SIZE);
	struct ether_header* eh = (struct ether_header *)packet;
	struct ether_arp* arp_req = (struct ether_arp *)(packet + ETHER_HDR_SIZE);

	// Fill the ethernet header
	memset(eh->ether_dhost, 0xff, ETH_ALEN); // Broadcast
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN); // Source MAC
	eh->ether_type = htons(ETH_P_ARP); // This packet is ARP

	// Fill the arp request header
	arp_req->arp_hrd = htons(ARPHRD_ETHER); // Hardware type
	arp_req->arp_pro = htons(ETH_P_IP); // Protocol type
	arp_req->arp_hln = ETH_ALEN; // Hardware address length
	arp_req->arp_pln = 4; // Protocol address length
	arp_req->arp_op = htons(ARPOP_REQUEST); // ARP request
	memcpy(arp_req->arp_sha, iface->mac, ETH_ALEN); // Sender hardware address
	arp_req->arp_spa = htonl(iface->ip); // Sender protocol address
	memset(arp_req->arp_tha, 0x00, ETH_ALEN); // Target hardware address
	arp_req->arp_tpa = htonl(dst_ip); // Target protocol address

	// Send ARP request packet
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + ETHER_ARP_SIZE);
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	// fprintf(stderr, "TODO: send arp reply when receiving arp request.\n");

	// Malloc space for ethernet header + arp header
	char* packet = malloc(ETHER_HDR_SIZE + ETHER_ARP_SIZE);
	struct ether_header* eh = (struct ether_header *)packet;
	struct ether_arp* arp_rpl = (struct ether_arp *)(packet + ETHER_HDR_SIZE);

	// Fill the ethernet header
	memset(eh->ether_dhost, 0xff, ETH_ALEN); // Broadcast
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN); // Source MAC
	eh->ether_type = htons(ETH_P_ARP); // This packet is ARP

	// Fill the arp reply header
	arp_rpl->arp_hrd = htons(ARPHRD_ETHER); // Hardware type
	arp_rpl->arp_pro = htons(ETH_P_IP); // Protocol type
	arp_rpl->arp_hln = ETH_ALEN; // Hardware address length
	arp_rpl->arp_pln = 4; // Protocol address length
	arp_rpl->arp_op = htons(ARPOP_REPLY); // ARP reply
	memcpy(arp_rpl->arp_sha, iface->mac, ETH_ALEN); // Sender hardware address
	arp_rpl->arp_spa = htonl(iface->ip); // Sender protocol address
	memcpy(arp_rpl->arp_tha, req_hdr->arp_sha, ETH_ALEN); // Target hardware address, use request's sha
	arp_rpl->arp_tpa = req_hdr->arp_spa; // Target protocol address, use request's spa

	// Send ARP request packet
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + ETHER_ARP_SIZE);
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");

	struct ether_arp* arp_hdr = (struct ether_arp *)(packet + ETHER_HDR_SIZE);

	u16 arp_op = ntohs(arp_hdr->arp_op);
	u32 arp_tpa = ntohl(arp_hdr->arp_tpa);
	u32 arp_spa = ntohl(arp_hdr->arp_spa);

	if (arp_op == ARPOP_REQUEST) {
		// log(DEBUG, "receive arp request, send arp reply");
		if (arp_tpa == iface->ip) {
			// Self is being asked, send arp reply
			arp_send_reply(iface, arp_hdr);
		}
	}
	else {  // is ARPOP_REPLY
		// log(DEBUG, "receive arp reply");
		if (arp_tpa == iface->ip) {
			// Self is answered, save it into arpcache
			u8 new_mac[ETH_ALEN];
			memcpy(new_mac, arp_hdr->arp_sha, ETH_ALEN);
			arpcache_insert(arp_spa, new_mac);
		}
	}
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}

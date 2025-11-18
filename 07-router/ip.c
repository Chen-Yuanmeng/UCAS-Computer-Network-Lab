#include "ip.h"

#include <stdio.h>
#include <stdlib.h>

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: handle ip packet.\n");
	struct iphdr* ip_header = packet_to_ip_hdr(packet);

	// check checksum before modifying the packet
	u16 ip_sum = checksum((u16 *)ip_header, ip_header->ihl * 4, 0);
	if (ip_sum != 0) {
		free(packet);
		return;
	}

	if (ip_header->daddr == iface->ip) {
		if (ip_header->protocol == IPPROTO_ICMP) {
			handle_icmp_packet(iface, packet, len);
		}
		else {
			// Sending a non-ICMP packet to a router is meaningless
			free(packet);
			return;
		}
	}

	// TTL -= 1
	ip_header->ttl -= 1;
	if (ip_header->ttl <= 0) {
		// ttl expired, send ICMP time exceeded
		icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		free(packet);
		return;
	}

	// Recompute checksum
	ip_header->checksum = ip_checksum(ip_header);

	// Search in routing table
	u32 daddr = ntohl(ip_header->daddr);
	rt_entry_t* rt_entry = longest_prefix_match(daddr);

	if (!rt_entry) {
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
		free(packet);
		return;
	}

	// Send packet to next hop
	u32 next_hop = rt_entry->gw ? rt_entry->gw : daddr;

	if (daddr == rt_entry->iface->ip) { // 
		if (ip_header->protocol == IPPROTO_ICMP) {
			handle_icmp_packet(rt_entry->iface, packet, len);
			return;
		}
		else {
			// Sending a non-ICMP packet to a router is meaningless
			free(packet);
			return;
		}
	}

	iface_send_packet_by_arp(rt_entry->iface, next_hop, packet, len);
}

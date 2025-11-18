#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* send icmp packet
 * 
 * Type, code pairs:
 * 1. No result in router table
 * 	 Type: ICMP_DEST_UNREACH
 *   Code: ICMP_NET_UNREACH
 * 2. ARP response failed
 *   Type: ICMP_DEST_UNREACH
 *   Code: ICMP_HOST_UNREACH
 * 3. TTL down to 0
 * 	 Type: ICMP_TIME_EXCEEDED
 *   Code: ICMP_EXC_TTL
 * 4. Send Echo reply (if in_pkt is echo request)
 * 	 Type: ICMP_ECHOREPLY
 *   Code: 0
 * 
 * 1, 2, 3: Rest of ICMP Header
 * - First 4 bytes set 0
 * - Copy IP header (first 20 bytes)
 * - 
 */
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	// fprintf(stderr, "TODO: malloc and send icmp packet.\n");

	// Note: icmp_send_packet packs the data; ip_send_packet actually sends the
	// packet. icmp_send_packet calls ip_send_packet to send the packet out.

	struct iphdr* iph = packet_to_ip_hdr(in_pkt);
	char* in_ipdata = IP_DATA(iph);

	char* out_pkt = NULL;

	int out_len = 0;
	int icmp_len = 0;

	// Cauculate out packet length
	if (type == ICMP_ECHOREPLY) {
		icmp_len = ntohs(iph->tot_len) - IP_HDR_SIZE(iph);
	}
	else {
		// Other 3 len are the same
		icmp_len = ICMP_HDR_SIZE + IP_HDR_SIZE(iph) + ICMP_COPIED_DATA_LEN;
	}
	out_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + icmp_len;

	out_pkt = malloc(out_len);
	memset(out_pkt, 0, out_len);

	struct iphdr* oph = packet_to_ip_hdr(out_pkt);

	if (type == ICMP_ECHOREPLY) {
		ip_init_hdr(oph, ntohl(iph->daddr), ntohl(iph->saddr), IP_BASE_HDR_SIZE + icmp_len, IPPROTO_ICMP);
	}
	else if (type == ICMP_PORT_UNREACH || type == ICMP_TIME_EXCEEDED) {
		rt_entry_t *rt_entry = longest_prefix_match(ntohl(iph->saddr));
		if (!rt_entry) {
			free(out_pkt);
			return;
		}
		ip_init_hdr(oph, rt_entry->iface->ip, ntohl(iph->saddr), IP_BASE_HDR_SIZE + icmp_len, IPPROTO_ICMP);
	}

	char* out_ipdata = IP_DATA(oph);
	struct icmphdr* out_icmp_hdr = (struct icmphdr*) out_ipdata;

	if (type == ICMP_ECHOREPLY) {
		memcpy(out_ipdata, in_ipdata, icmp_len);
	}
	else if (type == ICMP_DEST_UNREACH || type == ICMP_TIME_EXCEEDED) {
		out_icmp_hdr->icmp_identifier = 0;
		out_icmp_hdr->icmp_sequence = 0;
		memcpy(out_ipdata + ICMP_HDR_SIZE, iph, icmp_len - ICMP_HDR_SIZE);
	}

	out_icmp_hdr->type = type;
	out_icmp_hdr->code = code;
	out_icmp_hdr->checksum = icmp_checksum(out_icmp_hdr, icmp_len);

	if (out_pkt) {
		ip_send_packet(out_pkt, out_len);
	}
}

void handle_icmp_packet(iface_info_t *iface, char *packet, int len) {
	struct iphdr* ip_header = packet_to_ip_hdr(packet);
	struct icmphdr* icmp_header = (struct icmphdr*) IP_DATA(ip_header);

	if (icmp_header->type == ICMP_ECHOREQUEST) {
		icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		return;
	}
}

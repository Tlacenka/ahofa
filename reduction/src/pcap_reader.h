/// @author Jakub Semric
///
/// Class PcapReader for reading and processing packets in packet caputure files.
/// @file pcap_reader.h
///
/// Unless otherwise stated, all code is licensed under a
/// GNU General Public Licence v2.0

#ifndef __pcap_reader_h
#define __pcap_reader_h

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cassert>

#include <pcap.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/ethernet.h>

/// Class PcapReader for reading and processing packets in packet caputure files.
namespace pcapreader
{

struct vlan_ethhdr {
    u_int8_t  ether_dhost[ETH_ALEN];  /* destination eth addr */
    u_int8_t  ether_shost[ETH_ALEN];  /* source ether addr    */
    u_int16_t h_vlan_proto;
    u_int16_t h_vlan_TCI;
    u_int16_t ether_type;
} __attribute__ ((__packed__));

static inline const unsigned char *get_payload(
    const unsigned char *packet,
    const struct pcap_pkthdr *header);

template<typename F>
void process_payload(F func, unsigned long count);


/// Generic function for processing packet payload.
///
/// @tparam F lambda function which manipulates with packet payload
/// @param count Total number of processed packets, which includes some payload data.
template<typename F>
void process_payload(const char* capturefile, F func, unsigned long count)
{
    char err_buf[4096] = "";
    pcap_t *pcap;

    if (!(pcap = pcap_open_offline(capturefile, err_buf))) {
        throw std::runtime_error(
            "cannot open pcap file '" + std::string(capturefile) + "'");
    }

    struct pcap_pkthdr *header;
    const unsigned char *packet, *payload;

    while (pcap_next_ex(pcap, &header, &packet) == 1 && count) {
        payload = get_payload(packet, header);
        int len = header->caplen - (payload - packet);
        if (len > 0) {
            count--;
            func(payload, len);
        }
    }

    pcap_close(pcap);
}

inline const unsigned char *get_payload(
    const unsigned char *packet,
    const struct pcap_pkthdr *header)
{
    const unsigned char *packet_end = packet + header->caplen;
    size_t offset = sizeof(ether_header);
    const ether_header* eth_hdr = reinterpret_cast<const ether_header*>(packet);
    uint16_t ether_type = ntohs(eth_hdr->ether_type);
    if (ETHERTYPE_VLAN == ether_type)
    {
        offset = sizeof(vlan_ethhdr);
        const vlan_ethhdr* vlan_hdr = reinterpret_cast<const vlan_ethhdr*>(packet);
        ether_type = ntohs(vlan_hdr->ether_type);
    }

    unsigned l4_proto;

    if (ETHERTYPE_IP == ether_type)
    {
        const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
        offset += sizeof(ip);
        l4_proto = ip_hdr->ip_p;
    }
    else if (ETHERTYPE_IPV6 == ether_type)
    {
        const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
        offset += sizeof(ip6_hdr);
        l4_proto = ip_hdr->ip6_nxt;
    }
    else
    {
        return packet_end;
    }

    bool cond;
    do {
        cond = false;
        if (IPPROTO_TCP == l4_proto)
        {
            const tcphdr* tcp_hdr = reinterpret_cast<const tcphdr*>(packet + offset);
            size_t tcp_hdr_size = tcp_hdr->th_off * 4;
            offset += tcp_hdr_size;
        }
        else if (IPPROTO_UDP == l4_proto)
        {
            offset += sizeof(udphdr);
        }
        else if (IPPROTO_IPIP == l4_proto)
        {
            const ip* ip_hdr = reinterpret_cast<const ip*>(packet + offset);
            offset += sizeof(ip);
            l4_proto = ip_hdr->ip_p;
            cond = true;
        }
        else if (IPPROTO_ESP == l4_proto)
        {
            offset += 8;
        }
        else if (IPPROTO_ICMP == l4_proto)
        {
            offset += sizeof(icmphdr);
        }
        else if (IPPROTO_ICMPV6 == l4_proto)
        {
            offset += sizeof(icmp6_hdr);
        }
        else if (IPPROTO_FRAGMENT == l4_proto)
        {
            const ip6_frag* ip_hdr = reinterpret_cast<const ip6_frag*>(packet + offset);
            offset += sizeof(ip6_frag);
            l4_proto = ip_hdr->ip6f_nxt;
            cond = true;
        }
        else if (IPPROTO_IPV6 == l4_proto)
        {
            const ip6_hdr* ip_hdr = reinterpret_cast<const ip6_hdr*>(packet + offset);
            offset += sizeof(ip6_hdr);
            l4_proto = ip_hdr->ip6_nxt;
        }
        else
        {
            return packet + header->caplen;
        }
    } while (cond);

    if (offset > header->caplen) {
        // TODO write packet to log file
        // beware of a shared access, use mutex
        return packet + header->caplen;
    }

    return packet + offset;
}

void print_readable(const unsigned char *payload, unsigned length) {
    for (unsigned i = 0; i < length; i++) {
        if (isprint(payload[i])) {
            printf("%c", payload[i]);
        }
        else {
            printf("\\x%.2x", payload[i]);
        }
    }
    printf("\n");
}

};  // end of namespace

#endif
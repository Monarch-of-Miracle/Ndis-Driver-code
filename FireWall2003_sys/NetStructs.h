#pragma once
typedef struct ETH_HEADER 
{
	unsigned char	h_dest[6];	/* destination eth addr	*/
	unsigned char	h_source[6];	/* source ether addr	*/
	unsigned short	h_proto;		    /* packet type ID field	*/
}   ETH_HEADER, *PETH_HEADER;


typedef struct IP_HEADER
{
	unsigned char   ip_hlen:4, ip_ver:4;
	unsigned char   ip_tos;           /* type of service */
	unsigned short  ip_len;           /* datagram length */
	unsigned short  ip_id;            /* identification  */
	unsigned short  ip_off;           /* fragment offset & flags */
	unsigned char   ip_ttl;           /* time to live field */
	unsigned char   ip_proto;         /* datagram protocol */
	unsigned short  ip_csum;          /* header checksum */
	unsigned int    ip_src;
	unsigned int	ip_dst;
}IP_HEADER, *PIP_HEADER;



typedef struct _TCP_HEADER 
{
	USHORT	srcPort;
	USHORT	dstPort;
	ULONG	SeqNumber;
	ULONG   AckNumber;

	USHORT	res1 : 4;
	USHORT	DataOffset : 4;
	USHORT	fin : 1;
	USHORT	syn : 1;
	USHORT	rst : 1;
	USHORT	psh : 1;
	USHORT	ack : 1;
	USHORT	urg : 1;
	USHORT	ece : 1;
	USHORT	cwr : 1;

	USHORT	Window;
	USHORT	CheckSum;
	USHORT	UrgPointer;

} TCP_HEADER, *PTCP_HEADER;
#define GET_TCPHEADER(ip) (PTCP_HEADER)((char*)ip + ip->ip_hlen * sizeof(ULONG));

typedef struct UDP_HEADER
{
	USHORT    uh_sport;        /* source port */
	USHORT    uh_dport;        /* destination port */
	USHORT   uh_ulen;        /* udp length */
	USHORT   uh_sum;            /* udp checksum */

}UDP_HEADER, *PUDP_HEADER;
#define GET_UDPHEADER(ip) (PUDP_HEADER)((char*)ip + ip->ip_hlen * sizeof(ULONG));


#define IP_OFFSET                               0x0E

//IP Protocol Types
#define PROT_ICMP                               0x01 
#define PROT_TCP                                0x06 
#define PROT_UDP                                0x11 
 
 
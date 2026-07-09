/*
borrowed from musl library.

License there says that includes may be used without attribution.
*/

/* netinet/ip.h */
struct iphdr {
	/* changed from unsigned int so it takes the correct size */
	uint8_t ihl:4;
	uint8_t version:4;
	uint8_t tos;
	uint16_t tot_len;
	uint16_t id;
	uint16_t frag_off;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t saddr;
	uint32_t daddr;
};

struct ip {
	/* changed from unsigned int so it takes the correct size */
	uint8_t ip_hl:4;
	uint8_t ip_v:4;

	uint8_t ip_tos;
	uint16_t ip_len;
	uint16_t ip_id;
	uint16_t ip_off;
	uint8_t ip_ttl;
	uint8_t ip_p;
	uint16_t ip_sum;
	uint32_t ip_src_, ip_dst_;
};

/* netinet/ip_icmp.h */
#define ICMP_ECHOREPLY		0
#define ICMP_DEST_UNREACH	3
#define ICMP_SOURCE_QUENCH	4
#define ICMP_REDIRECT		5
#define ICMP_ECHO		8
#define ICMP_TIME_EXCEEDED	11
#define ICMP_PARAMETERPROB	12
#define ICMP_TIMESTAMP		13
#define ICMP_TIMESTAMPREPLY	14
#define ICMP_INFO_REQUEST	15
#define ICMP_INFO_REPLY		16
#define ICMP_ADDRESS		17
#define ICMP_ADDRESSREPLY	18

struct icmp_ra_addr {
	uint32_t ira_addr;
	uint32_t ira_preference;
};

struct icmp {
	uint8_t  icmp_type;
	uint8_t  icmp_code;
	uint16_t icmp_cksum;
	union {
		uint8_t ih_pptr;
		struct in_addr ih_gwaddr;
		struct ih_idseq {
			uint16_t icd_id;
			uint16_t icd_seq;
		} ih_idseq;
		uint32_t ih_void;

		struct ih_pmtu {
			uint16_t ipm_void;
			uint16_t ipm_nextmtu;
		} ih_pmtu;

		struct ih_rtradv {
			uint8_t irt_num_addrs;
			uint8_t irt_wpa;
			uint16_t irt_lifetime;
		} ih_rtradv;
	} icmp_hun;
	union {
		struct {
			uint32_t its_otime;
			uint32_t its_rtime;
			uint32_t its_ttime;
		} id_ts;
		struct {
			struct ip idi_ip;
		} id_ip;
		struct icmp_ra_addr id_radv;
		uint32_t   id_mask;
		uint8_t    id_data[1];
	} icmp_dun;
};

#define	icmp_pptr	icmp_hun.ih_pptr
#define	icmp_gwaddr	icmp_hun.ih_gwaddr
#define	icmp_id		icmp_hun.ih_idseq.icd_id
#define	icmp_seq	icmp_hun.ih_idseq.icd_seq
#define	icmp_void	icmp_hun.ih_void
#define	icmp_pmvoid	icmp_hun.ih_pmtu.ipm_void
#define	icmp_nextmtu	icmp_hun.ih_pmtu.ipm_nextmtu
#define	icmp_num_addrs	icmp_hun.ih_rtradv.irt_num_addrs
#define	icmp_wpa	icmp_hun.ih_rtradv.irt_wpa
#define	icmp_lifetime	icmp_hun.ih_rtradv.irt_lifetime
#define	icmp_otime	icmp_dun.id_ts.its_otime
#define	icmp_rtime	icmp_dun.id_ts.its_rtime
#define	icmp_ttime	icmp_dun.id_ts.its_ttime
#define	icmp_ip		icmp_dun.id_ip.idi_ip
#define	icmp_radv	icmp_dun.id_radv
#define	icmp_mask	icmp_dun.id_mask
#define	icmp_data	icmp_dun.id_data

#define	ICMP_MINLEN	8
#define	ICMP_TSLEN	(8 + 3 * sizeof (n_time))
#define	ICMP_MASKLEN	12
#define	ICMP_ADVLENMIN	(8 + sizeof (struct ip) + 8)
#define	ICMP_ADVLEN(p)	(8 + ((p)->icmp_ip.ip_hl << 2) + 8)

#define	ICMP_UNREACH		3
#define	ICMP_TIMXCEED		11

#define	ICMP_UNREACH_NET	        0
#define	ICMP_UNREACH_HOST	        1
#define	ICMP_UNREACH_PROTOCOL	        2
#define	ICMP_UNREACH_PORT	        3
#define	ICMP_UNREACH_NEEDFRAG	        4
#define	ICMP_UNREACH_SRCFAIL	        5
#define	ICMP_UNREACH_NET_UNKNOWN        6
#define	ICMP_UNREACH_HOST_UNKNOWN       7
#define	ICMP_UNREACH_ISOLATED	        8
#define	ICMP_UNREACH_NET_PROHIB	        9
#define	ICMP_UNREACH_HOST_PROHIB        10
#define	ICMP_UNREACH_TOSNET	        11
#define	ICMP_UNREACH_TOSHOST	        12
#define	ICMP_UNREACH_FILTER_PROHIB      13
#define	ICMP_UNREACH_HOST_PRECEDENCE    14
#define	ICMP_UNREACH_PRECEDENCE_CUTOFF  15

#define	ICMP_TIMXCEED_INTRANS	0
#define	ICMP_TIMXCEED_REASS	1

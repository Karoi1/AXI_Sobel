#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#include "xil_types.h"
#include "netif/xadapter.h"

#define UDP_IMG_PORT            5000u
#define UDP_MAX_PAYLOAD         1400u
#define UDP_HDR_SIZE            4u
#define UDP_PKT_DATA_SIZE       (UDP_MAX_PAYLOAD - UDP_HDR_SIZE)

#define UDP_MAX_PKTS            221u

void udp_proto_init(void);
int  udp_pkt_available(void);
void udp_consume_pkt(void);
int  udp_image_ready(void);
void udp_send_image(struct netif *netif);
void udp_send_input(struct netif *netif);
void udp_reset_state(void);

#endif

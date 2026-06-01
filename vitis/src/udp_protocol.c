#include <string.h>
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "udp_protocol.h"
#include "vdma_driver.h"

static struct udp_pcb *udp_pcb;
static ip_addr_t       pc_addr;
static u16_t           pc_port;

static u8   rx_bitmap[UDP_MAX_PKTS / 8 + 1];
static u16  rx_total_pkts;
static u16  rx_received_count;
static int  rx_ready;
static int  rx_locked;   /* reject new images during VDMA/send */

static u8   rx_pkt_buf[UDP_PKT_DATA_SIZE];
static u16  rx_pkt_idx;
static u16  rx_pkt_len;
static int  rx_pkt_pending;

static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
	const ip_addr_t *addr, u16_t port)
{
	u8  *buf;
	u16  total_pkts, pkt_idx, data_len;

	if (p == NULL) return;
	if (p->len < UDP_HDR_SIZE) { pbuf_free(p); return; }

	buf = (u8 *)p->payload;
	total_pkts = buf[0] | ((u16)buf[1] << 8);
	pkt_idx    = buf[2] | ((u16)buf[3] << 8);
	data_len   = p->len - UDP_HDR_SIZE;

	if (pkt_idx >= UDP_MAX_PKTS) { pbuf_free(p); return; }

	if (pkt_idx == 0) {
		if (rx_locked) { pbuf_free(p); return; }
		rx_total_pkts    = total_pkts;
		rx_received_count = 0;
		rx_ready          = 0;
		memset(rx_bitmap, 0, sizeof(rx_bitmap));
		pc_addr = *addr;
		pc_port = port;
	} else if (rx_total_pkts == 0) {
		pbuf_free(p);
		return;
	}

	rx_pkt_idx     = pkt_idx;
	rx_pkt_len     = data_len;
	memcpy(rx_pkt_buf, buf + UDP_HDR_SIZE, data_len);
	rx_pkt_pending = 1;

	pbuf_free(p);
}

void udp_proto_init(void)
{
	udp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (udp_pcb == NULL) {
		xil_printf("[UDP] Failed to create PCB\r\n");
		return;
	}
	if (udp_bind(udp_pcb, IP_ANY_TYPE, UDP_IMG_PORT) != ERR_OK) {
		xil_printf("[UDP] Failed to bind port %d\r\n", UDP_IMG_PORT);
		return;
	}
	udp_recv(udp_pcb, udp_recv_callback, NULL);
	xil_printf("[UDP] Listening on port %d\r\n", UDP_IMG_PORT);
}

int udp_pkt_available(void)
{
	return rx_pkt_pending;
}

void udp_consume_pkt(void)
{
	u16  pkt_idx = rx_pkt_idx;
	u32  offset  = (u32)pkt_idx * UDP_PKT_DATA_SIZE;

	memcpy((u8 *)FRAMEBUF_IN(0) + offset, rx_pkt_buf, rx_pkt_len);
	rx_pkt_pending = 0;

	if (!(rx_bitmap[pkt_idx >> 3] & (1u << (pkt_idx & 7)))) {
		rx_bitmap[pkt_idx >> 3] |= (1u << (pkt_idx & 7));
		rx_received_count++;
	}

	if (pkt_idx == 0) {
		xil_printf("[UDP] Pkt0: total=%d, len=%d\r\n", rx_total_pkts, rx_pkt_len);
	}

	if (rx_received_count >= rx_total_pkts) {
		Xil_DCacheFlushRange(FRAMEBUF_IN(0), IMG_SIZE);
		rx_ready  = 1;
		rx_locked = 1;
		xil_printf("[UDP] Image complete: %d pkts\r\n", rx_received_count);
	}

}

int udp_image_ready(void)
{
	return rx_ready;
}

void udp_send_image(struct netif *netif)
{
	u8  *img = (u8 *)FRAMEBUF_OUT(0);
	u32  remaining = IMG_SIZE_OUT;
	u16  total_pkts = (remaining + UDP_PKT_DATA_SIZE - 1) / UDP_PKT_DATA_SIZE;
	u16  pkt_idx;
	struct pbuf *p;
	u8  hdr[UDP_HDR_SIZE];
	err_t err;

	for (pkt_idx = 0; pkt_idx < total_pkts; pkt_idx++) {
		u16 payload_len = (remaining > UDP_PKT_DATA_SIZE)
			? UDP_PKT_DATA_SIZE : (u16)remaining;

		p = pbuf_alloc(PBUF_TRANSPORT, UDP_HDR_SIZE + payload_len, PBUF_RAM);
		if (p == NULL) {
			xil_printf("[UDP] pbuf_alloc failed at pkt %d\r\n", pkt_idx);
			return;
		}

		hdr[0] = total_pkts & 0xFF;
		hdr[1] = (total_pkts >> 8) & 0xFF;
		hdr[2] = pkt_idx & 0xFF;
		hdr[3] = (pkt_idx >> 8) & 0xFF;

		memcpy(p->payload, hdr, UDP_HDR_SIZE);
		memcpy((u8 *)p->payload + UDP_HDR_SIZE,
			img + pkt_idx * UDP_PKT_DATA_SIZE, payload_len);

		err = udp_sendto(udp_pcb, p, &pc_addr, pc_port);
		pbuf_free(p);

		if (err != ERR_OK) {
			xil_printf("[UDP] sendto failed at pkt %d: %d\r\n", pkt_idx, err);
			return;
		}

		{
			volatile int w;
			for (w = 0; w < 80000; w++);
		}
		xemacif_input(netif);
		remaining -= payload_len;
	}

	xil_printf("[UDP] Sent %d pkts, %d bytes\r\n", total_pkts, IMG_SIZE_OUT);
}

void udp_send_input(struct netif *netif)
{
	u8  *img = (u8 *)FRAMEBUF_IN(0);
	u32  remaining = IMG_SIZE;
	u16  total_pkts = (remaining + UDP_PKT_DATA_SIZE - 1) / UDP_PKT_DATA_SIZE;
	u16  pkt_idx;
	struct pbuf *p;
	u8  hdr[UDP_HDR_SIZE];
	err_t err;

	for (pkt_idx = 0; pkt_idx < total_pkts; pkt_idx++) {
		u16 payload_len = (remaining > UDP_PKT_DATA_SIZE)
			? UDP_PKT_DATA_SIZE : (u16)remaining;

		p = pbuf_alloc(PBUF_TRANSPORT, UDP_HDR_SIZE + payload_len, PBUF_RAM);
		if (p == NULL) {
			xil_printf("[UDP] send input pbuf_alloc failed at pkt %d\r\n", pkt_idx);
			return;
		}

		hdr[0] = total_pkts & 0xFF;
		hdr[1] = (total_pkts >> 8) & 0xFF;
		hdr[2] = pkt_idx & 0xFF;
		hdr[3] = (pkt_idx >> 8) & 0xFF;

		memcpy(p->payload, hdr, UDP_HDR_SIZE);
		memcpy((u8 *)p->payload + UDP_HDR_SIZE,
			img + pkt_idx * UDP_PKT_DATA_SIZE, payload_len);

		err = udp_sendto(udp_pcb, p, &pc_addr, pc_port);
		pbuf_free(p);

		if (err != ERR_OK) {
			xil_printf("[UDP] sendto failed at pkt %d: %d\r\n", pkt_idx, err);
			return;
		}

		{
			volatile int w;
			for (w = 0; w < 80000; w++);
		}
		xemacif_input(netif);
		remaining -= payload_len;
	}

	xil_printf("[UDP] Sent input echo %d pkts, %d bytes\r\n", total_pkts, IMG_SIZE);
}

void udp_reset_state(void)
{
	rx_ready          = 0;
	rx_received_count = 0;
	rx_total_pkts     = 0;
	rx_locked         = 0;
	rx_pkt_pending    = 0;
	memset(rx_bitmap, 0, sizeof(rx_bitmap));
}

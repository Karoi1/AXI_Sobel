#ifndef VDMA_DRIVER_H
#define VDMA_DRIVER_H

#include "xil_types.h"

#define VDMA_BASEADDR           0x43000000u

/* MM2S Register Offsets */
#define MM2S_VDMACR             0x00u
#define MM2S_VDMASR             0x04u
#define MM2S_VSIZE              0x50u
#define MM2S_HSIZE              0x54u
#define MM2S_FRMDLY_STRIDE      0x58u
#define MM2S_START_ADDR1        0x5Cu
#define MM2S_START_ADDR2        0x60u
#define MM2S_START_ADDR3        0x64u

/* S2MM Register Offsets */
#define S2MM_VDMACR             0x30u
#define S2MM_VDMASR             0x34u
#define S2MM_VSIZE              0xA0u
#define S2MM_HSIZE              0xA4u
#define S2MM_FRMDLY_STRIDE      0xA8u
#define S2MM_START_ADDR1        0xACu
#define S2MM_START_ADDR2        0xB0u
#define S2MM_START_ADDR3        0xB4u

/* VDMACR Bits */
#define VDMA_CR_RS              (1u << 0)
#define VDMA_CR_CIRC_PARK       (1u << 1)
#define VDMA_CR_RESET           (1u << 2)
#define VDMA_CR_GENLOCK_EN      (1u << 3)
#define VDMA_CR_FRMCNT_EN       (1u << 4)
#define VDMA_CR_GENLOCK_SRC     (1u << 7)
#define VDMA_CR_ERR_IRQEN       (1u << 14)
#define VDMA_CR_FRMCNT_IRQEN    (1u << 12)   /* FrmCnt_Irq assert enable */

/* VDMASR Bits */
#define VDMA_SR_HALTED          (1u << 0)
#define VDMA_SR_VDMAINT_ERR     (1u << 4)
#define VDMA_SR_VDMASLV_ERR     (1u << 5)
#define VDMA_SR_VDMADEC_ERR     (1u << 6)
#define VDMA_SR_ERR_IRQ         (1u << 14)

/* Shared Registers */
#define PARK_PTR_REG            0x28u
#define VDMA_VERSION_REG        0x2Cu
#define VDMA_SR_SOF_EARLY_ERR   (1u << 7)
#define VDMA_SR_SOF_LATE_ERR    (1u << 11)

#define VDMA_CTRL_RUN  (VDMA_CR_RS | VDMA_CR_FRMCNT_EN | VDMA_CR_FRMCNT_IRQEN | VDMA_CR_ERR_IRQEN)
//                     bit 0       bit 4              bit 12              bit 14
//                   = 0x5011  (No Genlock: each channel runs independently)

/* Image dimensions */
#define IMG_WIDTH               640u
#define IMG_HEIGHT              480u
#define IMG_HEIGHT_OUT          480u
#define IMG_STRIDE              640u
#define IMG_SIZE                (IMG_WIDTH * IMG_HEIGHT)
#define IMG_SIZE_OUT            (IMG_WIDTH * IMG_HEIGHT_OUT)

/* Frame buffer addresses */
#define FRAMEBUF_IN_BASE        0x0D000000u
#define FRAMEBUF_IN(x)          (FRAMEBUF_IN_BASE + (u32)(x) * IMG_SIZE)

#define FRAMEBUF_OUT_BASE       0x0D900000u
#define FRAMEBUF_OUT(x)         (FRAMEBUF_OUT_BASE + (u32)(x) * IMG_SIZE_OUT)

void vdma_reset(void);
void vdma_configure(void);
void vdma_start(void);
int  vdma_done(void);
void vdma_dump_status(void);
void vdma_dump_output_hex(void);

#endif

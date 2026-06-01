#include "vdma_driver.h"
#include "xil_io.h"
#include "xil_printf.h"

void vdma_reset(void)
{
	u32 reg;

	Xil_Out32(VDMA_BASEADDR + MM2S_VDMACR, VDMA_CR_RESET);
	do {
		reg = Xil_In32(VDMA_BASEADDR + MM2S_VDMACR);
	} while (reg & VDMA_CR_RESET);

	Xil_Out32(VDMA_BASEADDR + S2MM_VDMACR, VDMA_CR_RESET);
	do {
		reg = Xil_In32(VDMA_BASEADDR + S2MM_VDMACR);
	} while (reg & VDMA_CR_RESET);

	xil_printf("[VDMA] Reset complete\r\n");
}

void vdma_configure(void)
{
	/* write configure for MM2S */
	Xil_Out32(VDMA_BASEADDR + MM2S_VDMACR, VDMA_CTRL_RUN);

	Xil_Out32(VDMA_BASEADDR + MM2S_START_ADDR1, FRAMEBUF_IN(0));
	Xil_Out32(VDMA_BASEADDR + MM2S_START_ADDR2, FRAMEBUF_IN(0));
	Xil_Out32(VDMA_BASEADDR + MM2S_START_ADDR3, FRAMEBUF_IN(0));

	Xil_Out32(VDMA_BASEADDR + MM2S_FRMDLY_STRIDE, IMG_STRIDE);
	Xil_Out32(VDMA_BASEADDR + MM2S_HSIZE, IMG_WIDTH);

	/* write configure for S2MM */
	Xil_Out32(VDMA_BASEADDR + S2MM_VDMACR, VDMA_CTRL_RUN);

	Xil_Out32(VDMA_BASEADDR + S2MM_START_ADDR1, FRAMEBUF_OUT(0));
	Xil_Out32(VDMA_BASEADDR + S2MM_START_ADDR2, FRAMEBUF_OUT(0));
	Xil_Out32(VDMA_BASEADDR + S2MM_START_ADDR3, FRAMEBUF_OUT(0));

	Xil_Out32(VDMA_BASEADDR + S2MM_FRMDLY_STRIDE, IMG_STRIDE);
	Xil_Out32(VDMA_BASEADDR + S2MM_HSIZE, IMG_WIDTH);

	/* Mask non-halting SOF/EOL errors  Genlock handles frame sync */
	Xil_Out32(VDMA_BASEADDR + S2MM_VDMASR, 0xFFFFFFFF);
	Xil_Out32(VDMA_BASEADDR + 0x3C, 0x0000000F);
}

void vdma_start(void)
{


	/* Clear sticky status bits (FrmCnt_Irq, error flags) from previous run */
	Xil_Out32(VDMA_BASEADDR + MM2S_VDMASR, 0xFFFFFFFF);
	Xil_Out32(VDMA_BASEADDR + S2MM_VDMASR, 0xFFFFFFFF);

	/* Re-enable RS auto-cleared by FrameCntEn after prior frame */
	Xil_Out32(VDMA_BASEADDR + MM2S_VDMACR, VDMA_CTRL_RUN);
	Xil_Out32(VDMA_BASEADDR + S2MM_VDMACR, VDMA_CTRL_RUN);

	xil_printf("[VDMA] VDMA start\n\r");
	xil_printf("[VDMA] S2MM: HSIZE=%d VSIZE=%d STRIDE=%d ADDR1=0x%08X\r\n",
		Xil_In32(VDMA_BASEADDR + S2MM_HSIZE),
		Xil_In32(VDMA_BASEADDR + S2MM_VSIZE),
		Xil_In32(VDMA_BASEADDR + S2MM_FRMDLY_STRIDE),
		Xil_In32(VDMA_BASEADDR + S2MM_START_ADDR1));
	xil_printf("[VDMA] MM2S: HSIZE=%d VSIZE=%d STRIDE=%d ADDR1=0x%08X\r\n",
		Xil_In32(VDMA_BASEADDR + MM2S_HSIZE),
		Xil_In32(VDMA_BASEADDR + MM2S_VSIZE),
		Xil_In32(VDMA_BASEADDR + MM2S_FRMDLY_STRIDE),
		Xil_In32(VDMA_BASEADDR + MM2S_START_ADDR1));
	Xil_Out32(VDMA_BASEADDR + S2MM_VSIZE, IMG_HEIGHT_OUT);
	Xil_Out32(VDMA_BASEADDR + MM2S_VSIZE, IMG_HEIGHT);
}

int vdma_done(void)
{
	/* Check Frame Cnt */
    /* Wait for MM2S to finish; S2MM may have SOF errors but data is in buffer */
    return (Xil_In32(VDMA_BASEADDR + MM2S_VDMASR) & (1u << 12));
}


void vdma_dump_status(void)
{
	u32 mm2s_cr = Xil_In32(VDMA_BASEADDR + MM2S_VDMACR);
	u32 s2mm_cr = Xil_In32(VDMA_BASEADDR + S2MM_VDMACR);
	u32 mm2s_sr = Xil_In32(VDMA_BASEADDR + MM2S_VDMASR);
	u32 s2mm_sr = Xil_In32(VDMA_BASEADDR + S2MM_VDMASR);
	u32 park    = Xil_In32(VDMA_BASEADDR + PARK_PTR_REG);

	xil_printf("[VDMA] MM2S_CR=0x%04X SR=0x%08X  S2MM_CR=0x%04X SR=0x%08X\r\n",
		mm2s_cr, mm2s_sr, s2mm_cr, s2mm_sr);
	xil_printf("[VDMA] PARK=0x%08X (RdFrm=%d WrFrm=%d)\r\n",
		park,
		(park >> 16) & 0x1F,
		(park >> 24) & 0x1F);
	xil_printf("[VDMA] MM2S: Halt=%d FrmCnt=%d  S2MM: Halt=%d FrmCnt=%d\r\n",
		mm2s_sr & 1,
		(mm2s_sr >> 16) & 0xFF,
		s2mm_sr & 1,
		(s2mm_sr >> 16) & 0xFF);

	if (mm2s_sr & VDMA_SR_VDMADEC_ERR) xil_printf("  MM2S: Decode Error\r\n");
	if (mm2s_sr & VDMA_SR_VDMASLV_ERR) xil_printf("  MM2S: Slave Error\r\n");
	if (mm2s_sr & VDMA_SR_VDMAINT_ERR) xil_printf("  MM2S: Internal Error\r\n");

	if (s2mm_sr & VDMA_SR_VDMADEC_ERR) xil_printf("  S2MM: Decode Error\r\n");
	if (s2mm_sr & VDMA_SR_VDMASLV_ERR) xil_printf("  S2MM: Slave Error\r\n");
	if (s2mm_sr & VDMA_SR_VDMAINT_ERR) xil_printf("  S2MM: Internal Error\r\n");
	if (s2mm_sr & VDMA_SR_SOF_EARLY_ERR) xil_printf("  S2MM: SOF Early Error\r\n");
	if (s2mm_sr & VDMA_SR_SOF_LATE_ERR)  xil_printf("  S2MM: SOF Late Error\r\n");
}

void vdma_dump_output_hex(void)
{
	volatile u8 *buf = (volatile u8 *)FRAMEBUF_OUT(0);
	int i, j;
	xil_printf("[VDMA] OUT BUF first 256 bytes:\r\n");
	for (i = 0; i < 16; i++) {
		xil_printf("  %04X:", i * 16);
		for (j = 0; j < 16; j++) {
			xil_printf(" %02X", buf[i * 16 + j]);
		}
		xil_printf("\r\n");
	}
}

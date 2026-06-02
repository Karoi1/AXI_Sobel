# AXI Sobel Edge Detection on Zybo Z7-20

**AXI-Stream Sobel 边缘检测完整工程** — 基于 Xilinx Zybo Z7-20 (XC7Z020)，通过 UDP 以太网收发图像，PL 端 VDMA + Sobel 硬件加速处理。

---

## Overview / 概述

```
PC (UDP) ──640×480 image──▶ Zynq PS (lwIP) ──DDR──▶ VDMA MM2S ──▶ Sobel Core ──▶ VDMA S2MM ──DDR──▶ UDP ──▶ PC
```

- **器件**: Zybo Z7-20 (XC7Z020-1CLG400C)
- **工具**: Vivado 2024.1 + Vitis Classic 2024.1
- **运行环境**: PS 裸机 (standalone, Cortex-A9)
- **图像规格**: 灰度 8-bit, 640×480 → 输出 640×480 (零填充)

## Key Features / 特性

| Feature | Description |
|---------|-------------|
| VDMA | MM2S (HP0) + S2MM (HP1) 独立端口，Genlock Dynamic Master/Slave |
| Sobel | 3 RTL 模块 (line_buffer + sobel_kernel + sobel_core)，4路并行，纯流式 |
| Network | UDP raw API (lwIP)，静态 IP 192.168.1.10:5000 |
| Output | 640×480 灰度，左右零填充，\|Gx\| + \|Gy\| 幅值近似 |

## Repository Structure / 仓库结构

```
├── vivado/                    # Vivado Hardware Project
│   ├── create_project.tcl     # 工程重建脚本
│   ├── bd/zybo.tcl            # Block Design TCL
│   ├── hdl/                   # RTL 源文件
│   ├── xci/                   # IP 配置文件
│   ├── sim/                   # 仿真 testbench
│   └── output/                # 预编译 XSA + bitstream
│
├── vitis/                     # Vitis Bare-Metal Software
│   ├── platform/              # 平台描述 + lwIP 配置
│   ├── patches/               # BSP 补丁 (RTL8211E-VL PHY fix)
│   └── src/                   # 应用源码
│
├── tools/                     # PC Python 工具
│   ├── onboard_utils/         # 上板测试 (send/recv)
│   └── sim_utils/             # 仿真验证 (pack/check)
│
└── test_imgs/                 # 测试图像
    ├── src/
    ├── sim_result/
    └── onboard_result/
```

## Quick Start / 快速开始

### 1. Hardware / 硬件

```tcl
# Vivado Tcl Console:
cd vivado
source create_project.tcl
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
write_hw_platform -fixed -include_bit -force -file output/zybo_wrapper.xsa
```

Or use the pre-built files:
- `vivado/output/zybo_wrapper_V5.xsa` (hardware platform)
- `vivado/output/zybo_wrapper.bit` (bitstream)

### 2. Software / 软件

```tcl
# Vitis XSCT Console:
cd vitis/platform
source create_platform.tcl
```

Then manually:
1. Apply patch: copy `vitis/patches/xemacpsif_physpeed.c` over the BSP source at `<bsp>/libsrc/lwip220_v1_0/src/lwip-2.2.0/contrib/ports/xilinx/netif/xemacpsif_physpeed.c`
2. Apply patch: copy `vitis/platform/lwipopts.h` over `<bsp>/include/lwipopts.h`
3. Create Application Project → add `vitis/src/*.c` and `vitis/src/*.h`
4. Build → generate `BOOT.BIN`

### 3. PC Tools / PC 端工具

```bash
# Send image for Sobel processing
python tools/onboard_utils/send_sobel.py <image.png>

# Loopback echo test
python tools/onboard_utils/recv_result.py <image.png>

# Pack image for simulation
python tools/sim_utils/pack_img.py <image.png>
```

## Hardware Config / 硬件配置

| Parameter | Value |
|-----------|-------|
| VDMA stream width | 32-bit (4 pixels/beat) |
| VDMA AXI width | 32-bit |
| VDMA line buffer depth | 2048 |
| VDMA max burst | 32 |
| Genlock | MM2S Dynamic Master, S2MM Dynamic Slave |
| fsync | Both None (free-run) |
| Frame stores | 1 (Park mode) |
| HP ports | HP0 (MM2S) + HP1 (S2MM) |
| Sobel magnitude | \|Gx\| + \|Gy\| |

## Memory Map / 内存映射

| Resource | Address | Size |
|----------|---------|------|
| VDMA AXI-Lite | 0x4300_0000 | 64 KB |
| FRAMEBUF_IN | 0x0D00_0000 | 307,200 B |
| FRAMEBUF_OUT | 0x0D90_0000 | 307,200 B |

## Key Bugs Fixed / 已修复的关键问题

1. **UDP Callback DDR Corruption** — callback 不直接写 FRAMEBUF，通过 BSS ring buffer 中转
2. **VDMA S2MM SOF Late Error** — `c_use_s2mm_fsync` 改为 0 (None)
3. **lwIP RTL8211E-VL PHY** — RGMII delay 配置 + conditional AN restart
4. **S2MM EOLEarlyErr** — HP0/HP1 分离 + line buffer depth 增至 2048
5. **Multi-transfer RS reset** — `vdma_start()` 重写 VDMACR 恢复 RS 位

## License

MIT

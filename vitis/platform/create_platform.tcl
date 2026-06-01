###############################################################
# Vitis Platform Reconstruction Script
# Usage: In Vitis XSCT Console, run: source create_platform.tcl
###############################################################

# Adjust XSA path to your local copy
set hw ../vivado/output/zybo_wrapper_V5.xsa

platform create -name {platformV5} \
    -hw ${hw} \
    -proc {ps7_cortexa9_0} -os {standalone}

platform write
platform generate -domains
platform active {platformV5}
domain active {zynq_fsbl}
bsp reload
domain active {standalone_domain}
bsp reload
bsp setlib -name lwip220 -ver 1.0

# Apply RTL8211E-VL PHY fix
# 1. Copy patches/xemacpsif_physpeed.c over BSP source
# 2. Copy platform/lwipopts.h over BSP include/lwipopts.h

bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform generate -domains standalone_domain

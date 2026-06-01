# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct E:\vivadoproject\pcam-udp\vitis_z720_sobel_udp_V5\platformV5\platform.tcl
# 
# OR launch xsct and run below command.
# source E:\vivadoproject\pcam-udp\vitis_z720_sobel_udp_V5\platformV5\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {platformV5}\
-hw {../vivado/output/zybo_wrapper_V5.xsa}\
-proc {ps7_cortexa9_0} -os {standalone}

platform write
platform generate -domains 
platform active {platformV5}
domain active {zynq_fsbl}
bsp reload
bsp reload
domain active {standalone_domain}
bsp reload
bsp setlib -name lwip220 -ver 1.0
bsp write
bsp reload
catch {bsp regenerate}
platform generate
platform generate -domains standalone_domain 

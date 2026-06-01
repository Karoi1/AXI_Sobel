###############################################################
# Vivado Project Reconstruction Script
# Usage: In Vivado Tcl Console, run: source create_project.tcl
###############################################################

set project_name  zybo_sobel
set project_dir   ./${project_name}
set part          xc7z020clg400-1
set board         digilentinc.com:zybo-z7-20:part0:1.2

puts "=== Creating Vivado Project: ${project_name} ==="

# 1. Create project
create_project ${project_name} ${project_dir} -part ${part}
set_property board_part ${board} [current_project]
set_property target_language VHDL [current_project]

# 2. Add HDL source files
add_files -norecurse hdl/sobel_core.v
add_files -norecurse hdl/sobel_kernel.v
add_files -norecurse hdl/line_buffer.v

# 3. Upgrade local IP to current Vivado version if needed
update_ip_catalog -rebuild

# 4. Rebuild Block Design from TCL
puts "=== Rebuilding Block Design ==="
source bd/zybo.tcl

# 5. Create HDL wrapper for the block design
set design_name zybo
make_wrapper -files [get_files ${design_name}.bd] -top
add_files -norecurse ${project_dir}/${project_name}.srcs/sources_1/bd/${design_name}/hdl/${design_name}_wrapper.vhd
update_compile_order -fileset sources_1

# 6. Add simulation files
add_files -fileset sim_1 -norecurse sim/tb_sobel_core.v
set_property top svtb_sobel_core [get_filesets sim_1]

puts "=== Project created. Run Synthesis + Implementation ==="
puts "  Launch runs:   launch_runs impl_1 -to_step write_bitstream -jobs 4"
puts "  Wait:          wait_on_run impl_1"
puts "  Export XSA:    write_hw_platform -fixed -include_bit -force -file output/zybo_wrapper.xsa"

open_project iq-demod-zynq/iq-demod-zynq.xpr -part xc7z020clg484-1
set_property board_part xilinx.com:zc702:part0:1.4 [current_project]

# 0) Make sure the project has your BD and sources
#    (replace 'system.bd' with your BD name/path)

read_bd iq-demod-zynq/iq-demod-zynq.srcs/sources_1/bd/top/top.bd
report_ip_status

# 1) Update BD outputs (IPs, HDL stubs, etc.)
generate_target all [get_files top.bd]
puts [get_files top.bd]

# 2) Regenerate a clean wrapper that matches BD external ports
make_wrapper -files [get_files top.bd] -top
# puts [get_files *]

# 3) Add the wrapper and make it the top
add_files -norecurse ./top_wrapper.v
set_property top top_wrapper [current_fileset]
update_compile_order -fileset sources_1

# 4) Synthesize using that wrapper
synth_design -top top_wrapper

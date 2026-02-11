# Start from a clean temp packaging project
create_project -in_memory -part xc7z020clg484-1

# Set the directory containing your source files
set src_dir "../ip_repo/iq_demodulator_1.0/hdl"

# Check if directory exists
if {![file isdirectory $src_dir]} {
    puts "ERROR: Directory '$src_dir' does not exist."
    return
}

# Get all files in the directory (recursively if needed)
# Example: HDL files (.v, .sv, .vhd) and constraints (.xdc)
set file_list [glob -nocomplain -directory $src_dir -types f *.v *.sv *.vhd *.xdc]

# If you want recursive search, uncomment:
# set file_list [glob -nocomplain -types f -directory $src_dir -tails -recursive *.v *.sv *.vhd *.xdc]

# Add each file to the project
if {[llength $file_list] == 0} {
    puts "No matching files found in $src_dir"
} else {
    add_files $file_list
    puts "Added [llength $file_list] files from $src_dir"
}

update_compile_order -fileset sources_1

# Package into an IP repo folder
ipx::package_project -root_dir ../ip_repo/iq_demodulator_1.0 -vendor quera.com -library ip -taxonomy /UserIP
set core [ipx::current_core]

# Make sure the HDL module is set as the top for the synthesis view
set_property top iq_demodulator [ipx::get_file_groups xilinx_anylanguagesynthesis -of_objects $core]

# Save and close the core
ipx::save_core $core
ipx::check_integrity -quiet $core
close_project

# Add the repo to your main project (where the BD lives)
ipx::unload_core $core

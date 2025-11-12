# Create a project
source recreate.tcl

# Launch synthesis run
launch_runs synth_1

# Wait for synthesis to complete
wait_on_run synth_1

# Open the synthesized design (optional, but good for checking results)
open_run synth_1

# Launch implementation run
launch_runs impl_1

# Wait for implementation to complete
wait_on_run impl_1

# Open the implemented design (optional)
open_run impl_1

# Generate the bitstream
write_bitstream -force top.bit

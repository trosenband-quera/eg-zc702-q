  # Create instance: xadc_wiz_0, and set properties
  set xadc_wiz_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 ]
  set_property -dict [ list \
   CONFIG.ADC_OFFSET_AND_GAIN_CALIBRATION {false} \
   CONFIG.BIPOLAR_VP_VN {true} \
   CONFIG.CHANNEL_ENABLE_TEMPERATURE {true} \
   CONFIG.CHANNEL_ENABLE_VBRAM {true} \
   CONFIG.CHANNEL_ENABLE_VCCAUX {true} \
   CONFIG.CHANNEL_ENABLE_VCCDDRO {true} \
   CONFIG.CHANNEL_ENABLE_VCCINT {true} \
   CONFIG.CHANNEL_ENABLE_VCCPAUX {true} \
   CONFIG.CHANNEL_ENABLE_VCCPINT {true} \
   CONFIG.CHANNEL_ENABLE_VP_VN {true} \
   CONFIG.CHANNEL_ENABLE_VREFN {true} \
   CONFIG.CHANNEL_ENABLE_VREFP {true} \
   CONFIG.ENABLE_CALIBRATION_AVERAGING {false} \
   CONFIG.ENABLE_VCCDDRO_ALARM {false} \
   CONFIG.ENABLE_VCCPAUX_ALARM {false} \
   CONFIG.ENABLE_VCCPINT_ALARM {false} \
   CONFIG.EXTERNAL_MUX_CHANNEL {VP_VN} \
   CONFIG.OT_ALARM {false} \
   CONFIG.SENSOR_OFFSET_AND_GAIN_CALIBRATION {false} \
   CONFIG.SEQUENCER_MODE {Continuous} \
   CONFIG.SINGLE_CHANNEL_SELECTION {TEMPERATURE} \
   CONFIG.USER_TEMP_ALARM {false} \
   CONFIG.VCCAUX_ALARM {false} \
   CONFIG.VCCINT_ALARM {false} \
   CONFIG.XADC_STARUP_SELECTION {channel_sequencer} \
 ] $xadc_wiz_0

echo "Generating RTL"
${PYTHON} ../../../vendor/esl_epfl_x_heep/hw/vendor/pulp_platform_register_interface/vendor/lowrisc_opentitan/util/regtool.py -r -t ../rtl ./regs.hjson
echo "Generating SW"
${PYTHON} ../../../vendor/esl_epfl_x_heep/hw/vendor/pulp_platform_register_interface/vendor/lowrisc_opentitan/util/regtool.py --cdefines -o ../../../../sw/external/lib/drivers/template_ip/template_ip_regs.h ./regs.hjson

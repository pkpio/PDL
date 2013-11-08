
# PlanAhead Launch Script for Post PAR Floorplanning, created by Project Navigator

create_project -name LUTs -dir "E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs/planAhead_run_1" -part xc5vlx110tff1136-1
set srcset [get_property srcset [current_run -impl]]
set_property design_mode GateLvl $srcset
set_property edif_top_file "E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs/main.ngc" [ get_property srcset [ current_run ] ]
add_files -norecurse { {E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs} }
set_property target_constrs_file "constraints.ucf" [current_fileset -constrset]
add_files [list {constraints.ucf}] -fileset [get_property constrset [current_run]]
open_netlist_design
read_xdl -file "E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs/main.ncd"
if {[catch {read_twx -name results_1 -file "E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs/main.twx"} eInfo]} {
   puts "WARNING: there was a problem importing \"E:/Dropbox/Works and collections/Elec/Xilinx_learning/LUTs/main.twx\": $eInfo"
}

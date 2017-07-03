## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,em3 linker.cmd package/cfg/app_pem3.oem3

# To simplify configuro usage in makefiles:
#     o create a generic linker command file name 
#     o set modification times of compiler.opt* files to be greater than
#       or equal to the generated config header
#
linker.cmd: package/cfg/app_pem3.xdl
	$(SED) 's"^\"\(package/cfg/app_pem3cfg.cmd\)\"$""\"C:/Users/The Brook/workspace_v7/project/project_new/rfPacketTx_CC1310DK_7XD_TI/.config/xconfig_app/\1\""' package/cfg/app_pem3.xdl > $@
	-$(SETDATE) -r:max package/cfg/app_pem3.h compiler.opt compiler.opt.defs

MODULENAME = alsa_bridge

ifneq ($(KERNELRELEASE),)
	obj-m := $(MODULENAME).o
	# NOTE: -U flags on CONFIG_* are INEFFECTIVE because the kernel build system's
	# -include include/linux/kconfig.h is processed before ccflags-y, and kconfig.h
	# redefines all CONFIG_* macros from autoconf.h after -U would take effect.
	# The real fix is in the CI workflow (build-diretta.yml) which patches:
	# 1. include/linux/kconfig.h to #undef CONFIG_RANDOMIZE_BASE
	#    + CONFIG_DYNAMIC_MEMORY_LAYOUT + CONFIG_SPARSEMEM_VMEMMAP
	# 2. alsa_bridge.c to insert the same three #undefs before #include<linux/module.h>
	# See: .github/workflows/build-diretta.yml (Build alsa_bridge.ko step)
else

PWD := $(shell pwd)

default: 


	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif



clean :
	rm -f ./.$(MODULENAME)*
	rm -f ./$(MODULENAME).mod*
	rm -f ./$(MODULENAME).o
	rm -f ./$(MODULENAME).ko
	rm -f -r ./.tmp_versions
	rm -f -r ./Module.symvers
	rm -f -r ./modules.order

# make KERNELDIR=xxxxxxxxxx

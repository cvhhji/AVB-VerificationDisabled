obj-m += avb_interceptor.o
avb_interceptor-y := avb_interceptor_main.o exec_gate.o bootconfig_proxy.o vbmeta_proxy.o

ifndef AVB_DDK_TARGET
AVB_DDK_TARGET := unknown
endif
ccflags-y += -DAVB_DDK_TARGET=\"$(AVB_DDK_TARGET)\"

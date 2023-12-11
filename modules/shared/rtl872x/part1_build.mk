
ifeq (,$(SYSTEM_PART1_MODULE_VERSION))
$(error SYSTEM_PART1_MODULE_VERSION not defined)
endif

ifeq (,$(SYSTEM_PART1_MODULE_DEPENDENCY))
SYSTEM_PART1_MODULE_DEPENDENCY=0,0,0
endif

ifeq (,$(SYSTEM_PART1_MODULE_DEPENDENCY2))
SYSTEM_PART1_MODULE_DEPENDENCY2=0,0,0
endif

GLOBAL_DEFINES += MODULE_VERSION=$(SYSTEM_PART1_MODULE_VERSION)
GLOBAL_DEFINES += MODULE_FUNCTION=$(MODULE_FUNCTION_SYSTEM_PART)
GLOBAL_DEFINES += MODULE_INDEX=1
GLOBAL_DEFINES += MODULE_DEPENDENCY=${SYSTEM_PART1_MODULE_DEPENDENCY}
GLOBAL_DEFINES += MODULE_DEPENDENCY2=${SYSTEM_PART1_MODULE_DEPENDENCY2}

ifeq ("$(INCLUDE_APP)", "y")
GLOBAL_DEFINES += INCLUDE_APP
endif

LINKER_FILE=$(SYSTEM_PART1_MODULE_PATH)/linker.ld
LINKER_DEPS += $(LINKER_FILE)

LINKER_DEPS += $(SHARED_MODULAR)/linker_system_part1_common.ld
LINKER_DEPS += $(SYSTEM_PART1_MODULE_PATH)/module_system_part1_export.ld
LINKER_DEPS += $(USER_PART_MODULE_PATH)/module_user_export.ld

LINKER_DEPS += $(NEWLIB_TWEAK_SPECS)
LDFLAGS += --specs=$(NEWLIB_TWEAK_SPECS)
LDFLAGS += -L$(SYSTEM_PART1_MODULE_PATH)
LDFLAGS += -L$(SHARED_MODULAR)
LDFLAGS += -L$(USER_PART_MODULE_PATH)
LDFLAGS += -T$(LINKER_FILE)
ifneq ($(PLATFORM_DFU),)
LDFLAGS += -Wl,--defsym,PLATFORM_DFU=$(PLATFORM_DFU)
endif
LDFLAGS += -Wl,-Map,$(TARGET_BASE).map

MAIN_STACK_SIZE = 4096
LDFLAGS += -Wl,--defsym,__STACKSIZE__=$(MAIN_STACK_SIZE)
LDFLAGS += -Wl,--defsym,__STACK_SIZE=$(MAIN_STACK_SIZE)

# assembler startup script
ASRC += $(COMMON_BUILD)/arm/startup/startup_$(MCU_DEVICE_LC).S
ASFLAGS += -I$(COMMON_BUILD)/arm/startup
ASFLAGS +=  -Wa,--defsym,SPARK_INIT_STARTUP=1
ASFLAGS += -D__STACKSIZE__=$(MAIN_STACK_SIZE) -D__STACK_SIZE=$(MAIN_STACK_SIZE)

ifneq ("$(HAL_MINIMAL)","y")
USE_PRINTF_FLOAT ?= y
endif

ifeq ("$(USE_PRINTF_FLOAT)","y")
LDFLAGS += -u _printf_float -u _scanf_float
endif

LDFLAGS += -u uxTopUsedPriority

INCLUDE_DIRS += $(SHARED_MODULAR)/inc/system-part1
SYSTEM_PART1_MODULE_SRC_PATH = $(SYSTEM_PART1_MODULE_PATH)/src

CPPSRC += $(call target_files,$(SYSTEM_PART1_MODULE_SRC_PATH),*.cpp)
CSRC += $(call target_files,$(SYSTEM_PART1_MODULE_SRC_PATH),*.c)

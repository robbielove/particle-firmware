TARGET_AMBD_SDK_PATH = $(AMBD_SDK_MODULE_PATH)
TARGET_AMBD_SDK_SOC_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/component/soc/realtek/amebad
TARGET_AMBD_SDK_OS_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/component/os
TARGET_AMBD_SDK_COMMON_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/component/common
TARGET_AMBD_SDK_PROJECT_LIB_PATH = $(TARGET_AMBD_SDK_PATH)/ambd_sdk/project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp/asdk/lib/application
TARGET_AMBD_SDK_BLUETOOTH_PATH = $(TARGET_AMBD_SDK_COMMON_PATH)/bluetooth/realtek/sdk

INCLUDE_DIRS += $(TARGET_AMBD_SDK_PATH)
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/cmsis
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/fwlib/include
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/swlib/include
#INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/swlib/string
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/app/monitor/include
INCLUDE_DIRS += $(TARGET_AMBD_SDK_OS_PATH)/os_dep/include
INCLUDE_DIRS += $(TARGET_AMBD_SDK_OS_PATH)/freertos
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/api
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/api/platform
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/api/wifi
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)/misc
INCLUDE_DIRS += $(TARGET_AMBD_SDK_SOC_PATH)

INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/drivers/wlan/realtek/include
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/drivers/wlan/realtek/src/osdep
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/drivers/wlan/realtek/src/osdep/freertos

INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/board/amebad/lib
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/board/amebad/src
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/board/amebad/src/vendor_cmd
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/board/common/inc
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/app
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/bluetooth/gap
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/bluetooth/profile
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/bluetooth/profile/client
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/bluetooth/profile/server
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/os
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/platform
INCLUDE_DIRS += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/inc/stack
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/api/at_cmd
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/mbed/targets/hal/rtl8721d
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/mbed/hal_ext
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/mbed/hal
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/file_system/fatfs
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/file_system/fatfs/r0.10c/include
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/file_system/ftl
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/drivers/usb/device_new/core
INCLUDE_DIRS += $(TARGET_AMBD_SDK_COMMON_PATH)/drivers/usb/common_new

# Hack of the century!
LIBS_EXT_END += $(TARGET_AMBD_SDK_PROJECT_LIB_PATH)/lib_wlan.a
LIBS_EXT_END += $(TARGET_AMBD_SDK_PROJECT_LIB_PATH)/lib_wps.a
ifneq ("$(MODULE)", "user-part")
LIBS_EXT_END += $(TARGET_AMBD_SDK_BLUETOOTH_PATH)/board/amebad/lib/btgap.a
endif
LIBS_EXT_END += -Wl,--wrap=usb_hal_read_packet $(TARGET_AMBD_SDK_PROJECT_LIB_PATH)/lib_usbd_new.a

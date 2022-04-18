#!/bin/sh

### cd drivers; ./scripts/fullmac/explorer.sh

PROJECT=EXPLORER
ENABLE="RPU_MODE_EXPLORER IMG_PCIE HACK_MONITOR_MODE HACK_SERDES_CONFIG KERNEL_VERSION_4_4 RPU_64_BIT_DMA_SUPPORT"
DISABLE="notyet disabled KERNEL_VERSION_4_1 RPU_RESTRICTED_DMA"

#The following flags are left in the code to facilitate compile time
#enable/disable capability
#PROD_MODE STATS_SUPPORT TEST_MODE

###

SCRIPTS_COMMON_PATH=scripts/softmac/generate/generate_driver_common.sh

. $SCRIPTS_COMMON_PATH

generate_driver "$ENABLE" "$DISABLE"

# remove unnecessary files
rm -rf $PWD/$PROJECT/fw_load/meta

find $PWD/$PROJECT/ -size 0 | xargs rm -rf

# linux
rm -rf $PWD/$PROJECT/linux/softmac

# restore Makefiles
cp -r $PWD/linux/fullmac/Makefile* $PWD/$PROJECT/linux/fullmac/
rm -rf $PWD/$PROJECT/linux/fullmac/Makefile.lpw

# restore proj files
mkdir -p $PWD/$PROJECT/windows/fullmac/HAL
cp -r $PWD/windows/fullmac/WDI/*.* $PWD/$PROJECT/windows/fullmac/WDI/
cp -r $PWD/windows/fullmac/HAL/*.* $PWD/$PROJECT/windows/fullmac/HAL/
cp -r $PWD/windows/fullmac/img_wlan_fullmac.explorer.sln $PWD/$PROJECT/windows/fullmac/

cp -r $PWD/windows/fullmac/WDI/src/tlv_utils.cpp $PWD/$PROJECT/windows/fullmac/WDI/src/

# export shared files
cp -r $PWD/$PROJECT/shared/explorer/* $PWD/$PROJECT/shared/
rm -rf $PWD/$PROJECT/shared/explorer
rm -rf $PWD/$PROJECT/shared/lpw

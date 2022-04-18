#!/bin/sh

### cd drivers; ./scripts/fullmac/lpw.sh

PROJECT=LPW
ENABLE="IMG_PCIE KERNEL_VERSION_4_4 RPU_RESTRICTED_DMA"
DISABLE="RPU_MODE_EXPLORER notyet disabled HACK_MONITOR_MODE HACK_SERDES_CONFIG KERNEL_VERSION_4_1 RPU_64_BIT_DMA_SUPPORT"

#The following flags are left in the code to facilitate compile time
#enable/disable capability
#PROD_MODE STATS_SUPPORT TEST_MODE
###

SCRIPTS_COMMON_PATH=scripts/softmac/generate/generate_driver_common.sh

. $SCRIPTS_COMMON_PATH

generate_driver "$ENABLE" "$DISABLE"

##remove unnecessary files
rm -rf $PWD/$PROJECT/fw_load/meta

find $PWD/$PROJECT/ -size 0 | xargs rm -rf

# HAL
rm -rf $PWD/$PROJECT/osal/hal/src/hpqm.c
rm -rf $PWD/$PROJECT/osal/hal/inc/hpqm.h
rm -rf $PWD/$PROJECT/osal/hal/src/hpqm_reg.h
rm -rf $PWD/$PROJECT/osal/hal/src/hpqm_reg.c

# remove procfs files 
rm -rf $PWD/$PROJECT/linux/fullmac/src/procfs/host_stats.c
rm -rf $PWD/$PROJECT/linux/fullmac/src/procfs/lmac_stats.c
rm -rf $PWD/$PROJECT/linux/fullmac/src/procfs/params.c
rm -rf $PWD/$PROJECT/linux/fullmac/src/procfs/umac_stats.c

rm -rf $PWD/$PROJECT/linux/softmac

# restore Makefiles
cp -r $PWD/linux/fullmac/Makefile* $PWD/$PROJECT/linux/fullmac/
rm -rf $PWD/$PROJECT/linux/fullmac/Makefile.explorer

# restore proj files
mkdir -p $PWD/$PROJECT/windows/fullmac/HAL
cp -r $PWD/windows/fullmac/WDI/*.* $PWD/$PROJECT/windows/fullmac/WDI/
cp -r $PWD/windows/fullmac/HAL/*.* $PWD/$PROJECT/windows/fullmac/HAL/
cp -r $PWD/windows/fullmac/img_wlan_fullmac.lpw.sln $PWD/$PROJECT/windows/fullmac/

cp -r $PWD/windows/fullmac/WDI/src/tlv_utils.cpp $PWD/$PROJECT/windows/fullmac/WDI/src/

# export shared files
cp -r $PWD/$PROJECT/shared/lpw/* $PWD/$PROJECT/shared/
rm -rf $PWD/$PROJECT/shared/explorer
rm -rf $PWD/$PROJECT/shared/lpw

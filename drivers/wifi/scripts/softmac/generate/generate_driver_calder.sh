#!/bin/sh
###
PROJECT=CALDER

ENABLE="RPU_MODE_EXPLORER MULTI_CHAN_SUPPORT HAL_PCIE RPU_DMA_ZONE RPU_ACCESS_FULL_DDR RPU_SGI_SUPPORT SOC_ST RPU_HPQM_SUPPORT DEV_BOARD_VCU108 SOC_CALDER RPU_DFS_SUPPORT RPU_2GBAND_SUPPORT RPU_5GBAND_SUPPORT"
DISABLE="RPU_MODE_LPW PERF_PROFILING RPU_SNAPSHOT BB_LOOPBACK LOOPBACK DFS_TEST notyet BEACON_TIME_STAMP RPU_RESTRICTED_DMA"
###

SCRIPTS_COMMON_PATH=./../../scripts/softmac/generate/generate_driver_common.sh
. $SCRIPTS_COMMON_PATH
generate_driver "$ENABLE" "$DISABLE"
##remove unnecessary files
rm -f $PWD/$PROJECT/src/hal_hostport.c
rm -rf $PWD/$PROJECT/fw_load/meta
rm -rf $PWD/$PROJECT/fw_load/mips/rompatching/
rm -rf $PWD/$PROJECT/shared_amd
rm -rf $PWD/$PROJECT/shared_whisper


# restore Makefiles
cp -r $PWD/Makefile $PWD/$PROJECT/
cp -r $PWD/Makefile.calder $PWD/$PROJECT/
rm -rf $PROJECT/shared/lpw
rm -rf $PROJECT/shared/explorer

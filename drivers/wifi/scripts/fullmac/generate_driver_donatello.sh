#!/bin/sh

### cd drivers; ./scripts/fullmac/generate_driver_donatello.sh

PROJECT=DONATELLO
ENABLE="SOC_DONATELLO RPU_CONFIG_FMAC RPU_64_BIT_DMA_SUPPORT RPU_SUPPORT_WAPI"
DISABLE="notyet disabled RPU_RESTRICTED_DMA HACK_SERDES_CONFIG RPU_MODE_EXPLORER HACK_MONITOR_MODE RPU_CONFIG_72 RPU_BSS_DB_SUPPORT"

###

SCRIPTS_COMMON_PATH=scripts/softmac/generate/generate_driver_common.sh

. $SCRIPTS_COMMON_PATH

generate_driver "$ENABLE" "$DISABLE"

# remove unnecessary files
find $PWD/$PROJECT/ -size 0 -type f | xargs rm -rf

# linux/softmac
rm -rf $PWD/$PROJECT/linux

# windows
rm -rf $PWD/$PROJECT/windows

# restore Makefiles
cp -r $PWD/os_if/os/lnx/Makefile* $PWD/$PROJECT/os_if/os/lnx/
rm -rf $PWD/$PROJECT/os_if/os/lnx/Makefile.lpw
rm -rf $PWD/$PROJECT/os_if/os/lnx/Makefile.explorer


# export shared files
cp -r $PWD/$PROJECT/shared/donatello/* $PWD/$PROJECT/shared/
rm -rf $PWD/$PROJECT/shared/explorer
rm -rf $PWD/$PROJECT/shared/lpw
rm -rf $PWD/$PROJECT/shared/donatello

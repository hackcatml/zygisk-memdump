MODDIR=${0%/*}

MAIN_ARCH=$(resetprop ro.product.cpu.abi)
TOOL_NAME=$(ls ${MODDIR}/tool/${MAIN_ARCH})
cp ${MODDIR}/tool/${MAIN_ARCH}/${TOOL_NAME} /data/local/tmp/
chmod 755 /data/local/tmp/${TOOL_NAME}
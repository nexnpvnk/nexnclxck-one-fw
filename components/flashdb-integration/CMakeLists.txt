# Upstream FlashDB and its bundled FAL remain unmodified.
set(FLASHDB_ROOT ${CMAKE_CURRENT_LIST_DIR}/../FlashDB)
if(NOT FLASHDB_PORT_INCLUDE_DIR)
    message(FATAL_ERROR "Set FLASHDB_PORT_INCLUDE_DIR to the directory containing fdb_cfg.h and fal_cfg.h")
endif()
add_library(flashdb STATIC
    ${FLASHDB_ROOT}/src/fdb.c
    ${FLASHDB_ROOT}/src/fdb_kvdb.c
    ${FLASHDB_ROOT}/src/fdb_utils.c
    ${FLASHDB_ROOT}/port/fal/src/fal.c
    ${FLASHDB_ROOT}/port/fal/src/fal_flash.c
    ${FLASHDB_ROOT}/port/fal/src/fal_partition.c
)
target_include_directories(flashdb PUBLIC
    ${FLASHDB_ROOT}/inc
    ${FLASHDB_ROOT}/port/fal/inc
    ${FLASHDB_PORT_INCLUDE_DIR}
)

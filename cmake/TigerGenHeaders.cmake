# platform-specific config file
configure_file("${Tiger_SOURCE_DIR}/cmake/templates/ti_config.h.in" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/ti_config.h")
configure_file("${Tiger_SOURCE_DIR}/cmake/templates/ti_config.h.in" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/${TI_INTERNAL_NAME}/ti_config.h")
install(FILES "${TIGER_CONFIG_FILE_INCLUDE_DIR}/ti_config.h" DESTINATION ${TIGER_INCLUDE_INSTALL_PATH}/${TI_INTERNAL_NAME} COMPONENT dev)

# platform-specific config file
ti_compiler_optimization_fill_cpu_config()
configure_file("${Tiger_SOURCE_DIR}/cmake/templates/ti_cpu_config.h.in" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/ti_cpu_config.h")

# ----------------------------------------------------------------------------
#  tiger_modules.hpp based on actual modules list
# ----------------------------------------------------------------------------
set(TIGER_MODULE_DEFINITIONS_CONFIGMAKE "")

set(TIGER_MOD_LIST ${TIGER_MODULES_PUBLIC})
ti_list_sort(TIGER_MOD_LIST)
foreach(m ${TIGER_MOD_LIST})
  string(TOUPPER "${m}" m)
  set(TIGER_MODULE_DEFINITIONS_CONFIGMAKE "${TIGER_MODULE_DEFINITIONS_CONFIGMAKE}#define HAVE_${m}\n")
endforeach()

set(TIGER_MODULE_DEFINITIONS_CONFIGMAKE "${TIGER_MODULE_DEFINITIONS_CONFIGMAKE}\n")

#set(TIGER_MOD_LIST ${TIGER_MODULES_DISABLED_USER} ${TIGER_MODULES_DISABLED_AUTO} ${TIGER_MODULES_DISABLED_FORCE})
#ti_list_sort(TIGER_MOD_LIST)
#foreach(m ${TIGER_MOD_LIST})
#  string(TOUPPER "${m}" m)
#  set(TIGER_MODULE_DEFINITIONS_CONFIGMAKE "${TIGER_MODULE_DEFINITIONS_CONFIGMAKE}#undef HAVE_${m}\n")
#endforeach()

configure_file("${Tiger_SOURCE_DIR}/cmake/templates/${TI_INTERNAL_NAME}_modules.hpp.in" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/${TI_INTERNAL_NAME}/${TI_INTERNAL_NAME}_modules.hpp")
install(FILES "${TIGER_CONFIG_FILE_INCLUDE_DIR}/${TI_INTERNAL_NAME}/${TI_INTERNAL_NAME}_modules.hpp" DESTINATION ${TIGER_INCLUDE_INSTALL_PATH}/${TI_INTERNAL_NAME} COMPONENT dev)

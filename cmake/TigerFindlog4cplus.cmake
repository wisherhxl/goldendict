# ----------------------
# Find log4cplus

find_package(log4cplus REQUIRED PATHS ${TI_DEV_PATH} NO_DEFAULT_PATH)

set(log4cplus_LIBRARIES "log4cplus::log4cplusU")
ti_list_components(log4cplus)
# ----------------------
# Find log4cplus

find_package(log4cplus CONFIG REQUIRED)

set(log4cplus_LIBRARIES "log4cplus::log4cplus")
ti_list_components(log4cplus)
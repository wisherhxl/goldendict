# --------------------
#  Optional: Set Tiger and Qt install dir
#  to find them automatically
# set(QT_INSTALL_DIR
    # "$ENV{QT_DIR}"
    # "${QT_SEARCH_DIR}"
# )
# set(TIGER_INSTALL_DIR
    # "$ENV{TIGER_DIR}"
    # "${TIGER_SEARCH_DIR}"
# )
# set(PROTOBUF_INSTALL_DIR
    # "$ENV{PROTOBUF_DIR}"
    # "${PROTOBUF_SEARCH_DIR}"
# )
# set(BOOST_INSTALL_DIR
    # "$ENV{BOOST_DIR}"
    # "${BOOST_SEARCH_DIR}"
# )

# --------------------
# 3rd-party libraries wich cmake configuration
set(3rd_party_libs
		log4cplus # logger
		# basler   # camera
		# sentech  # camera
		# dahua    # camera
		# acs      # motion
        # glew   # opengl
)

set(3rd_party_lib_links
		log4cplus::log4cplusU
		# basler   # camera
		# sentech  # camera
		# dahua    # camera
		# acs      # motion
        # glew   # opengl
)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
		${default_3rd_party_dir})
foreach(3rd_party_lib ${3rd_party_libs})
	find_package( ${3rd_party_lib} REQUIRED PATHS ${TI_DEV_PATH} NO_DEFAULT_PATH)
endforeach()

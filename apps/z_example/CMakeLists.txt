ti_add_qt_app(z_example MODULES tiger proto EXTRAS ${qt_link})

add_test(NAME z_example_smoke COMMAND $<TARGET_FILE:z_example> --smoke)

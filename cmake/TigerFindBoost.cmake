# ----------------------
# Find Boost

set(boost_modules
    atomic
    chrono
    container
    context
    contract
    coroutine
    date_time
    exception
    fiber
    filesystem
    graph
    headers
    iostreams
    json
    locale
    log
    math_c99
    math_c99f
    math_c99l
    math_tr1
    math_tr1f
    math_tr1l
    nowide
    program_options
    python
    random
    regex
    serialization
    stacktrace_noop
    stacktrace_windbg_cached
    stacktrace_windbg
    system
    unit_test_framework
    thread
    timer
    type_erasure
    url
    wave
    # mpi
    # graph_parallel
)
find_package(Boost COMPONENTS ${boost_modules} REQUIRED PATHS ${TI_DEV_PATH} NO_DEFAULT_PATH)

ti_list_components(Boost)


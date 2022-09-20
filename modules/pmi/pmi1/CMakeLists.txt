target_include_directories(pmi-obj PUBLIC include)
target_sources(pmi-obj PRIVATE
        simple/simple_pmi.c
        simple/simple_pmiutil.c
)

add_executable(pmi-test simple/test.c)
target_link_libraries(pmi-test PRIVATE pmi-obj)
target_compile_definitions(pmi-test PRIVATE _GNU_SOURCE)
set_target_properties(pmi-test PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)

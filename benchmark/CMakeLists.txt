link_libraries(fb_lib)

add_executable(pingpong_sym pingpong_sym.cpp )
if(FB_USE_UCX)
    add_executable(pingpong_prg_ucx pingpong_prg_ucx.cpp)
else()
    add_executable(pingpong_prg pingpong_prg.cpp)
endif()
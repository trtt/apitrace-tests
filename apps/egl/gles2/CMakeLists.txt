# http://www.glfw.org/docs/latest/build.html#build_link_cmake_source
set (GLFW_BUILD_EXAMPLES 0)
set (GLFW_BUILD_TESTS 0)
set (GLFW_BUILD_DOCS 0)
set (GLFW_INSTALL 0)
set (GLFW_SUFFIX _egl_es2)
set (GLFW_USE_EGL 1)
set (GLFW_CLIENT_LIBRARY "glesv2")
add_subdirectory (${CMAKE_SOURCE_DIR}/thirdparty/glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw EXCLUDE_FROM_ALL)
set (GLFW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include)
set (GLFW_LIBRARY glfw${GLFW_SUFFIX} ${GLFW_LIBRARIES})


add_definitions (-DGLFW_INCLUDE_ES2)

include_directories(
    ${GLFW_INCLUDE_DIR}
    ${EGL_INCLUDE_DIRS}
    ${GLESV2_INCLUDE_DIRS}
)

set (api egl_gles2)

set (targets
    debug_marker
    tri_glsl
    map_buffer
)

foreach (target ${targets})
    add_executable (${api}_${target} ${target}.cpp)
    set_target_properties (${api}_${target} PROPERTIES OUTPUT_NAME ${target})
    target_link_libraries (${api}_${target}
        glad
        ${GLFW_LIBRARY}
        ${GLESV2_LIBRARIES}
        ${EGL_LIBRARIES}
    )
endforeach (target)

add_library (rtld_local_module MODULE rtld_local_module.cpp)
target_link_libraries (rtld_local_module ${GLFW_LIBRARY} ${GLESV2_LIBRARIES} ${EGL_LIBRARIES})
set_target_properties (rtld_local_module PROPERTIES OUTPUT_NAME rtld_local)
add_executable (${api}_rtld_local rtld_local.cpp)
set_target_properties (${api}_rtld_local PROPERTIES OUTPUT_NAME rtld_local)
target_link_libraries (${api}_rtld_local ${CMAKE_DL_LIBS})

add_app_tests ()

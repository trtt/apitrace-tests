include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/../d3dcommon
    ${DirectX_D3D10_INCLUDE_DIR}
)

link_libraries (
    ${DirectX_DXGI_LIBRARY}
    ${DirectX_D3D10_LIBRARY}
)

set (api d3d10)

set (targets
    tri
    map_buffer
)

foreach (target ${targets})
    add_executable (${api}_${target} ${target}.cpp)
    set_target_properties (${api}_${target} PROPERTIES OUTPUT_NAME ${target})
endforeach (target)

add_app_tests ()

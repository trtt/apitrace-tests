function (ADD_APP_TEST)
    cmake_parse_arguments(
        TEST
        # Options
        ""
        # One value args
        "NAME;TARGET;REF"
        # Multi value args
        "ARGS"
        ${ARGN}
    )

    if (APITRACE_EXECUTABLE AND APITRACE_SOURCE_DIR)
        add_test(
            NAME app_${TEST_NAME}
            COMMAND
            ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/app_driver.py
                --apitrace ${APITRACE_EXECUTABLE}
                --apitrace-source ${APITRACE_SOURCE_DIR}
                --api ${api}
                --ref-dump ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_REF}
                --
                "$<TARGET_FILE:${TEST_TARGET}>"
                ${TEST_ARGS}
        )

        # Report skips to ctest
        if (NOT CMAKE_VERSION VERSION_LESS 3.0.0)
            set_tests_properties (app_${TEST_NAME} PROPERTIES
                SKIP_RETURN_CODE 125
                ENVIRONMENT CMAKE_SKIP_RETURN_CODE=125
            )
        endif ()
    endif ()
endfunction ()

macro (add_app_tests)
    # Automatically discover tests from *.ref.txt files
    file (GLOB TEST_REFS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.ref.txt)
    list (SORT TEST_REFS)
    foreach (TEST_REF ${TEST_REFS})
        if (APITRACE_EXECUTABLE AND APITRACE_SOURCE_DIR)
            # Search for `//!test` header
            file (READ ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_REF} TEST_REF_DATA 4096)
            string (REGEX MATCH "^//!([_a-zA-Z0-9]+)([^\r\n]*)?" TEST_REF_HEAD "${TEST_REF_DATA}")
            set (TEST_TARGET "${CMAKE_MATCH_1}")
            set (TEST_ARGS ${CMAKE_MATCH_2})

            # Split the remaning arguments
            string (REPLACE " " ";" TEST_ARGS_SPLIT "${TEST_ARGS}")

            if (TEST_TARGET)
                if (TARGET "${api}_${TEST_TARGET}")
                    message (STATUS "Found test ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_REF}")
                    get_filename_component (TEST_NAME ${TEST_REF} NAME_WE)
                    add_app_test (
                        NAME ${api}_${TEST_NAME}
                        TARGET ${api}_${TEST_TARGET}
                        REF ${TEST_REF}
                        ARGS ${TEST_ARGS_SPLIT}
                    )
                else ()
                    message (STATUS "Skipping test ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_REF} because ${api}_${TEST_TARGET} is not being built.")
                endif ()
            endif ()
        endif ()
    endforeach ()
endmacro ()

if (OPENGL_FOUND)
    add_subdirectory (gl)
endif ()

if (EGL_FOUND)
    add_subdirectory (egl)
endif ()

if (DirectX_FXC_EXECUTABLE)
    add_subdirectory (d3dcommon)
endif ()

if (DirectX_D3D11_1_FOUND)
    add_subdirectory (d3d11_1)
endif ()

if (DirectX_D3D11_FOUND AND DirectX_D3D9_FOUND)
    add_subdirectory (d3d11)
endif ()

if (DirectX_D3D10_1_FOUND)
    add_subdirectory (d3d10_1)
endif ()

if (DirectX_D3D10_FOUND)
    add_subdirectory (d3d10)
endif ()

if (DirectX_D3D9_FOUND)
    add_subdirectory (d3d9)
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    if (DirectX_D3D8_FOUND)
        add_subdirectory (d3d8)
    endif ()

    if (DirectX_D3D_FOUND)
        add_subdirectory (d3d7)
    endif ()
endif ()

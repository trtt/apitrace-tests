cmake_minimum_required (VERSION 2.8.11)


option (TEST_UNRELIABLE "Run unreliable tests" OFF)


project (apitrace-tests)

find_path (APITRACE_SOURCE_DIR NAMES cmake/FindDirectX.cmake PATHS .. ../.. DOC "apitrace source tree" NO_DEFAULT_PATH)
if (NOT EXISTS ${APITRACE_SOURCE_DIR})
    message (WARNING "Please specify path to apitrace source tree via APITRACE_SOURCE_DIR")
endif ()

find_program (APITRACE_EXECUTABLE NAMES apitrace PATHS ${APITRACE_SOURCE_DIR} DOC "apitrace executable")
if (NOT EXISTS ${APITRACE_EXECUTABLE})
    message (WARNING "Please specify path to apitrace executable via APITRACE_EXECUTABLE")
endif ()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package (PythonInterp 2.7 REQUIRED)
if (NOT PYTHON_VERSION_MAJOR EQUAL 2)
    message (FATAL_ERROR "Python 2.x required and requested, but Python ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR} found.")
endif ()
find_package (PkgConfig)

# Set default built type
if (NOT CMAKE_BUILD_TYPE)
   set (CMAKE_BUILD_TYPE Debug
       CACHE
       STRING "Choose the build type, options are: None, Debug, Release, RelWithDebInfo, or MinSizeRel."
       FORCE)
endif (NOT CMAKE_BUILD_TYPE)

include (CMakeParseArguments)

find_package (OpenGL)
find_package (GLEW)

if (WIN32)
    find_package (DirectX)
elseif (PKG_CONFIG_FOUND)
	pkg_check_modules (EGL egl)
	pkg_check_modules (GLESV1 glesv1_cm)
	pkg_check_modules (GLESV2 glesv2)
endif ()

# Check for the presence of several python packages, which are needed to build
# generated tests.
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import PIL"
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE IMPORT_PIL_RESULT)
if (IMPORT_PIL_RESULT EQUAL 0)
    set (PIL_FOUND 1)
else ()
    message (STATUS "python PIL module not found")
endif ()

include (CheckCXXCompilerFlag)

if (UNIX)
	link_libraries(m)
endif (UNIX)

if (WIN32)
	# Don't define min/max macros
	add_definitions (-DNOMINMAX)
endif ()

if (MSVC)
    if (${MSVC_VERSION} LESS 1800)
        message (FATAL_ERROR "Visual Studio 2013 or later required")
    endif ()

    # Enable math constants defines
    add_definitions (-D_USE_MATH_DEFINES)

    # Silence several MSVC pedantic warnings
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)
    add_definitions (-wd4244) # conversion' conversion from 'type1' to 'type2', possible loss of data
    add_definitions (-wd4305) # truncation from 'type1' to 'type2'
else ()
    # Enable and require C++11
    #
    # We must use `-std=gnu++11` instead `-std=c++11` as the latter defines
    # __STRICT_ANSI__ which prevents _isnan from being declared with MinGW.
    #
    # See also:
    # - https://gcc.gnu.org/projects/cxx0x.html
    # - http://clang.llvm.org/cxx_status.html
    check_cxx_compiler_flag ("-std=gnu++11" CXX_COMPILER_FLAG_STD_GNUXX11)
    if (CXX_COMPILER_FLAG_STD_GNUXX11)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag ("-std=gnu++0x" CXX_COMPILER_FLAG_STD_GNUXX0X)
        if (NOT CXX_COMPILER_FLAG_STD_GNUXX0X OR
            CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.6")
            message (FATAL_ERROR "GCC 4.6 or later required for adequate C++11 support")
        else ()
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
        endif ()
    else ()
        message (FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} compiler does not support C++11")
    endif ()
endif ()

if (MINGW)
    # Avoid depending on MinGW runtime DLLs
    check_cxx_compiler_flag (-static-libgcc HAVE_STATIC_LIBGCC_FLAG)
    if (HAVE_STATIC_LIBGCC_FLAG)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc")
    endif ()
    check_cxx_compiler_flag (-static-libstdc++ HAVE_STATIC_LIBSTDCXX_FLAG)
    if (HAVE_STATIC_LIBSTDCXX_FLAG)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
    endif ()
endif ()

if (APPLE)
    # Silence deprecated function warnings
    add_definitions (-Wno-deprecated-declarations)
endif ()

enable_testing()

add_definitions (-DEXIT_SKIP=125)

include_directories (thirdparty/glad/include)
add_subdirectory (thirdparty/glad)

add_subdirectory (apps)
add_subdirectory (traces)
add_subdirectory (profiling)

# FIXME: Some of these tests are not reliable on some platforms:
#
# - They use 64x64 drawables, but on Windows these get bumped to bigger sizes
#   to make space for title bar, buttons, etc.
# - The 64x64 resize is not effctive on some platforms (e.g., xvfb-run)
#
if (OPENGL_FOUND AND PIL_FOUND)
    add_subdirectory (cli)
    if (TEST_UNRELIABLE)
        add_subdirectory (trim_stress)
    endif ()
endif ()

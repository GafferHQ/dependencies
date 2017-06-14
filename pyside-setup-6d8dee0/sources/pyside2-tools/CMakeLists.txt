cmake_minimum_required(VERSION 3.0)
project(pyside2-tools)

find_package(Shiboken2 2.0.0 REQUIRED)
find_package(Qt5 REQUIRED Core)
find_package(PySide2 2.0.0 REQUIRED)

set(pyside2_tools_MAJOR_VERSION "2")
set(pyside2_tools_MINOR_VERSION "0")
set(pyside2_tools_MICRO_VERSION "0")
set(pyside2_tools_VERSION "${pyside2_tools_MAJOR_VERSION}.${pyside2_tools_MINOR_VERSION}.${pyside2_tools_MICRO_VERSION}")

configure_file("pyside2uic/__init__.py.in" "${CMAKE_CURRENT_BINARY_DIR}/pyside2uic/__init__.py" @ONLY)

option(BUILD_TESTS "Build tests." TRUE)

if(CMAKE_HOST_APPLE)
    set(OSX_USE_LIBCPP "OFF" CACHE BOOL "Explicitly link the libc++ standard library (useful for osx deployment targets lower than 10.9.")
    if(OSX_USE_LIBCPP)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()

# UIC stuff
if (NOT SITE_PACKAGE)
    execute_process(
        COMMAND ${SHIBOKEN_PYTHON_INTERPRETER} -c "if True:
            from distutils import sysconfig
            from os.path import sep
            print(sysconfig.get_python_lib(1, 0, prefix='${CMAKE_INSTALL_PREFIX}').replace(sep, '/'))
            "
        OUTPUT_VARIABLE SITE_PACKAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT SITE_PACKAGE)
        message(FATAL_ERROR "Could not detect Python module installation directory.")
    endif()
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pyside2-uic
        DESTINATION bin
        PERMISSIONS
        OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_EXECUTE GROUP_READ
        WORLD_EXECUTE WORLD_READ)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pyside2uic/__init__.py"
        DESTINATION "${SITE_PACKAGE}/pyside2uic")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pyside2uic
        DESTINATION ${SITE_PACKAGE}
        PATTERN "*.in" EXCLUDE
        PATTERN "*pyside2uic/*.1" EXCLUDE
        PATTERN "*pyside2uic/__init__.py" EXCLUDE
        PATTERN "*pyside2uic\\__init__.py" EXCLUDE)

# Man pages for pyside2-uic
if (NOT win32)
    file(GLOB manpages "${CMAKE_CURRENT_SOURCE_DIR}/pyside2uic/*.1")
    install(FILES ${manpages} DESTINATION share/man/man1)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

add_custom_target(uninstall "${CMAKE_COMMAND}"
                  -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${pyside2_tools_VERSION})
add_custom_target(dist
    COMMAND mkdir -p "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}" &&
            git log > "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}/ChangeLog" &&
            git archive --prefix=${ARCHIVE_NAME}/ HEAD --format=tar --output="${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar" &&
            tar -C "${CMAKE_BINARY_DIR}" --owner=root --group=root -r "${ARCHIVE_NAME}/ChangeLog" -f "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar" &&
            bzip2 -f9 "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar" &&
            echo "Source package created at ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2.\n"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set(PYSIDE2RCC_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/pyrcc/pyside2-rcc)
add_subdirectory(pyrcc)
add_subdirectory(pylupdate)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

find_package(Qt5Xml)

add_executable(pyside2-rcc main.cpp rcc.cpp)
include_directories(pyside2-rcc
                    ${Qt5Xml_INCLUDE_DIRS}
                    ${Qt5Core_INCLUDE_DIRS})

target_link_libraries(pyside2-rcc
                      ${Qt5Core_LIBRARIES}
                      ${Qt5Xml_LIBRARIES})

install(TARGETS pyside2-rcc RUNTIME DESTINATION bin)

# Man pages
if (NOT win32)
    file(GLOB manpages "${CMAKE_CURRENT_SOURCE_DIR}/*.1")
    install(FILES ${manpages} DESTINATION share/man/man1)
endif()

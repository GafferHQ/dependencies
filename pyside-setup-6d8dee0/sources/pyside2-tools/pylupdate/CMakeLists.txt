
set(lupdate_SRC
fetchtr.cpp
main.cpp
merge.cpp
metatranslator.cpp
numberh.cpp
proparser.cpp
sametexth.cpp
simtexth.cpp
translator.cpp
)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(Qt5Xml)
find_package(Qt5Widgets)

set(lupdate_MOC_HEADERS translator.h)
qt5_wrap_cpp(lupdate_MOC_OUTFILES ${lupdate_MOC_HEADERS})

add_executable(pyside2-lupdate ${lupdate_SRC} ${lupdate_MOC_OUTFILES})
include_directories(pyside2-lupdate
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${Qt5Xml_INCLUDE_DIRS}
                    ${Qt5Core_INCLUDE_DIRS}
                    ${Qt5Gui_INCLUDE_DIRS}
                    ${Qt5Widgets_INCLUDE_DIRS}
                    )

target_link_libraries(pyside2-lupdate
                      ${Qt5Core_LIBRARIES}
                      ${Qt5Xml_LIBRARIES}
                      ${Qt5Gui_LIBRARIES}
                      ${Qt5Widgets_LIBRARIES}
                      )

install(TARGETS pyside2-lupdate RUNTIME DESTINATION bin)

# Man pages
if (NOT win32)
    file(GLOB manpages "${CMAKE_CURRENT_SOURCE_DIR}/*.1")
    install(FILES ${manpages} DESTINATION share/man/man1)
endif()

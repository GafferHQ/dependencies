!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dinput

HEADERS += \
    gbuffer.h \
    deferredrenderer.h \
    finaleffect.h \
    sceneeffect.h \
    pointlightblock.h

SOURCES += \
    main.cpp \
    gbuffer.cpp \
    deferredrenderer.cpp \
    finaleffect.cpp \
    sceneeffect.cpp \
    pointlightblock.cpp

RESOURCES += \
    deferred-renderer-cpp.qrc

OTHER_FILES += \
    geometry_gl2.vert \
    geometry_gl2.frag \
    geometry_gl3.frag \
    geometry_gl3.vert \
    final_gl2.vert \
    final_gl2.frag \
    final_gl3.frag \
    final_gl3.vert

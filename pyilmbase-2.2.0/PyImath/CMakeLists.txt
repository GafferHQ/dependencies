
ADD_LIBRARY ( PyImath ${LIB_TYPE}
    PyImath.cpp
    PyImath.h
    PyImathAutovectorize.cpp
    PyImathAutovectorize.h
    PyImathBox.cpp
    PyImathBox.h
    PyImathBox2Array.cpp
    PyImathBox3Array.cpp
    PyImathBoxArrayImpl.h
    PyImathColor.h
    PyImathColor3.cpp
    PyImathColor3ArrayImpl.h
    PyImathColor4.cpp
    PyImathColor4Array2DImpl.h
    PyImathColor4ArrayImpl.h
    PyImathDecorators.h
    PyImathEuler.cpp
    PyImathEuler.h
    PyImathExport.h
    PyImathFixedArray.cpp
    PyImathFixedArray.h
    PyImathFixedArray2D.h
    PyImathFixedMatrix.h
    PyImathFixedVArray.cpp
    PyImathFixedVArray.h
    PyImathFrustum.cpp
    PyImathFrustum.h
    PyImathLine.cpp
    PyImathLine.h
    PyImathMathExc.h
    PyImathMatrix.h
    PyImathMatrix33.cpp
    PyImathMatrix44.cpp
    PyImathOperators.h
    PyImathPlane.cpp
    PyImathPlane.h
    PyImathQuat.cpp
    PyImathQuat.h
    PyImathRandom.cpp
    PyImathRandom.h
    PyImathShear.cpp
    PyImathShear.h
    PyImathStringArray.cpp
    PyImathStringArray.h
    PyImathStringArrayRegister.h
    PyImathStringTable.cpp
    PyImathStringTable.h
    PyImathTask.cpp
    PyImathTask.h
    PyImathUtil.cpp
    PyImathUtil.h
    PyImathVec.h
    PyImathVec2Impl.h
    PyImathVec2fd.cpp
    PyImathVec2si.cpp
    PyImathVec3ArrayImpl.h
    PyImathVec3Impl.h
    PyImathVec3fd.cpp
    PyImathVec3si.cpp
    PyImathVec3siArray.cpp
    PyImathVec4ArrayImpl.h
    PyImathVec4Impl.h
    PyImathVec4fd.cpp
    PyImathVec4si.cpp
    PyImathVec4siArray.cpp
    PyImathVecOperators.h
)

TARGET_LINK_LIBRARIES ( PyImath
	Iex${ILMBASE_LIBSUFFIX}
	IexMath${ILMBASE_LIBSUFFIX}
	Imath${ILMBASE_LIBSUFFIX}
	${Boost_LIBRARIES}
	${PYTHON_LIBRARIES}
)

INSTALL ( TARGETS PyImath
	DESTINATION
	lib
)

ADD_LIBRARY ( imathmodule MODULE
    imathmodule.cpp
    PyImathFun.cpp
    PyImathBasicTypes.cpp 
)

IF ( ${PLATFORM_WINDOWS} )
    SET_TARGET_PROPERTIES ( imathmodule
        PROPERTIES PREFIX "" SUFFIX ".pyd" OUTPUT_NAME imath BUILD_WITH_INSTALL_RPATH ON
    )
ELSE ()
    SET_TARGET_PROPERTIES ( imathmodule
        PROPERTIES PREFIX "" SUFFIX ".so" BUILD_WITH_INSTALL_RPATH ON
    )
ENDIF ()

TARGET_LINK_LIBRARIES ( imathmodule
	PyImath
	PyIex
	Imath${ILMBASE_LIBSUFFIX}
	Iex${ILMBASE_LIBSUFFIX}
	${Boost_LIBRARIES}
)

INSTALL ( TARGETS imathmodule
	DESTINATION lib/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/site-packages
)


{

	"downloads" : [

		"https://github.com/OpenImageIO/oiio/archive/Release-1.8.12.tar.gz"

	],

	"license" : "LICENSE",

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D USE_FFMPEG=NO"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",
		"cp {buildDir}/share/doc/OpenImageIO/openimageio.pdf {buildDir}/doc",
		
	],

	"platform:windows" : {

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild &&"
				" cmake"
			 	" -G {cmakeGenerator}"
			 	" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
			 	" -D CMAKE_PREFIX_PATH={buildDir}"
			 	" -D USE_FFMPEG=OFF"
			 	" -D USE_QT=OFF"
			 	" -D USEPYTHON=OFF"
			 	" -D BUILDSTATIC=OFF"
			 	" -D OPENEXR_INCLUDE_PATH={buildDir}\\include"
			 	" -D OPENEXR_IMATH_LIBRARY={buildDir}\\lib\\Imath-2_2.lib"
			 	" -D OPENEXR_ILMIMF_LIBRARY={buildDir}\\lib\\IlmImf-2_2.lib"
				" -D OPENEXR_IEX_LIBRARY={buildDir}\\lib\\Iex-2_2.lib"
				" -D OPENEXR_ILMTHREAD_LIBRARY={buildDir}\\lib\\IlmThread-2_2.lib"
				" -D ZLIB_INCLUDE_DIR={buildDir}\\include"
				" -D ZLIB_LIBRARY={buildDir}\\lib\\zlib.lib"
				" -D PNG_PNG_INCLUDE_DIR={buildDir}\\include"
				" -D PNG_LIBRARY={buildDir}\\lib\\libpng16.lib"
				" -D JPEG_INCLUDE_DIR={buildDir}\\include"
				" -D JPEG_LIBRARY={buildDir}\\lib\\jpeg.lib"
				" -D TIFF_INCLUDE_DIR={buildDir}\\include"
				" -D TIFF_LIBRARY={buildDir}\\lib\\libtiff.lib"
				" -D PYTHON_INCLUDE_DIR={buildDir}\\include\\python2.7"
				" -D PYTHON_LIBRARY={buildDir}\\lib\\python27.lib"
				" -D OCIO_LIBRARY_PATH={buildDir}\\lib\\OpenColorIO.lib"
			 	" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",

		]

	}

}

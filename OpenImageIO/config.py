{

	"downloads" : [

		"https://github.com/OpenImageIO/oiio/archive/refs/tags/v2.5.10.1.tar.gz"

	],

	"url" : "http://www.openimageio.org",

	"license" : "LICENSE.md",

	"dependencies" : [ "Boost", "Python", "PyBind11", "OpenEXR", "LibTIFF", "LibPNG", "OpenJPEG", "LibJPEG-Turbo", "OpenColorIO", "LibRaw", "PugiXML", "Fmt", "LibWebP", "TBB", "FreeType" ],

	"environment" : {

		"PATH" : "{buildDir}/bin:$PATH",
		"LD_LIBRARY_PATH" : "{buildDir}/lib:$LD_LIBRARY_PATH",

	},

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D USE_FFMPEG=NO"
			" -D USE_GIF=0"
			" -D USE_OPENVDB=NO"
			" -D USE_PYTHON=YES"
			" -D USE_EXTERNAL_PUGIXML=YES"
			" -D BUILD_MISSING_FMT=NO"
			" -D OIIO_BUILD_TESTS=NO"
			" -D OIIO_DOWNLOAD_MISSING_TESTDATA=NO"
			" -D PYTHON_VERSION={pythonMajorVersion}"
			" -D Python_ROOT_DIR={buildDir}"
			" -D Python_FIND_STRATEGY=LOCATION"
			# These next two disable `iv`. This fails to
			# build on Mac due to OpenGL deprecations, and
			# we've never packaged it anyway.
			" -D USE_OPENGL=NO"
			" -D USE_QT=NO"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",
		"{extraCommands}",

	],

	"variables" : {

		"extraCommands" : "",

	},

	"manifest" : [

		"bin/maketx{executableExtension}",
		"bin/oiiotool{executableExtension}",

		"include/OpenImageIO",
		"{sharedLibraryDir}/{libraryPrefix}OpenImageIO*{sharedLibraryExtension}*",
		"lib/{libraryPrefix}OpenImageIO*.lib",
        "python/OpenImageIO",

		"doc/openimageio.pdf",

	],

	"platform:windows" : {

		"environment" : {

			"PATH" : "{buildDir}/bin;{buildDir}/lib;%PATH%",

		},

		"commands" : [

			"mkdir gafferBuild",
			"cd gafferBuild &&"
				" cmake"
			 	" -G {cmakeGenerator}"
				" -D CMAKE_CXX_STANDARD={c++Standard}"
			 	" -D CMAKE_BUILD_TYPE={cmakeBuildType}"
			 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
			 	" -D CMAKE_PREFIX_PATH={buildDir}"
			 	" -D USE_FFMPEG=NO"
			 	" -D USE_QT=NO"
			 	" -D USE_PYTHON=YES"
			 	" -D BUILDSTATIC=NO"
				" -D BOOST_ROOT={buildDir}"
				" -D OIIO_BUILD_TESTS=NO"
				" -D OIIO_DOWNLOAD_MISSING_TESTDATA=NO"
			 	" -D OPENEXR_INCLUDE_PATH={buildDir}/include"
			 	" -D OPENEXR_IMATH_LIBRARY={buildDir}/lib/Imath.lib"
			 	" -D OPENEXR_ILMIMF_LIBRARY={buildDir}/lib/IlmImf.lib"
				" -D OPENEXR_IEX_LIBRARY={buildDir}/lib/Iex.lib"
				" -D OPENEXR_ILMTHREAD_LIBRARY={buildDir}/lib/IlmThread.lib"
				" -D ZLIB_INCLUDE_DIR={buildDir}/include"
				" -D ZLIB_LIBRARY={buildDir}/lib/zlib.lib"
				" -D PNG_PNG_INCLUDE_DIR={buildDir}/include"
				" -D PNG_LIBRARY={buildDir}/lib/libpng16.lib"
				" -D JPEG_INCLUDE_DIR={buildDir}/include"
				" -D JPEG_LIBRARY={buildDir}/lib/jpeg.lib"
				" -D TIFF_INCLUDE_DIR={buildDir}/include"
				" -D TIFF_LIBRARY={buildDir}/lib/libtiff.lib"
				" -D PYTHON_VERSION={pythonMajorVersion}"
				" -D Python_ROOT_DIR={buildDir}"
				" -D Python_FIND_STRATEGY=LOCATION"
				" -D OCIO_LIBRARY_PATH={buildDir}/lib/OpenColorIO.lib"
				" -D USE_SIMD=sse4.2"
			 	" ..",
			"cd gafferBuild && cmake --build . --config {cmakeBuildType} --target install -- -j {jobs}",
		],

		"postMovePaths" : {

			"{buildDir}/lib/python{pythonVersion}/site-packages/OpenImageIO" : "{buildDir}/python",

		}

	},

	"platform:macos" : {

		"variables" : {

			"extraCommands" : "mv {buildDir}/lib/python{pythonVersion}/site-packages/OpenImageIO {pythonLibDir}/python{pythonVersion}/site-packages/OpenImageIO"

		},

	},

}

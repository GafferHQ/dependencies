{

	"downloads" : [

		"https://github.com/OpenImageIO/oiio/archive/Release-2.2.11.1.tar.gz"

	],

	"url" : "http://www.openimageio.org",

	"license" : "LICENSE.md",

	"dependencies" : [ "Boost", "Python", "PyBind11", "OpenEXR", "LibTIFF", "LibPNG", "LibJPEG-Turbo", "OpenColorIO", "LibRaw", "PugiXML" ],

	"commands" : [

		"mkdir gafferBuild",
		"cd gafferBuild &&"
			" cmake"
			" -D CMAKE_CXX_STANDARD={c++Standard}"
			" -D CMAKE_INSTALL_PREFIX={buildDir}"
			" -D CMAKE_INSTALL_LIBDIR={buildDir}/lib"
			" -D CMAKE_PREFIX_PATH={buildDir}"
			" -D USE_FFMPEG=NO"
			" -D USE_PYTHON=YES"
			" -D USE_EXTERNAL_PUGIXML=YES"
			" -D OIIO_BUILD_TESTS=NO"
			# These next two disable `iv`. This fails to
			# build on Mac due to OpenGL deprecations, and
			# we've never packaged it anyway.
			" -D USE_OPENGL=NO"
			" -D USE_QT=NO"
			" ..",
		"cd gafferBuild && make install -j {jobs} VERBOSE=1",

	],

	"manifest" : [

		"bin/maketx",
		"bin/oiiotool",

		"include/OpenImageIO",
		"lib/libOpenImageIO*{sharedLibraryExtension}*",

		"doc/openimageio.pdf",

	],

}

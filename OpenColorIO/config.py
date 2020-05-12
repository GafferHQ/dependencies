{

	"downloads" : [

		"https://github.com/imageworks/OpenColorIO/archive/v1.0.9.tar.gz",
		"https://github.com/imageworks/OpenColorIO-Configs/archive/v1.0_r2.tar.gz",

	],

	"url" : "http://opencolorio.org",

	"license" : "LICENSE",

	"dependencies" : [ "Python" ],

	"environment" : {

		"LD_LIBRARY_PATH" : "{buildDir}/lib",

	},

	"commands" : [

		"cmake"
		 	" -D CMAKE_INSTALL_PREFIX={buildDir}"
		 	" -D PYTHON={buildDir}/bin/python"
		 	# By default, OCIO will use tr1::shared_ptr. But Maya (2015 and 2016 at least)
			# ships with a libOpenColorIO built using boost::shared_ptr instead. We'd like
			# Gaffer's default packages to be useable in Maya, so we pass OCIO_USE_BOOST_PTR=1
			# to match Maya's build. Even though both Gaffer and Maya respect the VFXPlatform
			# 2016 by using OCIO 1.0.9, this is an example of where the platform is under
			# specified, and we must go the extra mile to get compatibility.
			" -D OCIO_USE_BOOST_PTR=1"
			" -D OCIO_BUILD_TRUELIGHT=OFF"
			" -D OCIO_BUILD_APPS=OFF"
			" -D OCIO_BUILD_NUKE=OFF"
			" .",

		"make clean && make -j {jobs} && make install",

		"mkdir -p {buildDir}/python",
		"mv {buildDir}/lib/python*/site-packages/PyOpenColorIO* {buildDir}/python",

		"mkdir -p {buildDir}/openColorIO",
		"cp ../OpenColorIO-Configs-1.0_r2/nuke-default/config.ocio {buildDir}/openColorIO",
		"cp -r ../OpenColorIO-Configs-1.0_r2/nuke-default/luts {buildDir}/openColorIO",

	],

	"manifest" : [

		"include/OpenColorIO",
		"lib/libOpenColorIO*{sharedLibraryExtension}*",
		"openColorIO",
		"python/PyOpenColorIO*",

	],

}

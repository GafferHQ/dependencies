{

	"downloads" : [

		"https://github.com/imageworks/OpenColorIO/archive/v1.0.9.tar.gz",
		"https://github.com/imageworks/OpenColorIO-Configs/archive/v1.0_r2.tar.gz",

	],

	"license" : "LICENSE",

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

	"platform:windows" : {

		"variables" : {

			"cmakeBuildType" : "Release",
			"cmakeGenerator": "\"Visual Studio 15 2017 Win64\"",

		},

		"environment" : {

			"PATH" : "%PATH%;%ROOT_DIR%\\winbuild\\patch\\bin",

		},

		"commands" : [

			#OCIO is particular about needing all slashes to be forward slashes
			"cmake"
				" -Wno-dev -G {cmakeGenerator}"
				" -DCMAKE_BUILD_TYPE={cmakeBuildType}"
				" -DCMAKE_INSTALL_PREFIX={buildDir}"
				" -DPYTHON={buildDir}\\bin\\python.exe"
				" -DOCIO_USE_BOOST_PTR=1"
				" -DOCIO_BUILD_TRUELIGHT=OFF"
				" -DOCIO_BUILD_APPS=OFF"
				" -DOCIO_BUILD_NUKE=OFF"
				" -DOCIO_BUILD_STATIC=OFF"
				" -DOCIO_BUILD_SHARED=ON"
				" -DOCIO_BUILD_PYGLUE=ON"
				" -DPYTHON_VERSION=2.7"
				" -DPYTHON_INCLUDE={buildDir}\\include"
				" -DPYTHON_LIB={buildDir}\\lib"
				" -DOCIO_PYGLUE_LINK=ON"
				" .",
			"cmake --build . --config {cmakeBuildType} --target install",
			"if not exist \"{buildDir}\\python\" mkdir {buildDir}\\python",
			"move {buildDir}\\PyOpenColorIO.dll {buildDir}\\python\\PyOpenColorIO.pyd",
			"if not exist \"{buildDir}\\openColorIO\" mkdir {buildDir}\\openColorIO",
			"if not exist \"{buildDir}\\openColorIO\\luts\" mkdir {buildDir}\\openColorIO\\luts",
			"copy ..\\OpenColorIO-Configs-1.0_r2\\nuke-default\\config.ocio {buildDir}\\openColorIO",
			"xcopy /s /e /h /y /i ..\\OpenColorIO-Configs-1.0_r2\\nuke-default\\luts {buildDir}\\openColorIO\\luts",
		],

	},
}

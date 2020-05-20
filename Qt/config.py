{

	"downloads" : [

		"https://download.qt.io/official_releases/qt/6.5/6.5.6/src/single/qt-everywhere-opensource-src-6.5.6.tar.xz"

	],

	"dependencies" : [ "LibPNG", "LibTIFF", "LibJPEG-Turbo", "FreeType", "LLVM" ],

	"url" : "https://www.qt.io",

	"license" : "LICENSE.LGPL3",

	"environment" : {

		"PKG_CONFIG_PATH" : "{buildDir}/lib/pkgconfig",

	},

	"commands" : [

		"./configure"
			" -prefix {buildDir} "
			" -plugindir {buildDir}/qt/plugins"
			" -release"
			" -opensource -confirm-license"
			" -no-rpath"
			" -no-dbus"
			" -no-egl"
			" -no-gstreamer"
			" -skip qtconnectivity"
			" -skip qtwebengine"
			" -skip qt3d"
			" -skip qtdeclarative"
			" -skip qtwebchannel"
			" -skip qtnetworkauth"
			" -skip qtwayland"
			" -skip qtdoc"
			" -skip qthttpserver"
			" -skip qtlocation"
			" -skip qtlottie"
			" -skip qtmqtt"
			" -skip qtopcua"
			" -skip qtquick3d"
			" -skip qtquick3dphysics"
			" -skip qtquickeffectmaker"
			" -skip qtquicktimeline"
			" -skip qtvirtualkeyboard"
			" -skip qtwebsockets"
			" -skip qtwebview"
			" -no-libudev"
			" -no-icu"
			" -qt-pcre"
			" -qt-harfbuzz"
			" -nomake examples"
			" -nomake tests"
			" {extraArgs}"
			" -I {buildDir}/include -I {buildDir}/include/freetype2"
			" -L {buildDir}/lib"
		,

		"cmake --build . --parallel {jobs} && cmake --install .",

		"cp {buildDir}/libexec/moc {buildDir}/bin",
		"cp {buildDir}/libexec/rcc {buildDir}/bin",
		"cp {buildDir}/libexec/uic {buildDir}/bin",

	],

	"manifest" : [

		"bin/moc{executableExtension}",
		"bin/qmake{executableExtension}",
		"bin/rcc{executableExtension}",
		"bin/uic{executableExtension}",

		"include/Qt*",

		"{sharedLibraryDir}/{libraryPrefix}Qt*{sharedLibraryExtension}",
		"lib/{libraryPrefix}Qt*",
		"lib/Qt*.framework",

		"mkspecs",
		"qt",
		"lib/cmake",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib",
			"LLVM_INSTALL_DIR" : "{buildDir}",

		},

		"variables" : {

			"extraArgs" : "-xcb -bundled-xcb-xinput",

		},

	},

	"platform:macos" : {

		"variables" : {

			"extraArgs" : "-no-freetype QMAKE_APPLE_DEVICE_ARCHS=arm64",

		},

	},

	"platform:windows" : {

		# "environment" : {

		# 	"PATH" : "%ROOT_DIR%\\Qt\\working\\qt-everywhere-src-6.5.6\\qtbase\\lib;{buildDir}\\lib;{buildDir}\\bin;%PATH%",

		# },

		"commands" : [

			lambda c : shutil.copy( c["variables"]["buildDir"] + "/lib/zlib.lib", c["variables"]["buildDir"] + "/lib/zdll.lib" ),
			lambda c : shutil.copy( c["variables"]["buildDir"] + "/lib/libpng16.lib", c["variables"]["buildDir"] + "/lib/libpng.lib" ),
			lambda c : shutil.copy( c["variables"]["buildDir"] + "/lib/jpeg.lib", c["variables"]["buildDir"] + "/lib/libjpeg.lib" ),
			# help Qt find the right zlib.dll
			lambda c : shutil.copy( c["variables"]["buildDir"] + "/bin/zlib.dll", os.environ["ROOT_DIR"] + "/Qt/working/qt-everywhere-src-6.5.6/qtbase/bin/zlib.dll" ),
			"call configure.bat"
				" -prefix {buildDir}"
				" -cmake-generator Ninja"
				" -plugindir {buildDir}/qt/plugins"
				" -release"
				" -opensource"
				" -confirm-license"
				" -opengl desktop"
				" -no-rpath"
				" -no-dbus"
				" -skip qt3d"
				" -skip qtcharts"
				" -skip qtconnectivity"
				" -skip qtdatavis3d"
				" -skip qtdeclarative"
				" -skip qtgamepad"
				" -skip qtnetworkauth"
				" -skip qtremoteobjects"
				" -skip qtsensors"
				" -skip qtserialbus"
				" -skip qtserialport"
				" -skip qtspeech"
				" -skip qtwebchannel"
				" -skip qtwebengine"
				" -skip qtdoc"
				" -skip qthttpserver"
				" -skip qtlocation"
				" -skip qtlottie"
				" -skip qtmqtt"
				" -skip qtopcua"
				" -skip qtquick3d"
				" -skip qtquick3dphysics"
				" -skip qtquickeffectmaker"
				" -skip qtquicktimeline"
				" -skip qtvirtualkeyboard"
				" -skip qtwebsockets"
				" -skip qtwebview"
				" -no-libudev"
				" -no-icu"
				" -qt-pcre"
				" -nomake examples"
				" -nomake tests"
				" -system-zlib"
				" -I {buildDir}/include"
				" -L {buildDir}/lib",
			"cmake --build . --parallel",
			"cmake --install ."

		]
	}

}

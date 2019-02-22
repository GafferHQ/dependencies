{

	"downloads" : [

		"https://damassets.autodesk.net/content/dam/autodesk/www/Company/files/2018/Qt561ForMaya2018Update4.zip"

	],

	"workingDir" : "qt-adsk-5.6.1-vfx",

	"license" : "LICENSE.LGPLv21",

	"commands" : [

		"tar -xf ../qt561-webkit.tgz",

		"./configure"
			" -prefix {buildDir}"
			" -plugindir {buildDir}/qt/plugins"
			" -release"
			" -opensource -confirm-license"
			" -no-rpath -no-gtkstyle"
			" -no-audio-backend -no-dbus"
			" -skip qtconnectivity"
			" -skip qtwebengine"
			" -skip qt3d"
			" -skip qtdeclarative"
			" -no-libudev"
			" -no-gstreamer"
			" -no-icu"
			" -qt-pcre"
			" -nomake examples"
			" -nomake tests"
			" {extraArgs}"
			" -I {buildDir}/include -I {buildDir}/include/freetype2"
			" -L {buildDir}/lib"
			" -c++std c++11"
		,

		"make -j {jobs}",
		"make install",

	],

	"platform:linux" : {

		"environment" : {

			"LD_LIBRARY_PATH" : "{buildDir}/lib",

		},

		"variables" : {

			"extraArgs" : "-qt-xcb",

		},

	},

	"platform:osx" : {

		"variables" : {

			"extraArgs" : "-no-freetype -platform macx-clang",

		},

	},

	"platform:windows" : {
		
		"environment" : {

			"PATH" : "{buildDir}\\lib;{buildDir}\\bin",

		},

		"commands" : [

			"cmake -E tar -xvf ../qt561-webkit.tgz",
			
			"copy {buildDir}\\lib\\zlib.lib {buildDir}\\lib\\zdll.lib"
			"copy {buildDir}\\lib\\libpng16.lib {buildDir}\\lib\\libpng.lib"
			"copy {buildDir}\\lib\\jpeg.lib {buildDir}\\lib\\libjpeg.lib"

			"call configure.bat"
				" -prefix {buildDir}"
				" -plugindir {buildDir}\\qt\\plugins"
				" -release"
				" -opensource -confirm-license"
				" -opengl desktop -no-angle"
				" -no-audio-backend -no-dbus"
				" -skip qtconnectivity"
				" -skip qtwebengine"
				" -skip qt3d"
				" -skip qtdeclarative"
				" -skip qtwebkit"
				" -nomake examples"
				" -nomake tests"
				" -system-zlib"
				" -no-openssl"
				" {extraArgs}"
				" -I {buildDir}\\include -I {buildDir}\\include\\freetype2"
				" -L {buildDir}\\lib"
				" -c++std c++11"
			,

			"{buildDir}\winbuild\jom\jom.exe",
			"{buildDir}\winbuild\jom\jom.exe install",

		],

	},

}

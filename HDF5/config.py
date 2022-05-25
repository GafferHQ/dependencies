{

	"downloads" : [

		"https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.12/hdf5-1.12.0/src/hdf5-1.12.0.tar.gz"

	],

	"url" : "http://www.hdfgroup.org",

	"license" : "COPYING",

	"commands" : [

		"./configure --prefix={buildDir} --enable-threadsafe --disable-hl --with-pthread=/usr/include",

		"make -j {jobs}",
		"make install",

	],

	"manifest" : [

		"lib/libhdf5*{sharedLibraryExtension}*",

	],

}

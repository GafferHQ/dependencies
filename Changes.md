10.0.0 alpha x (relative to 10.0.0 alpha 4)
--------------

- Partio : Removed.

10.0.0 alpha 4 (relative to 10.0.0 alpha 3)
--------------

- Cortex : Updated to version 10.5.15.0.

10.0.0 alpha 3 (relative to 10.0.0 alpha 2)
--------------

- PySide : Updated to version 6.5.6.
- Qt : Updated to version 6.5.6.
- Qt.py : Updated to version 1.4.6.

10.0.0 alpha 2 (relative to 10.0.0 alpha 1)
--------------

- Cycles : Updated to version 4.4.0.
- FreeType : Updated to version 2.13.3.
- LibRaw : Updated to version 0.21.4.
- MaterialX : Updated to version 1.39.3.
- OpenImageIO : Updated to version 3.0.6.1.
- OpenShadingLanguage : Updated to version 1.14.5.1.

10.0.0 alpha 1 (relative to 9.1.0)
--------------

- Boost : Updated to version 1.82.0.
- Cortex : Updated to version 10.5.14.1.
- Minizip : Updated to version 3.0.10.
- OpenColorIO : Updated to version 2.3.2.
- OpenEXR : Updated to version 3.2.4.
- OpenSubdiv : Updated to version 3.6.0.
- OpenVDB : Updated to version 11.0.0.
- Python : Updated to version 3.11.12.
- PySide : Updated to version 5.15.16.
- Qt : Updated to version 5.15.16.
- USD : Updated to version 25.05.01.

9.1.0 (relative to 9.0.0)
-----

- Cortex : Updated to version 10.5.11.0.

9.0.0 (relative to 8.0.1)
-----

- CI : Added CI for linux-gcc11 and macos-arm64 (Xcode 14.3.1).
- Cortex : Updated to version 10.5.10.0.
- Cycles :
  - Updated to version 4.2.0.
  - Disabled CUDA binary generation for Kepler and Maxwell architecture GPUs.
- Embree : Updated to version 4.3.2.
- Imath : Updated to version 3.1.11.
- LibJPEG-Turbo : Updated to version 3.0.3.
- LLVM : Updated to version 15.0.7.
- MaterialX : Updated to version 1.38.10.
- OpenImageIO : Updated to version 2.5.10.1.
- OpenPGL : Updated to version 0.6.0.
- OpenShadingLanguage :
  - Updated to version 1.13.11.0.
  - Enabled Optix support on Linux.
- PySide : Updated to version 5.15.14.
- Qt : Updated to version 5.15.14.
- USD : Updated to version 24.08.
- Zstandard : Added version 1.5.0.

8.x.x (relative to 8.0.1)
-----



8.0.1 (relative to 8.0.0)
-----

- Cortex : Updated to version 10.5.7.1.
- Jemalloc : Enabled `tls_model("initial-exec")` to prevent infinite recursion with dynamic TLS on Linux distributions with modern glibc versions.

8.0.0 (relative to 7.0.0)
-----

- Cortex : Updated to version 10.5.7.0.
- Cycles :
	- Updated to version 4.0.2.
	- Patched shader `IOR` values to default to 1.5.
	- Enabled CUDA and Optix devices.
- Embree : Updated to version 4.3.0.
- Imath : Updated to version 3.1.9.
- Jinja2 : Added version 3.1.3.
- LibWebP : Added version 1.3.2.
- MarkupSafe : Added version 2.1.5.
- MaterialX : Updated to version 1.38.8.
- OpenEXR : Updated to version 3.1.13.
- OpenImageIO : Updated to version 2.5.8.0.
- OpenPGL : Updated to version 0.5.0.
- OpenShadingLanguage : Updated to version 1.12.14.0.
- OpenSSL : Removed.
- OpenSubdiv : Updated to version 3.5.1.
- OpenVDB : Updated to version 10.1.0.
- PsUtil : Added version 5.9.6.
- PySide : Updated to version 5.15.12.
- Python : Updated to version 3.10.13.
- Qt :
	- Updated to version 5.15.12.
	- Removed QtPurchasing library.
	- Removed QtNetworkAuth library.
- USD :
	- Updated to version 23.11.
	- Added `sdrOsl`, for inclusion of OSL shaders in the Sdr Registry.
	- Added `usdGenSchema`.
	- Disabled Embree Hydra delegate, since it is incompatible with Embree 4.

7.x.x (relative to 7.0.0)
-----

- MaterialX : Fixed linking on MacOS.

7.0.0 (relative to 6.0.0)
-----

- Appleseed : Removed.
- Boost : Updated to version 1.80.0.
- Cortex : Updated to version 10.5.0.0.
- Cycles : Updated to version 3.5.0.
- Imath : Added version 3.1.7.
- MaterialX : Added version 1.38.4.
- Minizip : Added version 3.0.9.
- OpenColorIO : Updated to version 2.2.1.
- OpenEXR : Updated to version 3.1.7.
- OpenImageIO : Updated to version 2.4.11.0.
- OpenPGL : Added version 0.4.1.
- Partio : Added version 1.14.6.
- PCG : Added "version" 428802d.
- PyBind11 : Updated to version 2.10.4.
- PySide : Updated to version 5.15.8.
- Qt : Updated to version 5.15.8.
- Subprocess32 : Removed.
- Six : Removed.
- USD : Updated to version 23.05.
- Xerces : Removed.
- ZLib : Added version 1.2.13.

6.0.0 (relative to 5.1.0)
-----

- Cortex : Updated to version 10.4.5.0.
- Cycles : Updated to version 3.4.
- Embree : Updated to version 3.13.4.
- USD :
	- Updated to version 23.02.
	- Enabled the OpenImageIO plugin. Among other things, this allows OpenEXR textures to be shown in `usdview`.
- OpenColorIO : Updated to version 2.1.2.
- OpenImageIO : Updated to version 2.4.8.0.
- OpenShadingLanguage : Updated to version 1.12.9.0.
- OpenSubdiv : Updated to version 3.4.4.
- Expat : Added version 2.5.0.
- PyString : Added version 1.1.4.
- YAML-CPP : Added version 0.7.0.
- Fmt : Added version 9.1.0.
- Python : Removed Python 2 variant.

5.1.0 (relative to 5.0.0)
-----

- Cortex : Updated to 10.4.1.0.
- Qt : Reintroduced QtUiTools module (missing in 5.0.0).

5.0.0 (relative to 4.0.0)
-----

- C++ : Updated to c++17.
- Alembic : Updated to version 1.8.3.
- Blosc : Updated to 1.21.1.
- Boost : Updated to version 1.76.0.
- Cycles : Added version 3.1.1.
- Cortex : Updated to 10.4.0.0.
- CMark : Updated to 0.29.0.
- HDF5 : Updated to 1.12.0.
- LibFFI : Updated to 3.4.2.
- LibPNG : Updated to 1.6.37.
- LLVM : Upated to version 11.1.0.
- OpenColorIO : Updated to version 2.1.1.
- OpenImageIO : Updated to version 2.3.11.0.
- OpenJPEG : Added version 2.4.0.
- OpenShadingLanguage : Updated to version 1.11.17.0.
- OpenSSL : Updated to 1.1.1i.
- OpenVDB : Updated to version 9.1.0, and added `nanovdb`.
- PySide : Updated to 5.15.4.
- Python : Updated to 3.8.13 (MacOS only).
- Qt : Updated to 5.15.4.
- Subprocess32 : Changed to regular install rather than `.egg`.
- TBB : Updated to version 2020.3.
- USD : Updated to version 21.11.

4.0.0
-----

- OpenVDB : Updated to version 8.1.0.
- TBB : Updated to version 2020.2.
- Qt : Updated to version 5.15.2.
- PySide : Updated to version 5.15.2.
- Cortex : Updated to version 10.3.0.0.
- Embree : Added version 3.13.3.

3.1.0
-----

- Python : Fixed `ssl` module for Python 3.
- USD : Enabled OpenVDB support.
- Cortex : updated to version 10.2.2.0.

3.0.0
-----

- OpenShadingLanguage : Updated to version 1.11.14.1.
- OpenImageIO : Updated to version 2.2.15.1.
- Cortex : Updated to version 10.2.0.0.
- USD : Updated to version 21.05.
- OpenVDB : Updated to version 7.2.2.
- PyBind11 : Added version 2.6.2.
- PugiXML : Added version 1.11.
- LibTIFF : Updated to version 4.1.0.
- LLVM : Updated to version 10.0.1.

2.2.0
-----

- Qt : Updated to version 5.12.10.
- Cortex : Updated to version 10.1.4.0.

2.1.1
-----

- OpenSSL : Fixed packaging of missing libraries.

2.1.0
-----

- Cortex : Updated to version 10.1.2.0.
- LibFFI : Fixed compatibility with pre-haswell era processors.
- OpenSSL : Updated to version 1.1.1h.

2.0.0
-----

This major version introduces the concept of build variants, and provides package variants for both Python 2 and 3.

- Qt : Updated to version 5.12.8.
- PySide : Updated to version 5.12.6.
- Qt.py : Updated to version 1.2.5.
- Boost : Updated to version 1.68.0.
- OpenEXR : Updated to version 2.4.1.
- Python : Added version 3.7.6.
- Six : Added version 1.14.0.
- Subprocess32 : Updated to version 3.5.4.
- Appleseed : Updated to version 2.1.0-beta.
- LZ4 : Added version 1.9.2.
- Cortex : Updated to version 10.1.0.0.
- USD : Updated to version 20.11 and added usdImaging.
- LibRaw : Added version 0.19.5.
- OpenSubdiv : Added version 3.4.3.
- LibFFI : Added version 3.3.
- Jemalloc : Added version 3.6.0.
- Build : Switched standard to C++14.

1.2.0
-----

- Cortex : Updated to version 10.0.0-a76.

1.1.0
-----

- Cortex : Updated to version 10.0.0-a74.

1.0.0
-----

Moved to Semantic Versioning to describe changes in dependencies, and decouple from GAFFER_VERSION.

- Cortex : Updated to version 10.0.0-a72.
- USD : Updated to version 20.02.
- VDB : Updated to version 7.0.0.

0.54.2.0
--------

- Cortex :
	- Updated to version 10.0.0-a64.
	- Fixed extension for RenderMan display driver on Linux.
- GafferResources :
	- Update to version 0.54.2.0.

0.54.1.0
--------

- Cortex :
	- Updated to version 10.0.0-a60.
	- Added display driver for RenderMan.

0.54.0.0
--------

- OpenEXR : Updated to version 2.3.0.
- OpenVDB : Updated to version 6.0.0.
- Blosc : Updated to version 1.15.1.
- PySide : Updated to version corresponding with Maya 2018 Update 6.
- Cortex : Updated to version 10.0.0-a59.
- USD : Enable support for loading Alembic archives.
- Appleseed : Updated to version 2.0.5-beta.

0.54.0.0-rc3
------------

- Cortex : Updated to version 10.0.0-a54.

0.54.0.0-rc2
------------

- Cortex : Updated to version 10.0.0-a53.

0.54.0.0-rc1
------------

- Cortex : Updated to version 10.0.0-a52.
- TBB : Updated to version 2018 Update 5.
- Alembic : Added python bindings.

0.53.0.0
--------

- Cortex : Updated to version 10.0.0-a45.
- Qt : Updated to version corresponding to Maya 2018 Update 4.

0.52.0.0
--------

- Cortex : Updated to version 10.0.0-a38.
- USD : Fixed python module linking on OSX.

0.51.0.0
--------

- Cortex : Updated to version 10.0.0-a35.
- USD : Updated to version 18.09.
- Xerces : Updated to version 3.2.2.
- OpenImageIO : Added documentation to package.
- OpenShadingLanguage : Fixed documentation installation.

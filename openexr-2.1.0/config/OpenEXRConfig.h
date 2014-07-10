/* config/OpenEXRConfig.h.  Generated from OpenEXRConfig.h.in by configure.  */
//
// Define and set to 1 if the target system supports a proc filesystem
// compatible with the Linux kernel's proc filesystem.  Note that this
// is only used by a program in the IlmImfTest test suite, it's not
// used by any OpenEXR library or application code.
//

#define OPENEXR_IMF_HAVE_LINUX_PROCFS 1

//
// Define and set to 1 if the target system is a Darwin-based system
// (e.g., OS X).
//

/* #undef OPENEXR_IMF_HAVE_DARWIN */

//
// Define and set to 1 if the target system has a complete <iomanip>
// implementation, specifically if it supports the std::right
// formatter.
//

#define OPENEXR_IMF_HAVE_COMPLETE_IOMANIP 1

//
// Define and set to 1 if the target system has support for large
// stack sizes.
//

#define OPENEXR_IMF_HAVE_LARGE_STACK 1

//
// Current internal library namepace name
//
#define OPENEXR_IMF_INTERNAL_NAMESPACE_CUSTOM 1
#define OPENEXR_IMF_INTERNAL_NAMESPACE cus

//
// Current public user namepace name
//

#define OPENEXR_IMF_NAMESPACE_CUSTOM 1
#define OPENEXR_IMF_NAMESPACE bar

//
// Version string for runtime access
//

#define OPENEXR_VERSION_STRING "2.1.0"
#define OPENEXR_PACKAGE_STRING "OpenEXR 2.1.0"

#define OPENEXR_VERSION_MAJOR 2
#define OPENEXR_VERSION_MINOR 1
#define OPENEXR_VERSION_PATCH 0

// Version as a single hex number, e.g. 0x01000300 == 1.0.3
#define OPENEXR_VERSION_HEX ((OPENEXR_VERSION_MAJOR << 24) | \
                             (OPENEXR_VERSION_MINOR << 16) | \
                             (OPENEXR_VERSION_PATCH <<  8))


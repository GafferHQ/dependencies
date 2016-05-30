lib_LTLIBRARIES = libsvg.la
include_HEADERS = svg.h

if EXPAT
LIBSVG_EXTRA_SOURCES = svg_parser_expat.c svg_hash.c svg_hash.h
else
LIBSVG_EXTRA_SOURCES = svg_parser_libxml.c
endif

libsvg_la_SOURCES = \
	svg.c \
	svg.h \
	svgint.h \
	svg_ascii.h \
	svg_ascii.c \
	svg_attribute.c \
	svg_color.c \
	svg_element.c \
	svg_gradient.c \
	svg_group.c \
	svg_image.c \
	svg_length.c \
	svg_paint.c \
	svg_parser.c \
	svg_pattern.c \
	svg_path.c \
	svg_str.c \
	svg_style.c \
	svg_text.c \
	svg_transform.c \
	svg_version.h \
	$(LIBSVG_EXTRA_SOURCES)

libsvg_la_LDFLAGS = -version-info @VERSION_INFO@

INCLUDES = $(LIBSVG_CFLAGS) $(WARN_CFLAGS)

libsvg_la_LIBADD = $(LIBSVG_LIBS)


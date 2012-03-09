INCLUDES = \
	-I$(top_srcdir)

AM_CXXFLAGS = \
	$(COGL_DEP_CFLAGS) \
	$(COGL_EXTRA_CFLAGS) \
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API

noinst_PROGRAMS = cogl-v8

cogl_v8_SOURCES = \
	cogl-v8.cc \
	cogl-v8-pipeline.h \
	cogl-v8-pipeline.cc \
	cogl-v8-color.h \
	cogl-v8-color.cc \
	cogl-v8-primitive.h \
	cogl-v8-primitive.cc \
	$(NULL)
cogl_v8_CPPFLAGS=@V8_CPPFLAGS@
cogl_v8_LDADD = \
	$(COGL_DEP_LIBS) \
	$(top_builddir)/cogl/libcogl.la \
	@V8_LDFLAGS@

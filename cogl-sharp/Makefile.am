V_MCS = $(v_MCS_$(V))
v_MCS_ = $(v_MCS_$(AM_DEFAULT_VERBOSITY))
v_MCS_0 = @echo "  MCS   " $@;

assemblydir = $(libdir)/cogl
assembly_DATA = cogl2-sharp.dll cogl2-sharp.dll.config cogl2-sharp.dll.mdb

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cogl2-sharp.pc

sources =			\
	BufferBit.cs		\
	Color.cs		\
	ColorMask.cs		\
	Context.cs		\
	Driver.cs		\
	Exception.cs		\
	FrameBuffer.cs		\
	_FrameBuffer.cs		\
	Marshaller.cs		\
	Matrix.cs		\
	Object.cs		\
	OnScreen.cs		\
	_OnScreen.cs		\
	Pipeline.cs		\
	_Pipeline.cs		\
	PipelineAlphaFunc.cs	\
	PipelineCullFaceMode.cs	\
	PipelineFilter.cs	\
	PipelineWrapMode.cs	\
	PixelFormat.cs		\
	Quaternion.cs		\
	RendererConstraint.cs	\
	Texture.cs		\
	_Texture.cs		\
	TextureFlags.cs		\
	TextureType.cs		\
	VerticesMode.cs		\
	Winding.cs		\
	AssemblyInfo.cs		\
	$(NULL)

cogl2-sharp.dll: $(sources)
	@rm -f cogl2-sharp.dll.mdb
	$(V_MCS)$(MCS) $(CSFLAGS) @$(srcdir)/cogl2-sharp.rsp -debug -unsafe $(sources)

cogl2-sharp.dll.mdb: cogl2-sharp.dll

EXTRA_DIST =			\
	$(sources)		\
	cogl2-sharp.pc.in	\
	$(NULL)

CLEANFILES =			\
	cogl2-sharp.dll		\
	cogl2-sharp.dll.mdb	\
	$(NULL)

DISTCLEANFILES =		\
	AssemblyInfo.cs		\
	$(NULL)

if BUILD_NUNIT_TESTS
noinst_DATA = CoglTests.dll

TESTS = launch-tests

launch-tests: CoglTests.dll

tests_sources =			\
	tests/Config.cs		\
	tests/TestColor.cs	\
	tests/TestMatrix.cs	\
	tests/TestTexture.cs	\
	$(NULL)

CoglTests.dll: $(tests_sources) cogl2-sharp.dll
	$(V_MCS)$(MCS) $(CSFLAGS) @$(srcdir)/CoglTests.rsp -lib:$(top_builddir)/cogl-sharp -r:cogl2-sharp.dll $(MONO_NUNIT_LIBS) $(tests_sources)

REWRITE = sed -e "s|\@testsdir\@|$(srcdir)/tests|g"

tests/Config.cs: tests/Config.cs.in Makefile
	$(AM_V_GEN)$(REWRITE) $@.in > $@.tmp
	@mv $@.tmp $@

EXTRA_DIST += $(tests_sources)
CLEANFILES += CoglTests.dll
endif

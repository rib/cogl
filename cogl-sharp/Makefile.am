
assemblydir = $(libdir)/cogl
assembly_DATA = cogl2-sharp.dll cogl2-sharp.dll.config cogl2-sharp.dll.mdb

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cogl2-sharp.pc

sources =		\
	Color.cs	\
	AssemblyInfo.cs	\
	$(NULL)

cogl2-sharp.dll: $(sources)
	@rm -f cogl2-sharp.dll.mdb
	$(MCS) $(CSFLAGS) @$(srcdir)/cogl2-sharp.rsp -debug $(sources)

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

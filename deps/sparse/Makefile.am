include $(top_srcdir)/build/autotools/Makefile.am.silent

# preamble

NULL =

GCC_BASE = $(shell $(CC) --print-file-name=)
AM_CFLAGS = -DGCC_BASE=\"$(GCC_BASE)\"

noinst_LIBRARIES = libsparse.a

libsparse_a_SOURCES = \
	token.h \
	parse.h \
	lib.h \
	symbol.h \
	scope.h \
	expression.h \
	target.h \
	linearize.h \
	bitmap.h \
	ident-list.h \
	compat.h \
	flow.h \
	allocate.h \
	storage.h \
	ptrlist.h \
	dissect.h \
	target.c \
	parse.c \
	tokenize.c \
	pre-process.c \
	symbol.c \
	lib.c \
	scope.c \
	expression.c \
	show-parse.c \
	evaluate.c \
	expand.c \
	inline.c \
	linearize.c \
	sort.c \
	allocate.c \
	compat-linux.c \
	ptrlist.c \
	flow.c \
	cse.c \
	simplify.c \
	memops.c \
	liveness.c \
	storage.c \
	unssa.c \
	dissect.c \
	$(NULL)

noinst_PROGRAMS = dump-gtk-doc c2xml

dump_gtk_doc_SOURCES=dump-gtk-doc.c
dump_gtk_doc_LDADD=libsparse.la

c2xml_SOURCES=c2xml.c
c2xml_LDADD=libsparse.la
c2xml_CFLAGS=$(LIBXML_CFLAGS)
c2xml_LDFLAGS=$(LIBXML_LIBS)

#compile_EXTRA_DEPS = compile-i386.o
#
#compat-linux.o: compat/strtold.c compat/mmap-blob.c $(LIB_H)
#
#clean: clean-check
#	rm -f *.[oa] .*.d *.so $(PROGRAMS) $(SLIB_FILE) pre-process.h sparse.pc
#
#dist:
#	@if test "`git describe`" != "v$(VERSION)" ; then \
#		echo 'Update VERSION in the Makefile before running "make dist".' ; \
#		exit 1 ; \
#	fi
#	git archive --format=tar --prefix=sparse-$(VERSION)/ HEAD^{tree} | gzip -9 > sparse-$(VERSION).tar.gz
#
#check: all
#	$(Q)cd validation && ./test-suite

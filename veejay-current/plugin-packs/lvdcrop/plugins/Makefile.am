lividoplugin_LTLIBRARIES = lvd_crop.la lvd_cropscale.la 

if HAVE_GAVL
lividoplugin_LTLIBRARIES += lvd_scale0tilt.la
endif

lvd_crop_la_SOURCES = lvd_crop.c
lvd_cropscale_la_SOURCES = lvd_cropscale.c
if HAVE_GAVL
lvd_scale0tilt_la_SOURCES = lvd_scale0tilt.c
endif

AM_CPPFLAGS=$(FFMPEG_CFLAGS) $(GAVL_CFLAGS) $(VEEJAYCORE_CFLAGS) -I$(top_srcdir)/lvdcrop
AM_CFLAGS=-I@top_srcdir@/include $(OPTCFLAGS) -DSILENT
AM_LDFLAGS=-module -avoid-version -lm -export-dynamic -lstdc++ -DDYNAMIC -rdynamic $(FFMPEG_LIBS) $(GAVL_LIBS)
AM_LIBTOOLFLAGS = --tag=disable-static

lividoplugindir = @libdir@/livido-plugins

install-lividopluginLTLIBRARIES:$(lividoplugin_LTLIBRARIES)
	mkdir -p $(DESTDIR)/$(lividoplugindir)
	list='$(lividoplugin_LTLIBRARIES)'; \
	for file in $$list; do \
	 sofile=`basename $$file .la`.so; \
	 $(INSTALL_PROGRAM) .libs/$$sofile $(DESTDIR)/$(lividoplugindir);\
	done

uninstall-lividopluginLTLIBRARIES:
	list='$(lividoplugin_LTLIBRARIES)'; \
	for file in $$list; do \
	 sofile=`basename $$file .la`.so; \
	 rm -f $(DESTDIR)/$(lividoplugindir)/$$sofile; \
	done


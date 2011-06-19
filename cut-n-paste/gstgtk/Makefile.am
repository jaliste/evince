INCLUDES =                                      \
        -I$(top_srcdir)                         \
        $(BACKEND_CFLAGS)                       \
        $(GSTREAMER_CFLAGS)                     \
        $(WARN_CXXFLAGS)                        \
        $(DISABLE_DEPRECATED)

noinst_LTLIBRARIES = libgstgtk.la

libgstgtk_la_SOURCES =  \
	gtkvideoplayer.c \
	gtkvideoplayer.h \ 
	gtkvideowidget.c \ 
	gtkvideowidget.h

libgstgtk_la_CFLAGS =		\
	$(SHELL_CORE_CFLAGS)	\
	$(WARNING_CFLAGS)	\
	$(DISABLE_DEPRECATED)

-include $(top_srcdir)/git.mk

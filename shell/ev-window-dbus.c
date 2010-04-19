/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2010 Jose Aliste 
 *
 *  Author:
 *    Jose Aliste <jose.aliste@gmail.com>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "ev-window.h"
#include "ev-window-dbus.h"

void ev_window_dbus_sync_view (EvWindow *window, gchar *file, gint line, gint col);
#include "ev-window-service.h"
#include "ev-marshal.h"

enum {
	SIGNAL_SYNC_SOURCE,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (EvWindowDBus, ev_window_dbus, EV_TYPE_WINDOW)

#define WINDOW_DBUS_OBJECT_PATH "/org/gnome/evince/Window"
#define WINDOW_DBUS_INTERFACE   "org.gnome.evince.Window"
#define DBUS_TYPE_SOURCE_LINK (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))
#define DBUS_TYPE_SOURCE_LINK_ARRAY (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_SOURCE_LINK))


static void
view_sync_source_cb (EvView   *view, gpointer data,
		    EvWindowDBus *ev_window)
{
	GList *list_source = (GList *) data;
	GList *list = list_source;
	GPtrArray * array = g_ptr_array_new();

	while (list_source != NULL) {
		GValue *value;
		EvSourceLink 	*source;

		source = (EvSourceLink *) list_source->data;
		value = g_new0 (GValue, 1);
		g_value_init (value, DBUS_TYPE_SOURCE_LINK);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (DBUS_TYPE_SOURCE_LINK));
      		dbus_g_type_struct_set (value, 0, source->uri, 1, source->line, 2, source->col, G_MAXUINT);
      		g_ptr_array_add (array, g_value_get_boxed (value));

		g_free (source->uri);
		g_free (source);
		g_free (value);

		list_source = g_list_next (list_source);
	}

	g_signal_emit (ev_window, signals[SIGNAL_SYNC_SOURCE], 0, array);
	g_list_free (list);
	g_ptr_array_free (array, TRUE);
}

static void
ev_window_dbus_finalize (GObject *object)
{
	G_OBJECT_CLASS (ev_window_dbus_parent_class)->finalize (object);
}
static void
ev_window_dbus_dispose (GObject *object)
{
	G_OBJECT_CLASS (ev_window_dbus_parent_class)->dispose (object);
}

static void
ev_window_dbus_class_init (EvWindowDBusClass *ev_window_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_window_class);
	
	signals[SIGNAL_SYNC_SOURCE] = g_signal_new ("sync-source",
	  	         G_TYPE_FROM_CLASS (g_object_class),
		         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		         0,
		         NULL, NULL,
		         g_cclosure_marshal_VOID__POINTER,
		         G_TYPE_NONE, 1,
		         DBUS_TYPE_SOURCE_LINK_ARRAY);
	dbus_g_object_type_install_info (EV_TYPE_WINDOW_DBUS,
					 &dbus_glib_ev_window_object_info);
	g_object_class->dispose = ev_window_dbus_dispose;
	g_object_class->finalize = ev_window_dbus_finalize;

}

static void
ev_window_dbus_init (EvWindowDBus *ev_window)
{
	EvWindow *window = EV_WINDOW (ev_window);
	EvView 	 *view   = EV_VIEW (ev_window_get_view (window));
	g_signal_connect_object (view, "sync-source",
			         G_CALLBACK (view_sync_source_cb),
			         ev_window, 0);
	static int count = 1;
	DBusGConnection *connection;
	gchar path[100];
	GError *error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
	g_sprintf (path, "%s/%d", WINDOW_DBUS_OBJECT_PATH, count);
	if (connection) {
		dbus_g_connection_register_g_object (connection, path, G_OBJECT (ev_window));
		count++;
	}
}

/**
 * ev_window_dbus_new:
 *
 * Creates a #GtkWidget that represents the window. This window is controllable through DBUS.
 *
 * Returns: the #GtkWidget that represents the window.
 */
GtkWidget *
ev_window_dbus_new (void)
{
	GtkWidget *ev_window_dbus;

	ev_window_dbus = GTK_WIDGET (g_object_new (EV_TYPE_WINDOW_DBUS,
					      "type", GTK_WINDOW_TOPLEVEL,
					      NULL));

	return ev_window_dbus;
}

void 
ev_window_dbus_sync_view (EvWindow *window, gchar *file, gint line, gint col)
{
	gint n_pages;
	GList **results;
	EvDocument *document = ev_window_get_document (window);
	if (!document)
		return;
	results = ev_document_sync_to_view (document, file, line, col);
	n_pages = ev_document_get_n_pages (document);
	ev_view_set_sync_rects ( ev_window_get_view (window), results);
	
	gtk_window_present (GTK_WINDOW (window));
}

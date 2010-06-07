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

#include "ev-marshal.h"

#define EV_WINDOW_DBUS_OBJECT_PATH "/org/gnome/evince/Window"
#define EV_WINDOW_DBUS_INTERFACE   "org.gnome.evince.Window"

struct _EvWindowDBus {
	EvWindow		base_instance;
	guint registration_id;
	GDBusConnection *connection;
	gchar 		*dbus_path;
};

struct _EvWindowDBusClass {
	EvWindowClass		base_class;
};

G_DEFINE_TYPE (EvWindowDBus, ev_window_dbus, EV_TYPE_WINDOW)

static void
view_sync_source_cb (EvView   	  	*view, gpointer data,
		     EvWindowDBus 	*ev_window)
{
	GList *list_source = (GList *) data;
	GList *list = list_source;
	GVariantBuilder *builder;
	
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a(sii)"));

	while (list_source != NULL) {
		EvSourceLink 	*source;

		source = (EvSourceLink *) list_source->data;
		g_variant_builder_add (builder,"(sii)", source->uri, source->line, source->col);
		
      		g_free (source->uri);
		g_free (source);
		list_source = g_list_next (list_source);
	}
	g_dbus_connection_emit_signal (ev_window->connection,
                                 NULL,
                                 ev_window->dbus_path,
                                 EV_WINDOW_DBUS_INTERFACE,
                                 "SyncSource",
                                 g_variant_new ("(a(sii))", builder),
                                 NULL);
        g_variant_builder_unref (builder);
	g_list_free (list);
}

static void
method_call_cb (GDBusConnection       *connection,
                const gchar           *sender,
                const gchar           *object_path,
                const gchar           *interface_name,
                const gchar           *method_name,
                GVariant              *parameters,
                GDBusMethodInvocation *invocation,
                gpointer               user_data)
{
        EvWindow   *window = EV_WINDOW (user_data);
        EvDocument     *document;
	GList **results;
	const gchar *file;
	int	line, col;

	if (g_strcmp0 (method_name, "SyncView") != 0)
        	return;

	/* TODO:-  We should ensure that the first result is visible.
 	 *      -  We need a way of unsetting the highlight after some event also. */
	
	document = ev_window_get_document (window);
	if (document == NULL)
		return;
	g_variant_get (parameters, "(&sii)", &file, &line, &col);

	results = ev_document_sync_to_view (document, file, line, col);
	ev_view_set_sync_rects ( ev_window_get_view (window), results);
	if (results)	
		gtk_window_present (GTK_WINDOW (window));
}

static const char introspection_xml[] =
	"<node>"
	   "<interface name='org.gnome.evince.Window'>"
	   "<signal name='SyncSource'>"
	      "<arg type='a(sii)' name='source_links' direction='out'/>"
	   "</signal>"
	   "<method name='SyncView'>"
	      "<arg type='s' name='input_file' direction='in'/>"
	      "<arg type='i' name='line' direction='in'/>"
	      "<arg type='i' name='col'  direction='in'/>"
	    "</method>"
	   "</interface>"
	"</node>";

static const GDBusInterfaceVTable interface_vtable = {
	method_call_cb,
	NULL,
	NULL
};

static GDBusNodeInfo *introspection_data;


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

	g_object_class->dispose = ev_window_dbus_dispose;
	g_object_class->finalize = ev_window_dbus_finalize;

}

static void
ev_window_dbus_init (EvWindowDBus *ev_window)
{
	EvWindow *window = EV_WINDOW (ev_window);
	EvView 	 *view   = EV_VIEW (ev_window_get_view (window));
	static int count = 1;

	GError *error = NULL;
	
	ev_window->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	g_signal_connect_object (view, "sync-source",
			         G_CALLBACK (view_sync_source_cb),
			         ev_window, 0);
      	ev_window->dbus_path = g_strdup_printf ("%s/%d", EV_WINDOW_DBUS_OBJECT_PATH, count);

	if (ev_window->connection != NULL) {
		introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
                g_assert (introspection_data != NULL);

                ev_window->registration_id =
                    g_dbus_connection_register_object (ev_window->connection,
                                                       ev_window->dbus_path,
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       ev_window, NULL,
                                                       &error);

                if (ev_window->registration_id == 0) {
                        g_printerr ("Failed to register bus object: %s\n", error->message);
                        g_error_free (error);
                        return;
                }
  		count++;
                
        } else {
                g_printerr ("Failed to get bus connection: %s\n", error->message);
                g_error_free (error);
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


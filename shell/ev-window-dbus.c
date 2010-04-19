/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Juanjo Marín <juanj.marin@juntadeandalucia.es>
 *  Copyright (C) 2008 Carlos Garcia Campos
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2009 Christian Persch
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#ifdef WITH_GCONF
#include <gconf/gconf-client.h>
#endif

#include "egg-editable-toolbar.h"
#include "egg-toolbar-editor.h"
#include "egg-toolbars-model.h"

#include "eggfindbar.h"

#include "ephy-zoom-action.h"
#include "ephy-zoom.h"

#include "ev-application.h"
#include "ev-document-factory.h"
#include "ev-document-find.h"
#include "ev-document-fonts.h"
#include "ev-document-images.h"
#include "ev-document-links.h"
#include "ev-document-thumbnails.h"
#include "ev-document-annotations.h"
#include "ev-document-type-builtins.h"
#include "ev-document-misc.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"
#include "ev-file-monitor.h"
#include "ev-history.h"
#include "ev-image.h"
#include "ev-job-scheduler.h"
#include "ev-jobs.h"
#include "ev-message-area.h"
#include "ev-metadata.h"
#include "ev-navigation-action.h"
#include "ev-open-recent-action.h"
#include "ev-page-action.h"
#include "ev-password-view.h"
#include "ev-properties-dialog.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-sidebar-layers.h"
#include "ev-stock-icons.h"
#include "ev-utils.h"
#include "ev-keyring.h"
#include "ev-view.h"
#include "ev-view-presentation.h"
#include "ev-view-type-builtins.h"
#include "ev-window.h"
#include "ev-window-dbus.h"
#include "ev-window-title.h"
#include "ev-print-operation.h"
#include "ev-progress-message-area.h"

void ev_window_dbus_sync_view (EvWindow *window, gchar *file, gint line, gint col);
#include "ev-window-service.h"
#include "ev-marshal.h"

enum {
	SIGNAL_SYNC_SOURCE,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define EV_WINDOW_IS_PRESENTATION(w) (w->priv->presentation_view != NULL)

#define PAGE_SELECTOR_ACTION	"PageSelector"
#define ZOOM_CONTROL_ACTION	"ViewZoom"
#define NAVIGATION_ACTION	"Navigation"

#define GCONF_LOCKDOWN_DIR          "/desktop/gnome/lockdown"
#define GCONF_OVERRIDE_RESTRICTIONS "/apps/evince/override_restrictions"
#define GCONF_LOCKDOWN_SAVE         "/desktop/gnome/lockdown/disable_save_to_disk"
#define GCONF_LOCKDOWN_PRINT        "/desktop/gnome/lockdown/disable_printing"
#define GCONF_LOCKDOWN_PRINT_SETUP  "/desktop/gnome/lockdown/disable_print_setup"

#define SIDEBAR_DEFAULT_SIZE    132
#define LINKS_SIDEBAR_ID "links"
#define THUMBNAILS_SIDEBAR_ID "thumbnails"
#define ATTACHMENTS_SIDEBAR_ID "attachments"
#define LAYERS_SIDEBAR_ID "layers"

#define EV_PRINT_SETTINGS_FILE  "print-settings"
#define EV_PRINT_SETTINGS_GROUP "Print Settings"
#define EV_PAGE_SETUP_GROUP     "Page Setup"

#define EV_TOOLBARS_FILENAME "evince-toolbar.xml"

#define MIN_SCALE 0.05409
#define MAX_SCALE 4.0




G_DEFINE_TYPE (EvWindowDBus, ev_window_dbus, EV_TYPE_WINDOW)

#define WINDOW_DBUS_OBJECT_PATH "/org/gnome/evince/Evince/Window"
#define WINDOW_DBUS_INTERFACE   "org.gnome.evince.Window"

static void
view_sync_source_cb (EvView   *view, gchar *file_uri, int x, int y,
		    EvWindowDBus *ev_window)
{
	printf("Hola, %s,%d,%d\n",file_uri,x,y);
	g_signal_emit (ev_window, signals[SIGNAL_SYNC_SOURCE], 0, file_uri, x, y);
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
		         ev_marshal_VOID__STRING_INT_INT,
		         G_TYPE_NONE, 3,
		         G_TYPE_STRING,
		         G_TYPE_INT,
		         G_TYPE_INT);
	dbus_g_object_type_install_info (EV_TYPE_WINDOW_DBUS,
					 &dbus_glib_ev_window_object_info);
	g_object_class->dispose = ev_window_dbus_dispose;
	g_object_class->finalize = ev_window_dbus_finalize;

}

static void
ev_window_dbus_init (EvWindowDBus *ev_window)
{
	printf("dbus_init\n");
	EvWindow *window = EV_WINDOW (ev_window);
	EvView 	 *view   = EV_VIEW (ev_window_get_view (window));
	printf("view %p\n",view);
	g_signal_connect_object (view, "sync-source",
			         G_CALLBACK (view_sync_source_cb),
			         ev_window, 0);
	static int count = 1;
	DBusGConnection *connection;
	gchar path[100];
	GError *error = NULL;
	connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
	g_sprintf(path,"%s/%d",WINDOW_DBUS_OBJECT_PATH, count);
	if (connection) {
		dbus_g_connection_register_g_object (connection,
					 path,
					     G_OBJECT (ev_window));
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

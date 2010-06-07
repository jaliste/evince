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

#ifndef EV_WINDOW_DBUS_H
#define EV_WINDOW_DBUS_H

#include <glib.h>
#include <gtk/gtk.h>

#include "ev-link.h"

G_BEGIN_DECLS


typedef struct _EvWindowDBus EvWindowDBus;
typedef struct _EvWindowDBusClass EvWindowDBusClass;
typedef struct _EvWindowDBusPrivate EvWindowDBusPrivate;

#define EV_TYPE_WINDOW_DBUS			(ev_window_dbus_get_type())
#define EV_WINDOW_DBUS(object)			(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_WINDOW_DBUS, EvWindowDBus))
#define EV_WINDOW_DBUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_WINDOW_DBUS, EvWindowDBusClass))
#define EV_IS_WINDOW_DBUS(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_WINDOW_DBUS))
#define EV_IS_WINDOW_DBUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_WINDOW_DBUS))
#define EV_WINDOW_DBUS_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_WINDOW_DBUS, EvWindowDBusClass))



GType		ev_window_dbus_get_type	(void) G_GNUC_CONST;
GtkWidget      *ev_window_dbus_new           (void);

G_END_DECLS

#endif /* !EV_WINDOW_DBUS_H */

/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2010 Jose Aliste <jose.aliste@gmail.com>
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_MEDIA_H
#define EV_MEDIA_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_MEDIA              (ev_media_get_type())
#define EV_MEDIA(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_MEDIA, EvMedia))
#define EV_MEDIA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_MEDIA, EvMediaClass))
#define EV_IS_MEDIA(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_MEDIA))
#define EV_IS_MEDIA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_MEDIA))
#define EV_MEDIA_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_MEDIA, EvMediaClass))

typedef struct _EvMedia        EvMedia;
typedef struct _EvMediaClass   EvMediaClass;

struct _EvMedia {
	GObject  base_instance;
};

struct _EvMediaClass {
	GObjectClass base_class;
	gchar           * (* get_uri)  (EvMedia     *media);

	
};

GType   ev_media_get_type (void) G_GNUC_CONST;

gchar   *ev_media_get_uri (EvMedia *media);
G_END_DECLS

#endif /* EV_MEDIA_H */

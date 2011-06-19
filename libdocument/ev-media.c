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

#include <config.h>

#include "ev-media.h"

G_DEFINE_TYPE (EvMedia, ev_media, G_TYPE_OBJECT)

static void
ev_media_init (EvMedia *media)
{
}

static void
ev_media_finalize (GObject *object)
{
	EvMedia *media = EV_MEDIA (object);
	(* G_OBJECT_CLASS (ev_media_parent_class)->finalize) (object);
}

static void
ev_media_class_init (EvMediaClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = ev_media_finalize;
}

gchar *
ev_media_get_uri (EvMedia *media) 
{
	EvMediaClass *klass = EV_MEDIA_GET_CLASS (media);
	return klass->get_uri (media);
}




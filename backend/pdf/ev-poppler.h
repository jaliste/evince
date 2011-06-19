/* pdfdocument.h: Implementation of EvDocument for PDF
 * Copyright (C) 2004, Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include "ev-document.h"
#include "ev-media.h"

G_BEGIN_DECLS

#define PDF_TYPE_DOCUMENT             (pdf_document_get_type ())
#define PDF_DOCUMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PDF_TYPE_DOCUMENT, PdfDocument))
#define PDF_IS_DOCUMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PDF_TYPE_DOCUMENT))

#define PDF_TYPE_MEDIA                  (pdf_media_get_type ())
#define PDF_MEDIA(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PDF_TYPE_MEDIA, PdfMedia))
#define PDF_IS_MEDIA(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PDF_TYPE_MEDIA))

/*#define PDF_MEDIA_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PDF_TYPE_MEDIA, MamanBarClass))
#define PDF_IS_MEDIA_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), PDF_TYPE_MEDIA))
#define PDF_MEDIA_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), PDF_TYPE_MEDIA, MamanBarClass))
*/
typedef struct _PdfDocument PdfDocument;
typedef struct _PdfDocumentClass PdfDocumentClass;


typedef struct _PdfMedia PdfMedia;
typedef struct _PdfMediaClass PdfMediaClass;

GType                 pdf_document_get_type   (void) G_GNUC_CONST;
GType                 pdf_media_get_type      (void) G_GNUC_CONST;


G_MODULE_EXPORT GType register_evince_backend (GTypeModule *module);


G_END_DECLS

#endif /* __PDF_DOCUMENT_H__ */

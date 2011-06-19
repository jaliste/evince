
#ifndef __GTK_VIDEO_WIDGET_H__
#define __GTK_VIDEO_WIDGET_H__

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GTK_TYPE_VIDEO_WIDGET         (gtk_video_widget_get_type ())
#define GTK_VIDEO_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_VIDEO_WIDGET, GtkVideoWidget))
#define GTK_VIDEO_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_VIDEO_WIDGET, GtkVideoWidgetClass))
#define GTK_IS_VIDEO_WIDGET(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIDEO_WIDGET))
#define GTK_IS_VIDEO_WIDGET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIDEO_WIDGET))
#define GTK_VIDEO_WIDGET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIDEO_WIDGET, GtkVideoWidgetClass))


typedef struct _GtkVideoWidget GtkVideoWidget;
typedef struct _GtkVideoWidgetClass GtkVideoWidgetClass;

#if 0
typedef enum GtkVideoWidgetSizePolicy {
  GTK_VIDEO_WIDGET_SIZE_FIXED,
  GTK_VIDEO_WIDGET_SIZE_1_1,
  GTK_VIDEO_WIDGET_SIZE_HALF,
  GTK_VIDEO_WIDGET_SIZE_QUARTER
};
#endif

struct _GtkVideoWidget {
  GtkDrawingArea drawing_area;

  /*< private >*/
  GstElement *pipeline;
  GstElement *videosink;
  GtkWidget *toplevel;
  gulong toplevel_signal_handle;
  GdkGC *gc;

  GstBus *bus;

  int width;
  int height;
  int par_n;
  int par_d;

  gboolean forcing_resize;

  int default_height;
  int default_width;
  gboolean resize_for_media;
};

struct _GtkVideoWidgetClass {
  GtkDrawingAreaClass parent_class;

  void (*end_of_stream) (GtkVideoWidget *widget);
  void (*error) (GtkVideoWidget *widget);
};

GType gtk_video_widget_get_type (void) G_GNUC_CONST;
GtkWidget *gtk_video_widget_new (void);

//void gtk_video_widget_set_uri (GtkVideoWidget *video_widget, const char *uri);

void gtk_video_widget_set_pipeline (GtkVideoWidget *video_widget, GstPipeline *pipeline);

//void gtk_video_widget_set_playing (GtkVideoWidget *video_widget, gboolean playing);
//void gtk_video_widget_rewind (GtkVideoWidget *video_widget);

void gtk_video_widget_set_default_size (GtkVideoWidget *video_widget,
    int width, int height);
void gtk_video_widget_set_resize_for_media (GtkVideoWidget *video_widget,
    gboolean resize);


G_END_DECLS

#endif


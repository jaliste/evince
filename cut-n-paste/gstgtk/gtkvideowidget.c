
#include <config.h>
#include <gtkvideowidget.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/video/video.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#elif defined(GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined(GDK_WINDOWING_QUARTZ)
//#include <gdk/gdkquartz.h>
#endif




static void gtk_video_widget_realize (GtkWidget *widget, gpointer user_data);
static void on_message (GstBus *bus, GstMessage *message, GtkVideoWidget *video_widget);
static void on_sync_message (GstBus *bus, GstMessage *message, GtkVideoWidget *video_widget);
static void do_expose_event (GtkVideoWidget *video_widget, GdkEvent *event);
static void do_hierarchy_changed (GtkVideoWidget *video_widget);
static void do_property_notify_event (GtkVideoWidget *video_widget);
static void do_size_allocate (GtkVideoWidget *video_widget, GtkAllocation *req,
    gpointer ptr);
static void do_size_request (GtkVideoWidget *video_widget, GtkRequisition *req,
    gpointer ptr);
static gboolean do_toplevel_configure_event (GtkVideoWidget *video_widget);
static void on_caps (GObject *obj, GParamSpec *pspec, GtkVideoWidget *video_widget);

G_DEFINE_TYPE (GtkVideoWidget, gtk_video_widget, GTK_TYPE_DRAWING_AREA)


static guint gtk_video_widget_signal_end_of_stream;
static guint gtk_video_widget_signal_error;

static void
gtk_video_widget_class_init (GtkVideoWidgetClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) klass;

  /**
   * GtkVideoWidget::end-of-stream:
   * @widget: the video widget
   * 
   * An end of stream event occured in the media stream.
   */
  gtk_video_widget_signal_end_of_stream =
    g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (GtkVideoWidgetClass, end_of_stream), NULL, NULL,
        gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GtkVideoWidget::error:
   * @widget: the video widget
   * 
   * An error occured decoding the media stream.
   */
  gtk_video_widget_signal_error =
    g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (GtkVideoWidgetClass, error), NULL, NULL,
        gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

}

static void
gtk_video_widget_init (GtkVideoWidget *video_widget)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (video_widget), GTK_DOUBLE_BUFFERED);

  video_widget->width = 640;
  video_widget->height = 360;
  video_widget->forcing_resize = FALSE;

  video_widget->default_width = 400;
  video_widget->default_height = 300;

  video_widget->videosink = NULL;

  g_signal_connect (video_widget, "realize",
      G_CALLBACK(gtk_video_widget_realize), NULL);

}

GtkWidget *
gtk_video_widget_new (void)
{
  GtkVideoWidget *video_widget;

  video_widget = g_object_new (GTK_TYPE_VIDEO_WIDGET, NULL);

  return GTK_WIDGET (video_widget);
}
 
void
gtk_video_widget_set_pipeline (GtkVideoWidget *video_widget,
    GstPipeline *pipeline)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  if (video_widget->pipeline) {
    gst_object_unref (video_widget->pipeline);
  }
  if (video_widget->bus) {
    gst_object_unref (video_widget->bus);
  }
  if (pipeline == NULL) {
    video_widget->pipeline = NULL;
    return;
  }

  video_widget->pipeline = gst_object_ref(pipeline);
  video_widget->bus = gst_pipeline_get_bus (GST_PIPELINE(video_widget->pipeline));

  gst_bus_enable_sync_message_emission(video_widget->bus);
  gst_bus_add_signal_watch (video_widget->bus);

  g_signal_connect (G_OBJECT(video_widget->bus), "sync-message::element",
      G_CALLBACK(on_sync_message), video_widget);
  g_signal_connect (G_OBJECT(video_widget->bus), "message",
      G_CALLBACK(on_message), video_widget);

  g_signal_connect (G_OBJECT(video_widget), "expose_event",
      G_CALLBACK(do_expose_event), NULL);
  g_signal_connect (G_OBJECT(video_widget), "hierarchy_changed",
      G_CALLBACK(do_hierarchy_changed), NULL);
  g_signal_connect (G_OBJECT(video_widget), "property_notify_event",
      G_CALLBACK(do_property_notify_event), NULL);
  g_signal_connect (G_OBJECT(video_widget), "size_request",
      G_CALLBACK(do_size_request), NULL);
  g_signal_connect (G_OBJECT(video_widget), "size_allocate",
      G_CALLBACK(do_size_allocate), NULL);
}

static void
gtk_video_widget_realize (GtkWidget *widget, gpointer user_data)
{
#ifdef GDK_WINDOWING_X11
  GDK_WINDOW_XID (gtk_widget_get_window (widget));
#endif
}

static void
on_sync_message (GstBus *bus, GstMessage *message, GtkVideoWidget *video_widget)
{
  const GstStructure *structure;

  g_return_if_fail (GST_IS_BUS (bus));
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  //g_print("on_sync_message: %d\n", message->type);

  structure = gst_message_get_structure (message);
#if 0
  if (structure) {
    g_print("%s\n", gst_structure_to_string (structure));
  } else {
    g_print("NULL\n");
  }
#endif

  if (gst_structure_has_name(structure, "prepare-xwindow-id")) {
    video_widget->videosink = GST_ELEMENT(GST_MESSAGE_SRC(message));
    g_signal_connect (gst_element_get_pad (video_widget->videosink,
          "sink"), "notify::caps", G_CALLBACK(on_caps), video_widget);

    g_object_set (video_widget->videosink, "force-aspect-ratio", TRUE, NULL);

    GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (video_widget), GTK_DOUBLE_BUFFERED);

#ifdef GDK_WINDOWING_X11
    gst_x_overlay_set_xwindow_id (GST_X_OVERLAY(video_widget->videosink),
        GDK_WINDOW_XID(gtk_widget_get_window (GTK_WIDGET(video_widget))));
#elif defined(GDK_WINDOWING_WIN32)
    gst_x_overlay_set_xwindow_id (GST_X_OVERLAY(video_widget->videosink),
        (unsigned long)gdk_win32_drawable_get_handle(
          gtk_widget_get_window(GTK_WIDGET(video_widget))));
#elif defined(GDK_WINDOWING_QUARTZ)
    /* FIXME bogus */
#if 0
    gst_x_overlay_set_xwindow_id (GST_X_OVERLAY(video_widget->videosink),
        gdk_quartz_window_get_nswindow (gtk_widget_get_window(
            GTK_WIDGET(video_widget))));
#endif
#else
#error unimplemented GTK backend
#endif
  }
}

static void
on_message (GstBus *bus, GstMessage *message, GtkVideoWidget *video_widget)
{
  const GstStructure *structure;

  g_return_if_fail (GST_IS_BUS (bus));
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  //g_print("on_message: %d\n", message->type);
  structure = gst_message_get_structure (message);
#if 0
  if (structure) {
    g_print("%s\n", gst_structure_to_string (structure));
  } else {
    g_print("NULL\n");
  }
#endif

  switch (message->type) {
    case GST_MESSAGE_ERROR:
      g_signal_emit (video_widget, gtk_video_widget_signal_error, 0);
      break;
    case GST_MESSAGE_EOS:
      g_signal_emit (video_widget, gtk_video_widget_signal_end_of_stream, 0);
      break;
    case GST_MESSAGE_APPLICATION:
      g_print("APP message bus %p\n", bus);
      break;
    default:
      break;
  }

}

static gboolean
queue_resize (gpointer user_data)
{
  GtkVideoWidget *video_widget = GTK_VIDEO_WIDGET(user_data);

  gtk_widget_queue_resize (GTK_WIDGET(video_widget));

  return FALSE;
}

static void
on_caps (GObject *obj, GParamSpec *pspec, GtkVideoWidget *video_widget)
{
  GstPad *pad = GST_PAD (obj);
  GstVideoFormat format;
  GstCaps *caps;

  caps = gst_pad_get_negotiated_caps (pad);
  if (caps == NULL) return;

  gst_video_format_parse_caps (caps, &format,
    &video_widget->width, &video_widget->height);
  gst_video_parse_caps_pixel_aspect_ratio (caps,
    &video_widget->par_n, &video_widget->par_d);

  GST_DEBUG ("on caps %dx%d", video_widget->width, video_widget->height);

  gst_caps_unref (caps);

  video_widget->forcing_resize = TRUE;
  g_object_ref (video_widget);
  g_idle_add_full (G_PRIORITY_DEFAULT, queue_resize, video_widget,
      g_object_unref);
}

static void
do_expose_event (GtkVideoWidget *video_widget, GdkEvent *event)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  if (video_widget->videosink) {
    gst_x_overlay_expose (GST_X_OVERLAY(video_widget->videosink));
  } else {
    GST_DEBUG("drawing rect");

    if (video_widget->gc == NULL) {
      video_widget->gc = gdk_gc_new (GTK_WIDGET(video_widget)->window);
    }
    gdk_draw_rectangle (GTK_WIDGET(video_widget)->window,
        video_widget->gc,
        TRUE, 0, 0, GTK_WIDGET(video_widget)->allocation.width,
        GTK_WIDGET(video_widget)->allocation.height);
  }
}

static void
do_hierarchy_changed (GtkVideoWidget *video_widget)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  if (video_widget->toplevel) {
    g_signal_handler_disconnect (video_widget->toplevel,
        video_widget->toplevel_signal_handle);
  }
  video_widget->toplevel = gtk_widget_get_toplevel (GTK_WIDGET(video_widget));
  video_widget->toplevel_signal_handle =
    g_signal_connect_swapped (video_widget->toplevel,
        "configure_event",
        G_CALLBACK(do_toplevel_configure_event), video_widget);
}

static void
do_property_notify_event (GtkVideoWidget *video_widget)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

}

static void
do_size_allocate (GtkVideoWidget *video_widget, GtkAllocation *req,
    gpointer ptr)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  GST_DEBUG("size_allocate %dx%d", req->width, req->height);
}

static void
do_size_request (GtkVideoWidget *video_widget, GtkRequisition *req,
    gpointer ptr)
{
  g_return_if_fail (GTK_IS_VIDEO_WIDGET(video_widget));

  if (video_widget->resize_for_media) {
    if (video_widget->forcing_resize) {
      req->width = video_widget->width;
      req->height = video_widget->height;

      video_widget->forcing_resize = FALSE;
      gtk_widget_queue_resize (GTK_WIDGET(video_widget));
    } else {
      req->width = video_widget->width;
      req->height = video_widget->height;
    }
  } else {
    req->width = video_widget->default_width;
    req->height = video_widget->default_height;
  }

  GST_DEBUG("size_req %d %d", req->width, req->height);
}

static gboolean
do_toplevel_configure_event (GtkVideoWidget *video_widget)
{
  g_return_val_if_fail (GTK_IS_VIDEO_WIDGET(video_widget), FALSE);

  gtk_widget_queue_draw_area (GTK_WIDGET(video_widget), 0, 0, 1, 1);

  return FALSE;
}

void gtk_video_widget_set_default_size (GtkVideoWidget *video_widget,
    int width, int height)
{
  video_widget->default_width = width;
  video_widget->default_height = height;
}

void gtk_video_widget_set_resize_for_media (GtkVideoWidget *video_widget,
    gboolean resize)
{
  video_widget->resize_for_media = resize;
}




#include <config.h>
#include <gtkvideoplayer.h>
#include <gtkvideowidget.h>

#define SEEK_FLAGS (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT)
//#define SEEK_FLAGS (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE)

G_DEFINE_TYPE (GtkVideoPlayer, gtk_video_player, GTK_TYPE_VBOX)

static void set_button_image (GtkWidget *button, const char *stock_id);
static void do_toggle_play_pause (GtkVideoPlayer *video_player);
static void do_change_value (GtkRange *range, GtkScrollType scroll_type,
    gdouble value, GtkVideoPlayer *video_player);
static void on_end_of_stream (GtkVideoPlayer *video_player);
static void on_error (GtkVideoPlayer *video_player);
static void on_message (GstBus *bus, GstMessage *message, GtkVideoPlayer *video_player);
static gboolean do_timeout (gpointer priv);

enum
{
  END_OF_STREAM,
  ERROR,
  LAST_SIGNAL
};

static guint gtk_video_player_signals[LAST_SIGNAL] = { 0 };

static void
gtk_video_player_class_init (GtkVideoPlayerClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) klass;

  /**
   * GtkVideoPlayer::end-of-stream:
   * @player: the video player
   * 
   * An end of stream event occured in the media stream.
   */
  gtk_video_player_signals[END_OF_STREAM] =
    g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (GtkVideoPlayerClass, end_of_stream), NULL, NULL,
        gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);
  /**
   * GtkVideoPlayer::error:
   * @player: the video player
   * 
   * An error occured decoding the media stream.
   */
  gtk_video_player_signals[ERROR] =
    g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET (GtkVideoPlayerClass, error), NULL, NULL,
        gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

}

static void
gtk_video_player_init (GtkVideoPlayer *video_player)
{
  GtkWidget *hbox;

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (video_player), GTK_DOUBLE_BUFFERED);

  video_player->video_widget = gtk_video_widget_new();

  video_player->button = gtk_button_new ();
  video_player->scale = gtk_hscale_new_with_range (0.0, 10.0, 5.0);
  gtk_scale_set_digits (GTK_SCALE(video_player->scale), 2);
  gtk_range_set_increments (GTK_RANGE(video_player->scale), 5.0, 10.0);
  gtk_range_set_update_policy (GTK_RANGE(video_player->scale),
      GTK_UPDATE_CONTINUOUS);
  hbox = gtk_hbox_new (FALSE, 4);
  set_button_image (video_player->button, GTK_STOCK_MEDIA_PLAY);

  gtk_box_pack_start (GTK_BOX(video_player), video_player->video_widget,
      TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX(video_player), hbox,
      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(hbox), video_player->button,
      FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX(hbox), video_player->scale,
      TRUE, TRUE, 0);

  g_signal_connect_swapped (video_player->button, "clicked",
      G_CALLBACK(do_toggle_play_pause),
      video_player);
  g_signal_connect (video_player->scale, "change-value",
      G_CALLBACK(do_change_value), video_player);
  g_signal_connect_swapped (video_player->video_widget, "end-of-stream",
      G_CALLBACK(on_end_of_stream), video_player);
  g_signal_connect_swapped (video_player->video_widget, "error",
      G_CALLBACK(on_error), video_player);

  video_player->player = gst_element_factory_make ("playbin2", "player");
  video_player->bus = gst_element_get_bus (video_player->player);

  gtk_video_widget_set_pipeline (GTK_VIDEO_WIDGET(video_player->video_widget),
      GST_PIPELINE(video_player->player));

  gst_bus_enable_sync_message_emission(video_player->bus);
  gst_bus_add_signal_watch (video_player->bus);

  g_signal_connect (G_OBJECT(video_player->bus), "message",
      G_CALLBACK(on_message), video_player);
}

GtkWidget *
gtk_video_player_new (void)
{
  GtkVideoPlayer *video_player;

  video_player = g_object_new (GTK_TYPE_VIDEO_PLAYER, NULL);

  return GTK_WIDGET (video_player);
}
 
void
gtk_video_player_set_uri (GtkVideoPlayer *video_player, const char *uri)
{
  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));
  g_return_if_fail (uri != NULL);

  video_player->duration = 0.0;
  video_player->eos = FALSE;
  video_player->playing = FALSE;
  set_button_image (video_player->button, GTK_STOCK_MEDIA_PLAY);

  gst_element_set_state (video_player->player, GST_STATE_READY);
  g_object_set (video_player->player, "uri", uri, NULL);
  gst_element_set_state (video_player->player, GST_STATE_PAUSED);
}

void
gtk_video_player_set_file (GtkVideoPlayer *video_player, const char *file)
{
  char *uri;

  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));
  g_return_if_fail (file != NULL);

  uri = g_strdup_printf("file://%s", file);
  gtk_video_player_set_uri (video_player, uri);
  g_free (uri);
}

static void
gtk_video_player_rewind (GtkVideoPlayer *video_player)
{
  gboolean res;

  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));

  res = gst_element_seek (video_player->player, 1.0, GST_FORMAT_TIME,
      SEEK_FLAGS,
      GST_SEEK_TYPE_SET, (gint64)(0),
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  if (res) {
    //gst_pipeline_set_new_stream_time (GST_PIPELINE(video_player->player), 0);
  } else {
    g_print("seek failed\n");
  }

  do_timeout (video_player);
}

static gboolean
do_timeout (gpointer priv)
{
  GtkVideoPlayer *video_player = GTK_VIDEO_PLAYER (priv);
  gint64 position;
  GstFormat format;
  gboolean ret;

  format = GST_FORMAT_TIME;
  ret = gst_element_query_position (GST_ELEMENT(video_player->player),
      &format, &position);

  if (ret && position != GST_CLOCK_TIME_NONE) {
    GST_DEBUG("position %g", position / (double)GST_SECOND);

    gtk_range_set_value (GTK_RANGE(video_player->scale),
        position / (double)GST_SECOND);
  }

  if (!video_player->playing || video_player->eos) {
    video_player->update_id = 0;
    return FALSE;
  }
  return TRUE;
}

static void
do_toggle_play_pause (GtkVideoPlayer *video_player)
{
  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));

  if (video_player->eos) {
    gtk_video_player_rewind (video_player);
    video_player->eos = FALSE;
  }

  video_player->playing = !video_player->playing;
  if (video_player->playing) {
    set_button_image (video_player->button, GTK_STOCK_MEDIA_PAUSE);
    gst_element_set_state (video_player->player, GST_STATE_PLAYING);
    if (video_player->update_id == 0) {
      video_player->update_id = g_timeout_add (100, do_timeout,
          video_player);
    }
  } else {
    set_button_image (video_player->button, GTK_STOCK_MEDIA_PLAY);
    gst_element_set_state (video_player->player, GST_STATE_PAUSED);
  }
}

void
gtk_video_player_seek (GtkVideoPlayer *video_player, gdouble position)
{
  gboolean res;

  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));

  res = gst_element_seek (video_player->player, 1.0, GST_FORMAT_TIME,
      SEEK_FLAGS,
      GST_SEEK_TYPE_SET, (gint64)(position * GST_SECOND),
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  if (!res) {
    g_print("seek failed\n");
  }
  do_timeout (video_player);
}

static void
do_change_value (GtkRange *range, GtkScrollType scroll_type, gdouble value,
    GtkVideoPlayer *video_player)
{
  double v;
  gboolean res;

  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));

  if (video_player->eos) {
    video_player->eos = FALSE;
    video_player->playing = FALSE;
    set_button_image (video_player->button, GTK_STOCK_MEDIA_PLAY);
    gst_element_set_state (video_player->player, GST_STATE_PAUSED);
  }

  //v = gtk_range_get_value (GTK_RANGE(video_player->scale));
  v = value;

  GST_ERROR("seek %g", v);

  res = gst_element_seek (video_player->player, 1.0, GST_FORMAT_TIME,
      SEEK_FLAGS,
      GST_SEEK_TYPE_SET, (gint64)(v * GST_SECOND),
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  if (!res) {
    g_print("seek failed\n");
  }
  do_timeout (video_player);
}

static void
set_button_image (GtkWidget *button, const char *stock_id)
{
  GtkWidget *image;
  GtkWidget *old_image;

  g_print("setting to %s\n", stock_id);

  old_image = gtk_bin_get_child (GTK_BIN(button));
  if (old_image) {
    gtk_container_remove (GTK_CONTAINER(button), old_image);
  }

  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER(button), image);
}

static void
on_message (GstBus *bus, GstMessage *message, GtkVideoPlayer *video_player)
{
  const GstStructure *structure;

  g_return_if_fail (GST_IS_BUS (bus));
  g_return_if_fail (GTK_IS_VIDEO_PLAYER(video_player));

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
      g_signal_emit (video_player, gtk_video_player_signals[ERROR], 0);
      break;
    case GST_MESSAGE_EOS:
      g_signal_emit (video_player, gtk_video_player_signals[END_OF_STREAM], 0);
      break;
    case GST_MESSAGE_APPLICATION:
      g_print("APP message bus %p\n", bus);
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if ((void*)(message->src) == video_player->player) {
        GstState oldstate;
        GstState newstate;
        GstState pending;

        gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);

        if (newstate == GST_STATE_PAUSED && video_player->duration == 0.0) {
          gint64 duration;
          GstFormat format;

          format = GST_FORMAT_TIME;
          gst_element_query_duration (GST_ELEMENT(video_player->player),
              &format, &duration);
          if (duration != GST_CLOCK_TIME_NONE) {
            video_player->duration = duration / (double)GST_SECOND;
            gtk_range_set_range (GTK_RANGE(video_player->scale), 0,
                  video_player->duration);
            GST_ERROR("duration %g\n", video_player->duration);
          } else {
            GST_ERROR("unknown duration\n");
            video_player->duration = 0.0;
            gtk_range_set_range (GTK_RANGE(video_player->scale), 0, 1.0);
          }
        }
      }
      break;
    default:
      break;
  }

}

static void
on_end_of_stream (GtkVideoPlayer *video_player)
{
  set_button_image (video_player->button, GTK_STOCK_MEDIA_REWIND);

  video_player->eos = TRUE;
}

static void
on_error (GtkVideoPlayer *video_player)
{
  GtkWidget *dialog;

  set_button_image (video_player->button, GTK_STOCK_MEDIA_PLAY);
  video_player->playing = FALSE;
  //video_player->have_media = FALSE;

  dialog = gtk_dialog_new_with_buttons ("Message",
      GTK_WINDOW(gtk_widget_get_toplevel (GTK_WIDGET(video_player))),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_STOCK_OK,
      GTK_RESPONSE_NONE,
      NULL);

  gtk_container_add (
      GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
      gtk_label_new ("Could not play media file"));

  g_signal_connect_swapped (dialog,
      "response", 
      G_CALLBACK (gtk_widget_destroy),
      dialog);

  gtk_widget_show_all (dialog);

  gtk_dialog_run (GTK_DIALOG(dialog));
}



#ifndef __GTK_VIDEO_PLAYER_H__
#define __GTK_VIDEO_PLAYER_H__

#include <gtk/gtk.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GTK_TYPE_VIDEO_PLAYER         (gtk_video_player_get_type ())
#define GTK_VIDEO_PLAYER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_VIDEO_PLAYER, GtkVideoPlayer))
#define GTK_VIDEO_PLAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_VIDEO_PLAYER, GtkVideoPlayerClass))
#define GTK_IS_VIDEO_PLAYER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIDEO_PLAYER))
#define GTK_IS_VIDEO_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIDEO_PLAYER))
#define GTK_VIDEO_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIDEO_PLAYER, GtkVideoPlayerClass))


typedef struct _GtkVideoPlayer GtkVideoPlayer;
typedef struct _GtkVideoPlayerClass GtkVideoPlayerClass;

struct _GtkVideoPlayer {
  GtkVBox vbox;

  /*< private >*/
  GtkWidget *video_widget;
  GtkWidget *button;
  GtkWidget *scale;
  gboolean playing;
  gboolean eos;
  guint update_id;

  GstElement *player;
  GstBus *bus;
  double duration;
};

struct _GtkVideoPlayerClass {
  GtkVBoxClass parent_class;

  void (*end_of_stream) (GtkVideoPlayer *player);
  void (*error) (GtkVideoPlayer *player);

};

GType gtk_video_player_get_type (void) G_GNUC_CONST;
GtkWidget *gtk_video_player_new (void);

void gtk_video_player_set_uri (GtkVideoPlayer *video_player, const char *uri);
void gtk_video_player_set_file (GtkVideoPlayer *video_player, const char *file);
void gtk_video_player_seek (GtkVideoPlayer *video_player, gdouble position);

G_END_DECLS

#endif


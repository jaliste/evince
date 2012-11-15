#include <config.h>
#include "ev-pixbuf-cache.h"
#include "ev-job-scheduler.h"
#include "ev-view-private.h"

typedef struct _CacheJobInfo
{
	EvJob *job;
	EvJobPriority priority;
	gboolean page_ready;

	/* Region of the page that needs to be drawn */
	cairo_region_t  *region;

	/* Data we get from rendering */
	cairo_surface_t *surface;

	/* Selection data.
	 * Selection_points are the coordinates encapsulated in selection.
	 * target_points is the target selection size. */
	EvRectangle      selection_points;
	EvRectangle      target_points;
	EvSelectionStyle selection_style;
	gboolean         points_set;

	cairo_surface_t *selection;
	cairo_region_t  *selection_region;
} CacheJobInfo;

struct _EvPixbufCache
{
	GObject parent;

	/* We keep a link to our containing view just for style information. */
	GtkWidget *view;
	EvDocument *document;
	EvDocumentModel *model;
	int start_page;
	int end_page;
	gboolean inverted_colors;

	gsize max_size;

	/* preload_cache_size is the number of pages prior to the current
	 * visible area that we cache.  It's normally 1, but could be 2 in the
	 * case of twin pages.
	 */
	int preload_cache_size;

	GHashTable *job_table;
};

struct _EvPixbufCacheClass
{
	GObjectClass parent_class;

	void (* job_finished) (EvPixbufCache *pixbuf_cache);
};


enum
{
	JOB_FINISHED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS] = {0, };

static void          ev_pixbuf_cache_init       (EvPixbufCache      *pixbuf_cache);
static void          ev_pixbuf_cache_class_init (EvPixbufCacheClass *pixbuf_cache);
static void          ev_pixbuf_cache_finalize   (GObject            *object);
static void          ev_pixbuf_cache_dispose    (GObject            *object);
static void          job_finished_cb            (EvJob              *job,
						 EvPixbufCache      *pixbuf_cache);
static CacheJobInfo *find_job_cache             (EvPixbufCache      *pixbuf_cache,
						 int                 page);
static gboolean      new_selection_surface_needed(EvPixbufCache      *pixbuf_cache,
						  CacheJobInfo       *job_info,
						  gint                page,
						  gfloat              scale);


/* These are used for iterating through the prev and next arrays */
#define FIRST_VISIBLE_PREV(pixbuf_cache) \
	(MAX (0, pixbuf_cache->preload_cache_size - pixbuf_cache->start_page))
#define VISIBLE_NEXT_LEN(pixbuf_cache) \
	(MIN(pixbuf_cache->preload_cache_size, ev_document_get_n_pages (pixbuf_cache->document) - (1 + pixbuf_cache->end_page)))
#define PAGE_CACHE_LEN(pixbuf_cache) \
	((pixbuf_cache->end_page - pixbuf_cache->start_page) + 1)

#define MAX_PRELOADED_PAGES 3

G_DEFINE_TYPE (EvPixbufCache, ev_pixbuf_cache, G_TYPE_OBJECT)

static void job_table_key_destroy (gpointer data)
{
	gint *page = (gint *) data;

	g_free (page);
}

static void job_table_value_destroy (gpointer data)
{
	g_slice_free (CacheJobInfo, data);
}

static void
ev_pixbuf_cache_init (EvPixbufCache *pixbuf_cache)
{
	pixbuf_cache->start_page = -1;
	pixbuf_cache->end_page = -1;

	pixbuf_cache->job_table = g_hash_table_new_full (g_int_hash, g_int_equal, job_table_key_destroy, job_table_value_destroy);
}

static void
ev_pixbuf_cache_class_init (EvPixbufCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_pixbuf_cache_finalize;
	object_class->dispose = ev_pixbuf_cache_dispose;

	signals[JOB_FINISHED] =
		g_signal_new ("job-finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPixbufCacheClass, job_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
ev_pixbuf_cache_finalize (GObject *object)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = EV_PIXBUF_CACHE (object);

	ev_pixbuf_cache_clear (pixbuf_cache);
	g_hash_table_destroy (pixbuf_cache->job_table);

	g_object_unref (pixbuf_cache->model);

	G_OBJECT_CLASS (ev_pixbuf_cache_parent_class)->finalize (object);
}

static void
dispose_cache_job_info (CacheJobInfo *job_info,
			gpointer      data)
{
	if (job_info == NULL)
		return;

	if (job_info->job) {
		g_signal_handlers_disconnect_by_func (job_info->job,
						      G_CALLBACK (job_finished_cb),
						      data);
		ev_job_cancel (job_info->job);
		g_object_unref (job_info->job);
		job_info->job = NULL;
	}
	if (job_info->surface) {
		cairo_surface_destroy (job_info->surface);
		job_info->surface = NULL;
	}
	if (job_info->region) {
		cairo_region_destroy (job_info->region);
		job_info->region = NULL;
	}
	if (job_info->selection) {
		cairo_surface_destroy (job_info->selection);
		job_info->selection = NULL;
	}
	if (job_info->selection_region) {
		cairo_region_destroy (job_info->selection_region);
		job_info->selection_region = NULL;
	}

	job_info->points_set = FALSE;
	job_info = NULL;
}

static void
destroy_job_table_value (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
	dispose_cache_job_info ((CacheJobInfo *) value, EV_PIXBUF_CACHE (user_data));
}

static void
ev_pixbuf_cache_dispose (GObject *object)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = EV_PIXBUF_CACHE (object);
	g_hash_table_foreach (pixbuf_cache->job_table, destroy_job_table_value, pixbuf_cache);

	G_OBJECT_CLASS (ev_pixbuf_cache_parent_class)->dispose (object);
}


EvPixbufCache *
ev_pixbuf_cache_new (GtkWidget       *view,
		     EvDocumentModel *model,
		     gsize            max_size)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = (EvPixbufCache *) g_object_new (EV_TYPE_PIXBUF_CACHE, NULL);
	/* This is a backlink, so we don't ref this */
	pixbuf_cache->view = view;
	pixbuf_cache->model = g_object_ref (model);
	pixbuf_cache->document = ev_document_model_get_document (model);
	pixbuf_cache->max_size = max_size;

	return pixbuf_cache;
}

void
ev_pixbuf_cache_set_max_size (EvPixbufCache *pixbuf_cache,
			      gsize          max_size)
{
	if (pixbuf_cache->max_size == max_size)
		return;

	if (pixbuf_cache->max_size > max_size)
		ev_pixbuf_cache_clear (pixbuf_cache);
	pixbuf_cache->max_size = max_size;
}

static void
copy_job_to_job_info (EvJobRender   *job_render,
		      CacheJobInfo  *job_info,
		      EvPixbufCache *pixbuf_cache)
{
	g_assert (job_info != NULL);

	if (job_info->surface) {
		cairo_surface_destroy (job_info->surface);
	}
	job_info->surface = cairo_surface_reference (job_render->surface);
	if (pixbuf_cache->inverted_colors) {
		ev_document_misc_invert_surface (job_info->surface);
	}

	job_info->points_set = FALSE;
	if (job_render->include_selection) {
		if (job_info->selection) {
			cairo_surface_destroy (job_info->selection);
			job_info->selection = NULL;
		}
		if (job_info->selection_region) {
			cairo_region_destroy (job_info->selection_region);
			job_info->selection_region = NULL;
		}

		job_info->selection_points = job_render->selection_points;
		job_info->selection_region = cairo_region_reference (job_render->selection_region);
		job_info->selection = cairo_surface_reference (job_render->selection);
		g_assert (job_info->selection_points.x1 >= 0);
		job_info->points_set = TRUE;
	}

	if (job_info->job) {
		g_signal_handlers_disconnect_by_func (job_info->job,
						      G_CALLBACK (job_finished_cb),
						      pixbuf_cache);
		ev_job_cancel (job_info->job);
		g_object_unref (job_info->job);
		job_info->job = NULL;
	}

	job_info->page_ready = TRUE;
}

static void
job_finished_cb (EvJob         *job,
		 EvPixbufCache *pixbuf_cache)
{
	CacheJobInfo *job_info;
	EvJobRender *job_render = EV_JOB_RENDER (job);

	/* If the job is outside of our interest, we silently discard it */
	if ((job_render->page < (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size)) ||
	    (job_render->page > (pixbuf_cache->end_page + pixbuf_cache->preload_cache_size))) {
		g_object_unref (job);
		return;
	}

	job_info = find_job_cache (pixbuf_cache, job_render->page);
	if (job_info)
	{
		copy_job_to_job_info (job_render, job_info, pixbuf_cache);
		g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0, job_info->region);
	}
}

/* This checks a job to see if the job would generate the right sized pixbuf
 * given a scale.  If it won't, it removes the job and clears it to NULL.
 */

static void
check_job_size_and_unref (EvPixbufCache *pixbuf_cache,
                          CacheJobInfo  *job_info,
                          gfloat         scale)
{
	gint width, height;

	g_assert (job_info);

	if (job_info->job == NULL)
		return;

	_get_page_size_for_scale_and_rotation (job_info->job->document,
					       EV_JOB_RENDER (job_info->job)->page,
					       scale,
					       EV_JOB_RENDER (job_info->job)->rotation,
					       &width, &height);
	if (width == EV_JOB_RENDER (job_info->job)->target_width &&
	    height == EV_JOB_RENDER (job_info->job)->target_height)
		return;

	g_signal_handlers_disconnect_by_func (job_info->job,
					      G_CALLBACK (job_finished_cb),
					      pixbuf_cache);
	ev_job_cancel (job_info->job);
	g_object_unref (job_info->job);
	job_info->job = NULL;
}

/* Do all function that copies a job from an older cache to it's position in the
 * new cache.  It clears the old job if it doesn't have a place.
 */
static void
update_job_priority (CacheJobInfo *job_info,
		     int	   page,
		     int 	   new_preload_cache_size,
		     int 	   start_page,
		     int	   end_page)
{
	/* Assume you run this only for jobs we are keeping in the cache */
	gint new_priority;
	gint page_offset;

	if (page < start_page) {
                page_offset = (page - (start_page - new_preload_cache_size));

                g_assert (page_offset >= 0 &&
                          page_offset < new_preload_cache_size);
                new_priority = EV_JOB_PRIORITY_LOW;
        } else if (page > end_page) {
                page_offset = (page - (end_page + 1));

                g_assert (page_offset >= 0 &&
                          page_offset < new_preload_cache_size);
                new_priority = EV_JOB_PRIORITY_LOW;
        } else {
                page_offset = page - start_page;
                g_assert (page_offset >= 0 &&
                          page_offset <= ((end_page - start_page) + 1));
                new_priority = EV_JOB_PRIORITY_URGENT;
        }

	if (new_priority != job_info->priority && job_info->job) {
		// SHOULD UPDATE PRIORITY IF job_info->job is NULL???
		job_info->priority = new_priority;
                ev_job_scheduler_update_job (job_info->job, new_priority);
        }
}

static gsize
ev_pixbuf_cache_get_page_size (EvPixbufCache *pixbuf_cache,
			       gint           page_index,
			       gdouble        scale,
			       gint           rotation)
{
	gint width, height;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page_index, scale, rotation,
					       &width, &height);
	return height * cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);
}

static gint
ev_pixbuf_cache_get_preload_size (EvPixbufCache *pixbuf_cache,
				  gint           start_page,
				  gint           end_page,
				  gdouble        scale,
				  gint           rotation)
{
	gsize range_size = 0;
	gint  new_preload_cache_size = 0;
	gint  i;
	guint n_pages = ev_document_get_n_pages (pixbuf_cache->document);

	/* Get the size of the current range */
	for (i = start_page; i <= end_page; i++) {
		range_size += ev_pixbuf_cache_get_page_size (pixbuf_cache, i, scale, rotation);
	}

	if (range_size >= pixbuf_cache->max_size)
		return new_preload_cache_size;

	i = 1;
	while (((start_page - i > 0) || (end_page + i < n_pages)) &&
	       new_preload_cache_size < MAX_PRELOADED_PAGES) {
		gsize    page_size;
		gboolean updated = FALSE;

		if (end_page + i < n_pages) {
			page_size = ev_pixbuf_cache_get_page_size (pixbuf_cache, end_page + i,
								   scale, rotation);
			if (page_size + range_size <= pixbuf_cache->max_size) {
				range_size += page_size;
				new_preload_cache_size++;
				updated = TRUE;
			} else {
				break;
			}
		}

		if (start_page - i > 0) {
			page_size = ev_pixbuf_cache_get_page_size (pixbuf_cache, start_page - i,
								   scale, rotation);
			if (page_size + range_size <= pixbuf_cache->max_size) {
				range_size += page_size;
				if (!updated)
					new_preload_cache_size++;
			} else {
				break;
			}
		}
		i++;
	}

	return new_preload_cache_size;
}

static void
ev_pixbuf_cache_update_range (EvPixbufCache *pixbuf_cache,
			      gint           start_page,
			      gint           end_page,
			      guint          rotation,
			      gdouble        scale)
{
	gint          new_preload_cache_size;
	GHashTableIter iter;
	gpointer key, value;
	gint real_start, real_end, page;

	new_preload_cache_size = ev_pixbuf_cache_get_preload_size (pixbuf_cache,
								   start_page,
								   end_page,
								   scale,
								   rotation);
	if (pixbuf_cache->start_page == start_page &&
	    pixbuf_cache->end_page == end_page &&
	    pixbuf_cache->preload_cache_size == new_preload_cache_size)
		return;

	/* We go through each job in the cache and either clear it, and remove it from the
 	 * cache or update its priority */

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
	while (g_hash_table_iter_next (&iter, &key, &value))
  	{
		gint page = *((gint *)key);
		CacheJobInfo *job_info = (CacheJobInfo *) value;

		if (page < 0 || page < (start_page - new_preload_cache_size) ||
          		  			page > (end_page + new_preload_cache_size)) {

			dispose_cache_job_info (job_info, pixbuf_cache);
			g_hash_table_iter_remove (&iter);
        	} else {
			update_job_priority (job_info, page, new_preload_cache_size, start_page, end_page);
		}
	}

	/* We go through all pages in the cache range to add Caches for the pages that don't have
 	 * a cache. TODO: Possibly change the way we check for pages without cache */

	real_start = MAX (0, start_page - new_preload_cache_size);
	real_end = MIN (end_page + new_preload_cache_size, ev_document_get_n_pages (pixbuf_cache->document));

	for (page = real_start; page <= real_end; ++page)
	{
		CacheJobInfo *job_info;

		job_info = find_job_cache (pixbuf_cache, page);

		if (!job_info) {
                        gint *key = g_new (gint,1);

			job_info  = g_slice_new0 (CacheJobInfo);
			*key = page;

			g_hash_table_insert (pixbuf_cache->job_table, key, job_info);
		}
	}

	pixbuf_cache->preload_cache_size = new_preload_cache_size;
	pixbuf_cache->start_page = start_page;
	pixbuf_cache->end_page = end_page;
}

static CacheJobInfo *
find_job_cache (EvPixbufCache *pixbuf_cache,
		int            page)
{
	CacheJobInfo *job_info;

	job_info = (CacheJobInfo *) g_hash_table_lookup (pixbuf_cache->job_table, &page);

	return job_info;
}

static void
ev_pixbuf_cache_clear_job_sizes (EvPixbufCache *pixbuf_cache,
				 gfloat         scale)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		check_job_size_and_unref (pixbuf_cache, (CacheJobInfo *) value, scale);
	}
}

static void
get_selection_colors (GtkWidget *widget, GdkColor *text, GdkColor *base)
{
	GtkStyleContext *context = gtk_widget_get_style_context (widget);
        GtkStateFlags    state = 0;
        GdkRGBA          fg, bg;

        state |= gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE;

        gtk_style_context_get_color (context, state, &fg);
        text->pixel = 0;
        text->red = CLAMP ((guint) (fg.red * 65535), 0, 65535);
        text->green = CLAMP ((guint) (fg.green * 65535), 0, 65535);
        text->blue = CLAMP ((guint) (fg.blue * 65535), 0, 65535);

        gtk_style_context_get_background_color (context, state, &bg);
        base->pixel = 0;
        base->red = CLAMP ((guint) (bg.red * 65535), 0, 65535);
        base->green = CLAMP ((guint) (bg.green * 65535), 0, 65535);
        base->blue = CLAMP ((guint) (bg.blue * 65535), 0, 65535);
}

static void
add_job (EvPixbufCache  *pixbuf_cache,
	 CacheJobInfo   *job_info,
	 cairo_region_t *region,
	 gint            width,
	 gint            height,
	 gint            page,
	 gint            rotation,
	 gfloat          scale,
	 EvJobPriority   priority)
{
	g_assert (job_info != NULL);

	job_info->page_ready = FALSE;

	if (job_info->region)
		cairo_region_destroy (job_info->region);
	job_info->region = region ? cairo_region_reference (region) : NULL;

	job_info->job = ev_job_render_new (pixbuf_cache->document,
					   page, rotation, scale,
					   width, height);

	if (new_selection_surface_needed (pixbuf_cache, job_info, page, scale)) {
		GdkColor text, base;

		get_selection_colors (pixbuf_cache->view, &text, &base);
		ev_job_render_set_selection_info (EV_JOB_RENDER (job_info->job),
						  &(job_info->target_points),
						  job_info->selection_style,
						  &text, &base);
	}

	g_signal_connect (job_info->job, "finished",
			  G_CALLBACK (job_finished_cb),
			  pixbuf_cache);
	ev_job_scheduler_push_job (job_info->job, priority);
	job_info->priority = priority;
}

static void
add_job_if_needed (EvPixbufCache *pixbuf_cache,
		   CacheJobInfo  *job_info,
		   gint           page,
		   gint           rotation,
		   gfloat         scale,
		   EvJobPriority  priority)
{
	gint width, height;

	if (job_info->job)
		return;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page, scale, rotation,
					       &width, &height);

	if (job_info->surface &&
	    cairo_image_surface_get_width (job_info->surface) == width &&
	    cairo_image_surface_get_height (job_info->surface) == height)
		return;

	/* Free old surfaces for non visible pages */
	if (priority == EV_JOB_PRIORITY_LOW) {
		if (job_info->surface) {
			cairo_surface_destroy (job_info->surface);
			job_info->surface = NULL;
		}

		if (job_info->selection) {
			cairo_surface_destroy (job_info->selection);
			job_info->selection = NULL;
		}
	}

	add_job (pixbuf_cache, job_info, NULL,
		 width, height, page, rotation, scale,
		 priority);
}

static void
ev_pixbuf_cache_add_jobs_if_needed (EvPixbufCache *pixbuf_cache,
				    gint           rotation,
				    gfloat         scale)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CacheJobInfo *job_info = (CacheJobInfo *)value;
		gint page = *((gint *) key);

		add_job_if_needed (pixbuf_cache, job_info,
				   page, rotation, scale,
				   EV_JOB_PRIORITY_URGENT);
	}
}

void
ev_pixbuf_cache_set_page_range (EvPixbufCache  *pixbuf_cache,
				gint            start_page,
				gint            end_page,
				GList          *selection_list)
{
	gdouble scale = ev_document_model_get_scale (pixbuf_cache->model);
	gint    rotation = ev_document_model_get_rotation (pixbuf_cache->model);

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	g_return_if_fail (start_page >= 0 && start_page < ev_document_get_n_pages (pixbuf_cache->document));
	g_return_if_fail (end_page >= 0 && end_page < ev_document_get_n_pages (pixbuf_cache->document));
	g_return_if_fail (end_page >= start_page);

	/* First, resize the page_range as needed.  We cull old pages
	 * mercilessly. */
	ev_pixbuf_cache_update_range (pixbuf_cache, start_page, end_page, rotation, scale);

	/* Then, we update the current jobs to see if any of them are the wrong
	 * size, we remove them if we need to. */
	ev_pixbuf_cache_clear_job_sizes (pixbuf_cache, scale);

	/* Next, we update the target selection for our pages */
//	ev_pixbuf_cache_set_selection_list (pixbuf_cache, selection_list);

	/* Finally, we add the new jobs for all the sizes that don't have a
	 * pixbuf */
	ev_pixbuf_cache_add_jobs_if_needed (pixbuf_cache, rotation, scale);
}

void
ev_pixbuf_cache_set_inverted_colors (EvPixbufCache *pixbuf_cache,
				     gboolean       inverted_colors)
{
	GHashTableIter iter;
	gpointer key, value;

	if (pixbuf_cache->inverted_colors == inverted_colors)
		return;

	pixbuf_cache->inverted_colors = inverted_colors;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CacheJobInfo *job_info = (CacheJobInfo *)value;

		if (job_info && job_info->surface)
			ev_document_misc_invert_surface (job_info->surface);
	}
}

cairo_surface_t *
ev_pixbuf_cache_get_surface (EvPixbufCache *pixbuf_cache,
			     gint           page)
{
	CacheJobInfo *job_info;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	if (job_info->page_ready)
		return job_info->surface;

	/* We don't need to wait for the idle to handle the callback */
	if (job_info->job &&
	    EV_JOB_RENDER (job_info->job)->page_ready) {
		copy_job_to_job_info (EV_JOB_RENDER (job_info->job), job_info, pixbuf_cache);
		g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0, job_info->region);
	}

	return job_info->surface;
}

static gboolean
new_selection_surface_needed (EvPixbufCache *pixbuf_cache,
			      CacheJobInfo  *job_info,
			      gint           page,
			      gfloat         scale)
{
	if (job_info->selection) {
		gint width, height;
		gint selection_width, selection_height;

		_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
						       page, scale, 0,
						       &width, &height);

		selection_width = cairo_image_surface_get_width (job_info->selection);
		selection_height = cairo_image_surface_get_height (job_info->selection);

		if (width != selection_width || height != selection_height)
			return TRUE;
	} else {
		if (job_info->points_set)
			return TRUE;
	}

	return FALSE;
}

static void
clear_selection_if_needed (EvPixbufCache *pixbuf_cache,
			   CacheJobInfo  *job_info,
			   gint           page,
			   gfloat         scale)
{
	if (new_selection_surface_needed (pixbuf_cache, job_info, page, scale)) {
		if (job_info->selection)
			cairo_surface_destroy (job_info->selection);
		job_info->selection = NULL;
		job_info->selection_points.x1 = -1;
	}
}

/* Clears the cache of jobs and pixbufs.
 */
void
ev_pixbuf_cache_clear (EvPixbufCache *pixbuf_cache)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		dispose_cache_job_info ((CacheJobInfo *)value, pixbuf_cache);
		g_hash_table_iter_remove (&iter);
	}
}


void
ev_pixbuf_cache_style_changed (EvPixbufCache *pixbuf_cache)
{
	/* FIXME: doesn't update running jobs. */
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CacheJobInfo *job_info = (CacheJobInfo *)value;

		if (job_info->selection) {
			cairo_surface_destroy (job_info->selection);
			job_info->selection = NULL;
		}
	}
}

cairo_surface_t *
ev_pixbuf_cache_get_selection_surface (EvPixbufCache   *pixbuf_cache,
				       gint             page,
				       gfloat           scale,
				       cairo_region_t **region)
{
	CacheJobInfo *job_info;

	/* the document does not implement the selection interface */
	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return NULL;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* No selection on this page */
	if (!job_info->points_set)
		return NULL;

	/* If we have a running job, we just return what we have under the
	 * assumption that it'll be updated later and we can scale it as need
	 * be */
	if (job_info->job && EV_JOB_RENDER (job_info->job)->include_selection)
		return job_info->selection;

	/* Now, lets see if we need to resize the image.  If we do, we clear the
	 * old one. */
	clear_selection_if_needed (pixbuf_cache, job_info, page, scale);

	/* Finally, we see if the two scales are the same, and get a new pixbuf
	 * if needed.  We do this synchronously for now.  At some point, we
	 * _should_ be able to get rid of the doc_mutex, so the synchronicity
	 * doesn't kill us.  Rendering a few glyphs should really be fast.
	 */
	if (ev_rect_cmp (&(job_info->target_points), &(job_info->selection_points))) {
		EvRectangle *old_points;
		GdkColor text, base;
		EvRenderContext *rc;
		EvPage *ev_page;

		/* we need to get a new selection pixbuf */
		ev_document_doc_mutex_lock ();
		if (job_info->selection_points.x1 < 0) {
			g_assert (job_info->selection == NULL);
			old_points = NULL;
		} else {
			g_assert (job_info->selection != NULL);
			old_points = &(job_info->selection_points);
		}

		ev_page = ev_document_get_page (pixbuf_cache->document, page);
		rc = ev_render_context_new (ev_page, 0, scale);
		g_object_unref (ev_page);

		if (job_info->selection_region)
			cairo_region_destroy (job_info->selection_region);
		job_info->selection_region =
			ev_selection_get_selection_region (EV_SELECTION (pixbuf_cache->document),
							   rc, job_info->selection_style,
							   &(job_info->target_points));

		get_selection_colors (pixbuf_cache->view, &text, &base);

		ev_selection_render_selection (EV_SELECTION (pixbuf_cache->document),
					       rc, &(job_info->selection),
					       &(job_info->target_points),
					       old_points,
					       job_info->selection_style,
					       &text, &base);
		job_info->selection_points = job_info->target_points;
		g_object_unref (rc);
		ev_document_doc_mutex_unlock ();
	}
	if (region)
		*region = job_info->selection_region;
	return job_info->selection;
}

static void
update_job_selection (CacheJobInfo    *job_info,
		      EvViewSelection *selection)
{
	job_info->points_set = TRUE;
	job_info->target_points = selection->rect;
	job_info->selection_style = selection->style;
}

static void
clear_job_selection (CacheJobInfo *job_info)
{
	job_info->points_set = FALSE;
	job_info->selection_points.x1 = -1;

	if (job_info->selection) {
		cairo_surface_destroy (job_info->selection);
		job_info->selection = NULL;
	}
}

/* This function will reset the selection on pages that no longer have them, and
 * will update the target_selection on those that need it.  It will _not_ free
 * the previous selection_list -- that's up to caller to do.
 */
void
ev_pixbuf_cache_set_selection_list (EvPixbufCache *pixbuf_cache,
				    GList         *selection_list)
{

	EvViewSelection *selection;
	GList *list = selection_list;

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return;

        if (pixbuf_cache->start_page == -1 || pixbuf_cache->end_page == -1)
                return;

	/* We check each area to see what needs updating, and what needs freeing; */

	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gint page = *((gint *)key);
		CacheJobInfo *job_info = (CacheJobInfo *) value;

		if (page < 0) {
			continue;
		}

		selection = NULL;
		while (list) {
			if (((EvViewSelection *)list->data)->page == page) {
				selection = list->data;
				break;
			} else if (((EvViewSelection *)list->data)->page > page)
				break;
			list = list->next;
		}

		if (selection)
			update_job_selection (job_info, selection);
		else
			clear_job_selection (job_info);
	}
}

/* Returns what the pixbuf cache thinks is */

GList *
ev_pixbuf_cache_get_selection_list (EvPixbufCache *pixbuf_cache)
{
	EvViewSelection *selection;
	GList *retval = NULL;

	g_return_val_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache), NULL);

        if (pixbuf_cache->start_page == -1 || pixbuf_cache->end_page == -1)
                return NULL;

	/* We check each area to see what needs updating, and what needs freeing; */
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, pixbuf_cache->job_table);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
                gint page = *((gint *)key);
                CacheJobInfo *job_info = (CacheJobInfo *) value;

		if (page < 0) {
			continue;
		}

		if (job_info->selection_points.x1 != -1) {
			selection = g_new0 (EvViewSelection, 1);
			selection->page = page;
			selection->rect = job_info->selection_points;
			if (job_info->selection_region)
				selection->covered_region = cairo_region_reference (job_info->selection_region);
			retval = g_list_append (retval, selection);
		}
	}

	return retval;
}

void
ev_pixbuf_cache_reload_page (EvPixbufCache  *pixbuf_cache,
			     cairo_region_t *region,
			     gint            page,
			     gint            rotation,
			     gdouble         scale)
{
	CacheJobInfo *job_info;
        gint width, height;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page, scale, rotation,
					       &width, &height);
        add_job (pixbuf_cache, job_info, region,
		 width, height, page, rotation, scale,
		 EV_JOB_PRIORITY_URGENT);
}



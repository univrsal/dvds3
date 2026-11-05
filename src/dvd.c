/* This file is part of dvds3
 * DVD screen saver source
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * github.com/univrsal/dvds3
 */

#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/dstr.h>
#include <util/threading.h>

/* OBS Studio 33 replaced the gs_image_file functions with gs_image_file_ex */
#if LIBOBS_API_MAJOR_VER >= 33
typedef gs_image_file_ex_t dvd_image_t;
#define dvd_image_init(img, file) \
	gs_image_file_ex_init(img, file, GS_IMAGE_ALPHA_PREMULTIPLY)
#define dvd_image_init_texture gs_image_file_ex_init_texture
#define dvd_image_free gs_image_file_ex_free
#define dvd_image_get(img) (img)
#elif defined(_WIN32)
typedef gs_image_file_t dvd_image_t;
#define dvd_image_init gs_image_file_init
#define dvd_image_init_texture gs_image_file_init_texture
#define dvd_image_free gs_image_file_free
#define dvd_image_get(img) (img)
#else
/* Built against OBS < 33 headers (e.g. the Ubuntu PPA), but the plugin may
 * run on OBS 33+ where the gs_image_file functions were removed. Both the old
 * and the new struct start with the same fields (texture, cx, cy, loaded...)
 * and gs_image_file4 is larger than gs_image_file_ex, so we allocate that and
 * look up the functions at runtime. Nothing may reference the removed symbols
 * directly, or the plugin fails to load. */
#include <dlfcn.h>

typedef gs_image_file4_t dvd_image_t;
#define dvd_image_get(img) (&(img)->image3.image2.image)

static struct {
	bool looked_up;
	void (*init_ex)(void *, const char *, enum gs_image_alpha_mode);
	void (*init)(void *, const char *);
	void (*init_texture)(void *);
	void (*free)(void *);
} image_api;

static void image_api_lookup(void)
{
	void *obs;

	if (image_api.looked_up)
		return;
	image_api.looked_up = true;

	/* The global symbol scope, which includes libobs. RTLD_DEFAULT would
	 * require _GNU_SOURCE on glibc */
	obs = dlopen(NULL, RTLD_LAZY);
	image_api.init_ex = dlsym(obs, "gs_image_file_ex_init");
	if (image_api.init_ex) {
		image_api.init_texture =
			dlsym(obs, "gs_image_file_ex_init_texture");
		image_api.free = dlsym(obs, "gs_image_file_ex_free");
	} else {
		image_api.init = dlsym(obs, "gs_image_file_init");
		image_api.init_texture =
			dlsym(obs, "gs_image_file_init_texture");
		image_api.free = dlsym(obs, "gs_image_file_free");
	}
}

static void dvd_image_init(dvd_image_t *img, const char *file)
{
	image_api_lookup();
	if (image_api.init_ex)
		image_api.init_ex(img, file, GS_IMAGE_ALPHA_PREMULTIPLY);
	else
		image_api.init(img, file);
}

static void dvd_image_init_texture(dvd_image_t *img)
{
	image_api.init_texture(img);
}

static void dvd_image_free(dvd_image_t *img)
{
	image_api.free(img);
}
#endif

/* Reads the size of an image file, returns false if it couldn't be loaded */
static bool get_image_size(const char *file, uint32_t *cx, uint32_t *cy)
{
	dvd_image_t img;
	bool loaded;

	*cx = 0;
	*cy = 0;
	if (!file || !*file)
		return false;

	/* The size is known after decoding, no need to upload a texture */
	memset(&img, 0, sizeof(img));
	dvd_image_init(&img, file);
	loaded = dvd_image_get(&img)->loaded;
	if (loaded) {
		*cx = dvd_image_get(&img)->cx;
		*cy = dvd_image_get(&img)->cy;
	}

	obs_enter_graphics();
	dvd_image_free(&img);
	obs_leave_graphics();
	return loaded;
}

#define blog(log_level, format, ...)                  \
	blog(log_level, "[dvd_source: '%s'] " format, \
	     obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

#define FILTER_NAME "dvd-filter"
#define SOURCE_NAME "dvd-image"
#define DVD_SOURCE_ID "dvds3_source"

#define S_SOURCE_CX "source_cx"
#define S_SOURCE_CY "source_cy"
#define S_SPEED "speed"
#define S_SCALE "logo_scale"
#define S_COLOR "color_shift"
#define S_RESET "reset"
#define S_SOURCE_ID "source_id"
#define S_SOURCE_IMAGE "image_source"
#define S_FILE "file"
#define S_HUE_SHIFT "hue_shift"

#define _T(t) obs_module_text(t)
#define T_SOURCE_CX _T("DVDSource.Width")
#define T_SOURCE_CY _T("DVDSource.Height")
#define T_SOURCE _T("DVDSource.Name")
#define T_SPEED _T("DVDSource.Speed")
#define T_SCALE _T("DVDSource.Scale")
#define T_COLOR _T("DVDSource.Color")
#define T_RESET _T("DVDSource.Reset")
#define T_SOURCE_ID _T("DVDSource.Source")
#define T_SOURCE_IMAGE _T("DVDSource.Source.Image")
#define T_EXTERNAL_SOURCE _T("DVDSource.ExternalSource")

struct dvd_source {
	obs_source_t *source;
	obs_source_t *logo_source;
	obs_source_t *color_filter;

	/* External source that is shown instead of the image. Written by the
	 * UI thread and read by the graphics thread, so it's guarded by
	 * other_mutex. */
	obs_weak_source_t *other_source;
	pthread_mutex_t other_mutex;

	bool use_image;          /* False for custom src		*/
	bool use_color;          /* Enable color filter		*/
	uint32_t cx, cy;         /* Source size			*/
	uint32_t img_cx, img_cy; /* Scaled size of the shown source	*/
	struct vec2 pos;         /* Image position		*/
	int8_t d_x, d_y;         /* Movement vector		*/
	double scale, speed;     /* Image scale/speed		*/
	double hue_shift;        /* Current color filter hue shift	*/
};

struct enum_param {
	obs_property_t *list;
	obs_source_t *this;
};

static const char *dvd_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_SOURCE;
}

/* Random number in [0, limit] */
static int rand_limit(int limit)
{
	if (limit <= 0)
		return 0;
	if (limit >= RAND_MAX)
		limit = RAND_MAX - 1;

	int divisor = RAND_MAX / (limit + 1);
	int retval;

	do {
		retval = rand() / divisor;
	} while (retval > limit);

	return retval;
}

/* Returns a new reference to the external source, if there is one */
static obs_source_t *get_other_source(struct dvd_source *context)
{
	obs_source_t *target;

	pthread_mutex_lock(&context->other_mutex);
	target = obs_weak_source_get_source(context->other_source);
	pthread_mutex_unlock(&context->other_mutex);
	return target;
}

/* The color filter can only have one parent, so it's moved to whichever
 * source is currently shown */
static void move_color_filter(struct dvd_source *context, obs_source_t *to)
{
	if (!context->color_filter)
		return;

	obs_source_t *parent = obs_filter_get_parent(context->color_filter);
	if (parent == to)
		return;
	if (parent)
		obs_source_filter_remove(parent, context->color_filter);
	obs_source_filter_add(to, context->color_filter);
}

static void remove_source(struct dvd_source *context)
{
	obs_weak_source_t *weak;

	pthread_mutex_lock(&context->other_mutex);
	weak = context->other_source;
	context->other_source = NULL;
	pthread_mutex_unlock(&context->other_mutex);

	obs_source_t *target = obs_weak_source_get_source(weak);
	move_color_filter(context, context->logo_source);
	if (target) {
		obs_source_remove_active_child(context->source, target);
		obs_source_release(target);
	}
	obs_weak_source_release(weak);
}

static bool add_source(struct dvd_source *context, const char *name)
{
	bool result = false;
	obs_source_t *target = obs_get_source_by_name(name);

	if (target) {
		/* Fails if the source contains this source */
		if (obs_source_add_active_child(context->source, target)) {
			move_color_filter(context, target);

			obs_weak_source_t *weak =
				obs_source_get_weak_source(target);
			pthread_mutex_lock(&context->other_mutex);
			context->other_source = weak;
			pthread_mutex_unlock(&context->other_mutex);
			result = true;
		} else {
			warn("Can't add %s, as it would cause recursion", name);
		}
		obs_source_release(target);
	}
	return result;
}

static void dvd_source_update(void *data, obs_data_t *settings)
{
	struct dvd_source *context = data;

	context->cx = (uint32_t)obs_data_get_int(settings, S_SOURCE_CX);
	context->cy = (uint32_t)obs_data_get_int(settings, S_SOURCE_CY);
	context->scale = obs_data_get_double(settings, S_SCALE);
	context->speed = obs_data_get_double(settings, S_SPEED);
	context->use_color = obs_data_get_bool(settings, S_COLOR);

#ifndef MAC_OS
	obs_source_set_enabled(context->color_filter, context->use_color);
#endif

	obs_source_update(context->logo_source, settings);

	const char *id = obs_data_get_string(settings, S_SOURCE_ID);
	context->use_image = strcmp(id, S_SOURCE_IMAGE) == 0;

	remove_source(context);
	if (!context->use_image && *id) {
		/* id is postfixed with 'e' so get rid of that */
		char *name = bstrdup(id);
		name[strlen(name) - 1] = '\0';
		add_source(context, name);
		bfree(name);
	}
}

static void dvd_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_SOURCE_CX, 640);
	obs_data_set_default_int(settings, S_SOURCE_CY, 480);
	obs_data_set_default_double(settings, S_SCALE, 1.0);
	obs_data_set_default_double(settings, S_SPEED, 50.0);

	obs_data_set_default_bool(settings, S_COLOR, true);
	char *path = obs_module_file("dvd.png");
	obs_data_set_default_string(settings, S_FILE, path);
	obs_data_set_default_string(settings, S_SOURCE_ID, S_SOURCE_IMAGE);
	bfree(path);
}

static void dvd_source_destroy(void *data)
{
	struct dvd_source *context = data;

	/* Also moves the color filter back from the external source */
	remove_source(context);

	obs_source_remove(context->logo_source);
	obs_source_release(context->logo_source);

	if (context->color_filter) {
		obs_source_remove(context->color_filter);
		obs_source_release(context->color_filter);
	}

	pthread_mutex_destroy(&context->other_mutex);
	bfree(data);
}

static uint32_t dvd_source_getwidth(void *data)
{
	struct dvd_source *context = data;
	return context->cx;
}

static uint32_t dvd_source_getheight(void *data)
{
	struct dvd_source *context = data;
	return context->cy;
}

static void dvd_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct dvd_source *context = data;
	obs_source_t *target = NULL;

	if (context->use_image)
		target = obs_source_get_ref(context->logo_source);
	else
		target = get_other_source(context);

	if (!target)
		return;

	/* The size of the shown source can change at any time (e.g. a new
	 * image file was loaded, or a text source was edited) */
	context->img_cx =
		(uint32_t)(obs_source_get_width(target) * context->scale);
	context->img_cy =
		(uint32_t)(obs_source_get_height(target) * context->scale);

	gs_matrix_push();
	gs_matrix_translate3f(context->pos.x, context->pos.y, 0.f);
	gs_matrix_scale3f((float)context->scale, (float)context->scale, 1.f);
	obs_source_video_render(target);
	gs_matrix_pop();

	obs_source_release(target);
}

/* Moves the position along one axis and returns true if it hit an edge */
static bool move_axis(float *pos, int8_t *dir, float delta, float max)
{
	/* The image doesn't fit, so there's nowhere to move */
	if (max <= 0.f) {
		*pos = 0.f;
		return false;
	}

	*pos += delta;
	if (*pos <= 0.f) {
		*pos = 0.f;
		*dir = 1;
		return true;
	}
	if (*pos >= max) {
		*pos = max;
		*dir = -1;
		return true;
	}
	return false;
}

static void dvd_source_tick(void *data, float seconds)
{
	struct dvd_source *context = data;

	if (!obs_source_showing(context->source))
		return;

	float max_x = (float)context->cx - (float)context->img_cx;
	float max_y = (float)context->cy - (float)context->img_cy;
	float d_x = seconds * (float)(context->d_x * context->speed);
	float d_y = seconds * (float)(context->d_y * context->speed);

	/* Not short circuited, both axes have to move */
	bool bounce = move_axis(&context->pos.x, &context->d_x, d_x, max_x);
	bounce |= move_axis(&context->pos.y, &context->d_y, d_y, max_y);

#ifndef MAC_OS
	if (bounce && context->use_color) {
		/* Shift color for each bounce */
		context->hue_shift += 45.0;
		if (context->hue_shift > 180.0)
			context->hue_shift = -180.0 + 45.0;

		obs_data_t *settings = obs_data_create();
		obs_data_set_double(settings, S_HUE_SHIFT, context->hue_shift);
		obs_source_update(context->color_filter, settings);
		obs_data_release(settings);
	}
#else
	UNUSED_PARAMETER(bounce);
#endif
}

static bool use_color_shift_changed(obs_properties_t *props,
				    obs_property_t *p, obs_data_t *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(data);
	/* Returning true forces an update */
	return true;
}

static bool image_path_changed(obs_properties_t *props, obs_property_t *p,
			       obs_data_t *data)
{
	UNUSED_PARAMETER(p);
	uint32_t cx = 0, cy = 0;

	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	get_image_size(obs_data_get_string(data, S_FILE), &cx, &cy);

	obs_property_int_set_limits(obs_properties_get(props, S_SOURCE_CX),
				    cx + 4, 0xffff, 1);
	obs_property_int_set_limits(obs_properties_get(props, S_SOURCE_CY),
				    cy + 4, 0xffff, 1);
	return true;
}

static bool reset_logo_position(obs_properties_t *props,
				obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct dvd_source *context = data;
	int max_x = (int)context->cx - (int)context->img_cx;
	int max_y = (int)context->cy - (int)context->img_cy;

	context->pos.x = (float)rand_limit(max_x);
	context->pos.y = (float)rand_limit(max_y);
	context->d_x = rand_limit(1) == 0 ? -1 : 1;
	context->d_y = rand_limit(1) == 0 ? -1 : 1;
	return true;
}

static bool dvd_enum_global_sources(void *param, obs_source_t *current)
{
	struct enum_param *data = param;
	uint32_t flags = obs_source_get_output_flags(current);
	const char *name = obs_source_get_name(current);
	const char *id = obs_source_get_id(current);
	if (data->this == current || (flags & OBS_SOURCE_VIDEO) == 0 ||
	    strcmp(name, SOURCE_NAME) == 0 || strcmp(id, DVD_SOURCE_ID) == 0)
		return true;

	struct dstr label = {0};
	struct dstr value = {0};

	dstr_printf(&label, "%s %s", T_EXTERNAL_SOURCE, name);
	/* Every external source gets a postfix to distinguish it from the
	 * internal 'Image' option, to prevent confusion between a source
	 * named 'Image' and the actual 'Image' option */
	dstr_printf(&value, "%se", name);
	obs_property_list_add_string(data->list, label.array, value.array);

	dstr_free(&label);
	dstr_free(&value);
	return true;
}

static bool source_changed(obs_properties_t *props, obs_property_t *prop,
			   obs_data_t *data)
{
	UNUSED_PARAMETER(prop);
	const char *val = obs_data_get_string(data, S_SOURCE_ID);
	bool vis = strcmp(val, S_SOURCE_IMAGE) == 0;
	obs_property_t *path = obs_properties_get(props, S_FILE);

	obs_property_set_visible(path, vis);
	return true;
}

static obs_properties_t *dvd_source_properties(void *data)
{
	struct dvd_source *context = data;
	if (!context)
		return NULL;

	obs_properties_t *props = obs_source_properties(context->logo_source);
	if (!props)
		return NULL;

	obs_property_t *list = obs_properties_add_list(props, S_SOURCE_ID,
						       T_SOURCE_ID,
						       OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, T_SOURCE_IMAGE, S_SOURCE_IMAGE);
	struct enum_param e;
	e.list = list;
	e.this = context->source;

	obs_property_set_modified_callback(list, source_changed);
	obs_enum_sources(dvd_enum_global_sources, &e);

#ifndef MAC_OS
	obs_properties_add_bool(props, S_COLOR, T_COLOR);
#endif
	obs_properties_add_int(props, S_SOURCE_CX, T_SOURCE_CX, 25, 0xffff, 1);
	obs_properties_add_int(props, S_SOURCE_CY, T_SOURCE_CY, 25, 0xffff, 1);
	obs_properties_add_float_slider(props, S_SPEED, T_SPEED, 0.10f, 1000.f,
					0.25);
	obs_properties_add_float_slider(props, S_SCALE, T_SCALE, 0.10f, 10.f,
					0.25);
	obs_properties_add_button2(props, S_RESET, T_RESET, reset_logo_position,
				   context);
	obs_property_set_modified_callback(obs_properties_get(props, S_FILE),
					   image_path_changed);

	obs_property_set_modified_callback(obs_properties_get(props, S_COLOR),
					   use_color_shift_changed);

	obs_property_set_visible(obs_properties_get(props, "unload"), false);

	return props;
}

static void *dvd_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct dvd_source *context = bzalloc(sizeof(struct dvd_source));
	context->source = source;
	pthread_mutex_init(&context->other_mutex, NULL);

	/* The image source shares the settings, so it picks up the file */
	context->logo_source = obs_source_create_private(
		"image_source", SOURCE_NAME, settings);
#ifndef MAC_OS
	/* The filter gets its own settings, it's only updated from the tick */
	context->color_filter =
		obs_source_create_private("color_filter", FILTER_NAME, NULL);
	move_color_filter(context, context->logo_source);
#endif

	context->pos.x = 0;
	context->pos.y = 0;
	context->d_x = 1;
	context->d_y = 1;

	dvd_source_update(context, settings);
	return context;
}

static void dvd_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct dvd_source *context = data;
	cb(context->source, context->logo_source, param);
	obs_source_t *target = get_other_source(context);
	if (target) {
		cb(context->source, target, param);
		obs_source_release(target);
	}
}

static struct obs_source_info dvd_source_info = {
	.id = DVD_SOURCE_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = dvd_source_get_name,
	.create = dvd_source_create,
	.destroy = dvd_source_destroy,
	.update = dvd_source_update,
	.get_defaults = dvd_source_defaults,
	.get_width = dvd_source_getwidth,
	.get_height = dvd_source_getheight,
	.video_render = dvd_source_render,
	.enum_active_sources = dvd_enum_sources,
	.video_tick = dvd_source_tick,
	.get_properties = dvd_source_properties};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("dvd-screensaver", "en-US")

bool obs_module_load(void)
{
	obs_register_source(&dvd_source_info);
	return true;
}

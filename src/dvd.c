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

#include <graphics/image-file.h>
#include <obs-module.h>

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
	obs_weak_source_t *other_source;
	obs_source_t *color_filter;

	bool use_image;          /* False for custom src */
	bool use_color;          /* Enable color filter	*/
	uint32_t cx, cy;         /* Source size		*/
	uint32_t img_cx, img_cy; /* Scaled image size	*/
	struct vec2 pos;         /* Image position	*/
	int8_t d_x, d_y;         /* Movement vector	*/
	struct vec4 color;       /* Color		*/
	double scale, speed;     /* Image scale/speed	*/
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

static int rand_limit(int limit)
{
	int divisor = RAND_MAX / (limit + 1);
	int retval;

	do {
		retval = rand() / divisor;
	} while (retval > limit);

	return retval;
}

static void remove_source(struct dvd_source *context)
{
	obs_source_t *target =
		obs_weak_source_get_source(context->other_source);
	if (target) {
		obs_source_filter_remove(target, context->color_filter);
		obs_source_remove_active_child(context->source, target);
		obs_source_release(target);
	}

	if (context->other_source) {
		obs_weak_source_release(context->other_source);
		context->other_source = NULL;
	}
	obs_source_add_active_child(context->source, context->logo_source);
}

static bool add_source(struct dvd_source *context, const char *id)
{
	bool result = true;
	obs_source_t *target = obs_get_source_by_name(id);
	if (target) {
		context->other_source = obs_source_get_weak_source(target);
		context->img_cx = obs_source_get_width(target) * context->scale;
		context->img_cy =
			obs_source_get_height(target) * context->scale;
		obs_source_filter_add(target, context->color_filter);
		obs_source_remove_active_child(context->source,
					       context->logo_source);
		if (!obs_source_add_active_child(context->source, target)) {
			result = false;
			remove_source(context);
			warn("Can't add %s, as it would cause recursion", id);
		}
		obs_source_release(target);
	}
	return result;
}

static void dvd_source_update(void *data, obs_data_t *settings)
{
	struct dvd_source *context = data;

	context->cx = obs_data_get_int(settings, S_SOURCE_CX);
	context->cy = obs_data_get_int(settings, S_SOURCE_CY);
	context->scale = obs_data_get_double(settings, S_SCALE);
	context->speed = obs_data_get_double(settings, S_SPEED);
	context->use_color = obs_data_get_bool(settings, S_COLOR);

#ifndef MAC_OS
	obs_source_set_enabled(context->color_filter, context->use_color);
#endif

	obs_source_update(context->logo_source, settings);
	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	gs_image_file_t temp;
	const char *file = obs_data_get_string(settings, "file");
	const char *id = obs_data_get_string(settings, S_SOURCE_ID);
	context->use_image = strcmp(id, S_SOURCE_IMAGE) == 0;

	if (file && strlen(file) > 0) {
		gs_image_file_init(&temp, file);
		obs_enter_graphics();
		gs_image_file_init_texture(&temp);
		context->img_cx = temp.cx * context->scale;
		context->img_cy = temp.cy * context->scale;
		gs_image_file_free(&temp);
		obs_leave_graphics();
	} else {
		context->img_cx = obs_source_get_width(context->logo_source) *
				  context->scale;
		context->img_cy = obs_source_get_height(context->logo_source) *
				  context->scale;
	}

	if (context->use_image) {
		remove_source(context);
	} else {
		/* id is still postfixed with 'e' so get rid of that */
		char *cpy = bstrdup(id);
		int index = strlen(cpy) - 1;
		if (index > 0)
			cpy[index] = '\0';
		remove_source(context);

		add_source(context, cpy);
		bfree(cpy);
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
	obs_data_set_default_string(settings, "file", path);
	obs_data_set_default_string(settings, S_SOURCE_ID, T_SOURCE_IMAGE);
	bfree(path);
}

static void dvd_source_destroy(void *data)
{
	struct dvd_source *context = data;
	remove_source(context);
	obs_source_remove(context->logo_source);
	obs_source_release(context->logo_source);

#ifndef MAC_OS
	obs_source_remove(context->color_filter);
	obs_source_release(context->color_filter);
#endif
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
	struct dvd_source *context = data;
	if (obs_source_showing(context->source)) {
		obs_source_t *target = NULL;
		bool free = false;
		if (context->use_image) {
			target = context->logo_source;
		} else if (context->other_source) {
			target = obs_weak_source_get_source(
				context->other_source);
			free = true;
		}

		if (target) {
			gs_matrix_push();
			gs_matrix_translate3f(context->pos.x, context->pos.y,
					      0.f);
			gs_matrix_scale3f(context->scale, context->scale, 1.f);
			obs_source_video_render(target);
			gs_matrix_pop();
		}

		if (free)
			obs_source_release(target);
	}
}

static void dvd_source_tick(void *data, float seconds)
{
	struct dvd_source *context = data;

	if (obs_source_showing(context->source)) {
		float d_x = seconds * (context->d_x * context->speed);
		float d_y = seconds * (context->d_y * context->speed);

		bool bounce = false;

		if (context->pos.x + d_x + context->img_cx >= context->cx ||
		    context->pos.x + d_x <= 0) {
			context->d_x *= -1;
			bounce = true;
		} else {
			context->pos.x += d_x;
		}

		if (context->pos.y + d_y + context->img_cy >= context->cy ||
		    context->pos.y + d_y <= 0) {
			context->d_y *= -1;
			bounce = true;
		} else {
			context->pos.y += d_y;
		}
#ifndef MAC_OS
		if (bounce) /* Shift color for each bounce */ {
			obs_data_t *settings =
				obs_source_get_settings(context->color_filter);
			double val =
				obs_data_get_double(settings, "hue_shift") +
				45.f;
			if (val > 180.f)
				val = -180.f + 45.f;
			obs_data_set_double(settings, "hue_shift", val);
			obs_source_update(context->color_filter, settings);
			obs_data_release(settings);
		}
#endif
	}
}

bool use_color_shift_changed(obs_properties_t *props, obs_property_t *p,
			     obs_data_t *data)
{
	/* Returning true forces an update */
	return true;
}

bool image_path_changed(obs_properties_t *props, obs_property_t *p,
			obs_data_t *data)
{
	uint32_t cx = 0, cy = 0;

	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	gs_image_file_t temp;
	char *file = obs_data_get_string(data, "file");
	gs_image_file_init(&temp, file);

	obs_enter_graphics();
	gs_image_file_init_texture(&temp);
	cx = temp.cx;
	cy = temp.cy;
	gs_image_file_free(&temp);
	obs_leave_graphics();

	obs_property_int_set_limits(obs_properties_get(props, S_SOURCE_CX),
				    cx + 4, 0xffff, 1);
	obs_property_int_set_limits(obs_properties_get(props, S_SOURCE_CY),
				    cy + 4, 0xffff, 1);
	return true;
}
static bool reset_logo_position(obs_properties_t *props,
				obs_property_t *property, void *data)
{
	struct dvd_source *context = data;
	context->pos.x = rand_limit(context->cx - context->img_cx);
	context->pos.y = rand_limit(context->cx - context->img_cy);
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
	char *tmp = bzalloc(512);
	char *tmp2 = bzalloc(512);

	strcat(tmp, T_EXTERNAL_SOURCE);
	strcat(tmp, " ");
	strcat(tmp, name);

	strcat(tmp2, name);
	strcat(tmp2,
	       "e"); /* Every external source get's a postfix to distinguish it from
			     the internal 'Image' option, to prevent confusion between a
			     source named 'Image' and the actual 'Image' option */
	obs_property_list_add_string(data->list, tmp, tmp2);
	bfree(tmp);
	bfree(tmp2);
	return true;
}

static bool source_changed(obs_properties_t *props, obs_property_t *prop,
			   obs_data_t *data)
{
	char *val = obs_data_get_string(data, S_SOURCE_ID);
	bool vis = strcmp(val, S_SOURCE_IMAGE) == 0;
	obs_property_t *path = obs_properties_get(props, "file");

	obs_property_set_visible(path, vis);
	return true;
}

static obs_properties_t *dvd_source_properties(void *data)
{
	struct dvd_source *context = data;
	if (context) {
		obs_properties_t *props =
			obs_source_properties(context->logo_source);
		if (props) {
			obs_property_t *list = obs_properties_add_list(
				props, S_SOURCE_ID, T_SOURCE_ID,
				OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
			obs_property_list_add_string(list, T_SOURCE_IMAGE,
						     S_SOURCE_IMAGE);
			struct enum_param e;
			e.list = list;
			e.this = context->source;

			obs_property_set_modified_callback(list,
							   source_changed);
			obs_enum_sources(dvd_enum_global_sources, &e);

#ifndef MAC_OS
			obs_properties_add_bool(props, S_COLOR, T_COLOR);
#endif
			obs_properties_add_int(props, S_SOURCE_CX, T_SOURCE_CX,
					       25, 0xffff, 1);
			obs_properties_add_int(props, S_SOURCE_CY, T_SOURCE_CY,
					       25, 0xffff, 1);
			obs_properties_add_float_slider(props, S_SPEED, T_SPEED,
							0.10f, 1000.f, 0.25);
			obs_properties_add_float_slider(props, S_SCALE, T_SCALE,
							0.10f, 10.f, 0.25);
			obs_properties_add_button(props, S_RESET, T_RESET,
						  reset_logo_position);
			obs_property_set_modified_callback(
				obs_properties_get(props, "file"),
				image_path_changed);

			obs_property_set_modified_callback(
				obs_properties_get(props, S_COLOR),
				use_color_shift_changed);

			obs_property_set_visible(
				obs_properties_get(props, "unload"), false);

			return props;
		}
	}
	return NULL;
}

static void *dvd_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct dvd_source *context = bzalloc(sizeof(struct dvd_source));
	context->source = source;
	context->logo_source = obs_source_create_private(
		"image_source", SOURCE_NAME, settings);
#ifndef MAC_OS
	context->color_filter = obs_source_create_private(
		"color_filter", FILTER_NAME, settings);
	obs_source_add_active_child(context->source, context->color_filter);
	obs_source_filter_add(context->logo_source, context->color_filter);
#endif

	obs_source_add_active_child(context->source, context->logo_source);

	obs_source_update(context->logo_source, settings);
	dvd_source_update(context, settings);

	context->pos.x = 0;
	context->pos.y = 0;
	context->d_x = 1;
	context->d_y = 1;
	context->use_image = true;
	return context;
}

static void dvd_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct dvd_source *context = data;
	cb(context->source, context->logo_source, param);
	obs_source_t *target =
		obs_weak_source_get_source(context->other_source);
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

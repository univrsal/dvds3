* This file is part of dvds3
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
#include "obs-internal.h"

#define blog(log_level, format, ...) \
	blog(log_level, "[dvd_source: '%s'] " format, \
			obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) \
	blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) \
	blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) \
	blog(LOG_WARNING, format, ##__VA_ARGS__)

#define S_SOURCE_CX	"source_cx"
#define S_SOURCE_CY	"source_cy"
#define S_IMG_CX	"img_cx"
#define S_IMG_CY	"img_cy"
#define S_SPEED		"speed"
#define S_SCALE		"logo_scale"
#define S_COLOR		"color_shift"
#define S_RESET		"reset"

#define _T(t)		obs_module_text(t)
#define T_SOURCE_CX	_T("DVDSource.Width")
#define T_SOURCE_CY	_T("DVDSource.Height")
#define T_SOURCE	_T("DVDSource.Name")
#define T_SPEED		_T("DVDSource.Speed")
#define T_SCALE		_T("DVDSource.Scale")
#define T_COLOR		_T("DVDSource.Color")
#define T_RESET		_T("DVDSource.Reset")

struct dvd_source {
	obs_source_t*	source;
	obs_source_t*	image_source;
	obs_source_t*   color_filter;

	bool		use_color;	/* Enable color filter	*/
	uint32_t	cx, cy;		/* Source size		*/
	uint32_t	img_cx, img_cy;	/* Scaled image size	*/
	struct vec2	pos;		/* Image position	*/
	int8_t		d_x, d_y;	/* Movement vector	*/
	struct vec4	color;		/* Color		*/
	double		scale, speed;	/* Image scale/speed	*/
};

static const char* dvd_source_get_name(void* unused)
{
	UNUSED_PARAMETER(unused);
	return T_SOURCE;
}

static void dvd_source_update(void* data, obs_data_t* settings)
{
	struct dvd_source* context = data;
	obs_source_update(context->image_source, settings);
	
	context->cx = obs_data_get_int(settings, S_SOURCE_CX);
	context->cy = obs_data_get_int(settings, S_SOURCE_CY);
	context->scale = obs_data_get_double(settings, S_SCALE);
	context->speed = obs_data_get_double(settings, S_SPEED);
	context->use_color = obs_data_get_bool(settings, S_COLOR);

	obs_source_set_enabled(context->color_filter, context->use_color);

	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	gs_image_file_t temp;
	char* file = obs_data_get_string(settings, "file");
	if (file && strlen(file) > 0)
	{
		gs_image_file_init(&temp, file);
		obs_enter_graphics();
		gs_image_file_init_texture(&temp);
		context->img_cx = temp.cx * context->scale;
		context->img_cy = temp.cy * context->scale;
		gs_image_file_free(&temp);
		obs_leave_graphics();
	}
	else
	{
		context->img_cx = obs_source_get_width(context->image_source) * context->scale;
		context->img_cy = obs_source_get_height(context->image_source) * context->scale;
	}
}

static void dvd_source_defaults(obs_data_t* settings)
{
	
	obs_data_set_default_int(settings, S_SOURCE_CX, 640);
	obs_data_set_default_int(settings, S_SOURCE_CY, 480);
	obs_data_set_default_double(settings, S_SCALE, 1.0);
	obs_data_set_default_double(settings, S_SPEED, 50.0);
	obs_data_set_default_bool(settings, S_COLOR, true);
	char* path = obs_module_file("dvd.png");
	obs_data_set_default_string(settings, "file", path);
	bfree(path);
}

static void dvd_source_destroy(void* data)
{
	struct dvd_source* context = data;
	obs_source_remove(context->image_source);
	obs_source_release(context->image_source);
#ifndef MAC_OS
	obs_source_remove(context->color_filter);
	obs_source_release(context->color_filter);
#endif
	bfree(data);
}

static uint32_t dvd_source_getwidth(void* data)
{
	struct dvd_source* context = data;
	return context->cx;
}

static uint32_t dvd_source_getheight(void* data)
{
	struct dvd_source* context = data;
	return context->cy;
}

static void dvd_source_render(void* data, gs_effect_t* effect)
{
	struct dvd_source* context = data;
	gs_matrix_push();
	gs_matrix_translate3f(context->pos.x, context->pos.y, 0.f);
	gs_matrix_scale3f(context->scale, context->scale, 1.f);
	obs_source_video_render(context->image_source);
	gs_matrix_pop();
}

static void dvd_source_tick(void* data, float seconds)
{
	struct dvd_source* context = data;
	if (obs_source_showing(context->source))
	{
		float d_x = seconds * (context->d_x * context->speed);
		float d_y = seconds * (context->d_y * context->speed);
		
		bool bounce = false;

		if (context->pos.x + d_x + context->img_cx >= context->cx ||
			context->pos.x + d_x <= 0)
		{
			context->d_x *= -1;
			d_x *= -1;
			bounce = true;
		}
		else
		{
			context->pos.x += d_x;
		}
			
		if (context->pos.y + d_y + context->img_cy >= context->cy ||
			context->pos.y + d_y <= 0)
		{
			context->d_y *= -1;
			d_y *= -1;
			bounce = true;
		}
		else
		{
			context->pos.y += d_y;
		}
#ifndef MAC_OS
		if (bounce) /* Shift color for each bounce */
		{
			obs_data_t* settings = obs_source_get_settings(context->color_filter);
			double val = obs_data_get_double(settings, "hue_shift") + 45.f;
			if (val > 180.f)
				val = -180.f + 45.f;
			obs_data_set_double(settings, "hue_shift", val);
			obs_source_update(context->color_filter, settings);
			obs_data_release(settings);
		}
#endif
	}
}

bool use_color_shift_changed(obs_properties_t* props, obs_property_t* p, obs_data_t* data)
{
	/* Returning true forces an update */
	return true;
}

bool image_path_changed(obs_properties_t* props, obs_property_t* p, obs_data_t* data)
{
	uint32_t cx = 0, cy = 0;

	/* Since obs_source_getwidth reports old values, we
	 * get the image size here every time a new file is loaded
	 */
	gs_image_file_t temp;
	char* file = obs_data_get_string(data, "file");
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
static bool reset_logo_position(obs_properties_t* props, obs_property_t* property,
	void* data)
{
	struct dvd_source* context = data;
	context->pos.x = 0;
	context->pos.y = 0;
	context->d_x = 1;
	context->d_y = 1;
	return true;
}

static obs_properties_t* dvd_source_properties(void* data)
{
	struct dvd_source* context = data;
	if (context)
	{
		obs_properties_t* props = obs_source_properties(context->image_source);
		if (props)
		{
#ifndef MAC_OS
			obs_properties_add_bool(props, S_COLOR, T_COLOR);
#endif
			obs_properties_add_int(props, S_SOURCE_CX, T_SOURCE_CX, 25, 0xffff, 1);
			obs_properties_add_int(props, S_SOURCE_CY, T_SOURCE_CY, 25, 0xffff, 1);
			obs_properties_add_float_slider(props, S_SPEED, T_SPEED, 0.10f, 1000.f, 0.25);
			obs_properties_add_float_slider(props, S_SCALE, T_SCALE, 0.10f, 10.f, 0.25);
			obs_properties_add_button(props, S_RESET, T_RESET, reset_logo_position);

			obs_property_set_modified_callback(obs_properties_get(props, "file"),
				image_path_changed);

			obs_property_set_modified_callback(obs_properties_get(props, S_COLOR),
				use_color_shift_changed);

			obs_property_set_visible(obs_properties_get(props, "unload"), false);
			return props;
		}
	}
	return NULL;
}

static void* dvd_source_create(obs_data_t* settings, obs_source_t* source)
{
	struct dvd_source* context = bzalloc(sizeof(struct dvd_source));
	context->source = source;
	context->image_source = obs_source_create("image_source", "dvd-image", settings, NULL);
#ifndef MAC_OS
	context->color_filter = obs_source_create("color_filter", "dvd-filter", settings, NULL);
	obs_source_add_active_child(context->source, context->color_filter);
	obs_source_filter_add(context->image_source, context->color_filter);
#endif	

	obs_source_add_active_child(context->source, context->image_source);

	obs_source_update(context->image_source, settings);
	dvd_source_update(context, settings);
	
	context->pos.x = 0;
	context->pos.y = 0;
	context->d_x = 1;
	context->d_y = 1;
	return context;
}

static struct obs_source_info dvd_source_info = {
	.id = "dvds3_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = dvd_source_get_name,
	.create	= dvd_source_create,
	.destroy = dvd_source_destroy,
	.update = dvd_source_update,
	.get_defaults = dvd_source_defaults,
	.get_width = dvd_source_getwidth,
	.get_height = dvd_source_getheight,
	.video_render = dvd_source_render,
	.video_tick = dvd_source_tick,
	.get_properties = dvd_source_properties
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("dvd-screensaver", "en-US")

bool obs_module_load(void)
{
	obs_register_source(&dvd_source_info);
}

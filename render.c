#include <stdlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "config.h"
#include "criteria.h"
#include "mako.h"
#include "notification.h"
#include "render.h"
#include "wayland.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "icon.h"

#define M_PI 3.14159265358979323846

static void set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
		(color >> (3*8) & 0xFF) / 255.0,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0);
}

static void set_layout_size(PangoLayout *layout, int width, int height,
		int scale) {
	pango_layout_set_width(layout, width * scale * PANGO_SCALE);
	pango_layout_set_height(layout, height * scale * PANGO_SCALE);
}

static void move_to(cairo_t *cairo, double x, double y, int scale) {
	cairo_move_to(cairo, x * scale, y * scale);
}

static void set_rounded_rectangle(cairo_t *cairo, double x, double y, double width, double height,
		int scale, int radius_top_left, int radius_top_right, int radius_bottom_right, int radius_bottom_left) {
	if (width == 0 || height == 0) {
		return;
	}
	x *= scale;
	y *= scale;
	width *= scale;
	height *= scale;
	radius_top_left *= scale;
	radius_top_right *= scale;
	radius_bottom_right *= scale;
	radius_bottom_left *= scale;
	double degrees = M_PI / 180.0;

	cairo_new_sub_path(cairo);

	cairo_arc(cairo, x + radius_top_left, y + radius_top_left, radius_top_left, 180 * degrees, 270 * degrees);
	cairo_arc(cairo, x + width - radius_top_right, y + radius_top_right, radius_top_right, -90 * degrees, 0 * degrees);
	cairo_arc(cairo, x + width - radius_bottom_right, y + height - radius_bottom_right, radius_bottom_right, 0 * degrees, 90 * degrees);
	cairo_arc(cairo, x + radius_bottom_left, y + height - radius_bottom_left, radius_bottom_left, 90 * degrees, 180 * degrees);

	cairo_close_path(cairo);
}

static cairo_subpixel_order_t get_cairo_subpixel_order(
		enum wl_output_subpixel subpixel) {
	switch (subpixel) {
	case WL_OUTPUT_SUBPIXEL_UNKNOWN:
	case WL_OUTPUT_SUBPIXEL_NONE:
		return CAIRO_SUBPIXEL_ORDER_DEFAULT;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_RGB;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_BGR;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_VRGB;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_VBGR;
	}
	abort();
}

static void set_font_options(cairo_t *cairo, struct mako_surface *surface) {
	if (surface->surface_output == NULL) {
		return;
	}

	cairo_font_options_t *fo = cairo_font_options_create();
	if (surface->surface_output->subpixel == WL_OUTPUT_SUBPIXEL_NONE ||
	    surface->surface_output->subpixel == WL_OUTPUT_SUBPIXEL_UNKNOWN) {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_GRAY);
	} else {
		cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
		cairo_font_options_set_subpixel_order(fo,
			get_cairo_subpixel_order(surface->surface_output->subpixel));
	}
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
}

static int render_notification(cairo_t *cairo, struct mako_state *state, struct mako_surface *surface,
		struct mako_style *style, const char *text, struct mako_icon *icon, int offset_y, int scale,
		struct mako_hotspot *hotspot, int progress) {
	int border_size = 2 * style->border_size;
	int padding_height = style->padding.top + style->padding.bottom;
	int padding_width = style->padding.left + style->padding.right;
	int radius_top_left = style->border_radius.top;
	int radius_top_right = style->border_radius.right;
	int radius_bottom_right = style->border_radius.bottom;
	int radius_bottom_left = style->border_radius.left;
	int icon_radius = style->icon_border_radius;
	bool icon_vertical = style->icon_location == MAKO_ICON_LOCATION_TOP ||
		style->icon_location == MAKO_ICON_LOCATION_BOTTOM;

	int notif_width =
		(style->width <= surface->width) ? style->width : surface->width;

	int offset_x;
	if (surface->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		offset_x = surface->width - notif_width - style->margin.right;
	} else if (surface->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		offset_x = style->margin.left;
	} else {
		offset_x = (surface->width - notif_width) / 2;
	}

	double text_x = style->padding.left;
	if (icon != NULL && style->icon_location == MAKO_ICON_LOCATION_LEFT) {
		text_x = icon->width + 2*style->padding.left;
	}

	double text_y = style->padding.top;
	if (icon != NULL && style->icon_location == MAKO_ICON_LOCATION_TOP) {
		text_y = icon->height + 2*style->padding.top;
	}

	double text_layout_width = notif_width - border_size - padding_width;
	if (icon && ! icon_vertical) {
		text_layout_width -= icon->width;
		text_layout_width -= style->icon_location == MAKO_ICON_LOCATION_LEFT ?
			(style->padding.left * 2) : (style->padding.right * 2);
	}
	double text_layout_height = style->height - border_size - padding_height;
	if (icon && icon_vertical) {
		text_layout_height -= icon->height;
		text_layout_height -= style->icon_location == MAKO_ICON_LOCATION_TOP ?
			(style->padding.top * 2) : (style->padding.bottom * 2);
	}

	set_font_options(cairo, surface);

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	set_layout_size(layout, text_layout_width, text_layout_height, scale);
	pango_layout_set_alignment(layout, style->text_alignment);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	PangoFontDescription *desc =
		pango_font_description_from_string(style->font);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	PangoAttrList *attrs = NULL;
	GError *error = NULL;
	char *buf = NULL;
	if (pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
		pango_layout_set_text(layout, buf, -1);
		free(buf);
	} else {
		fprintf(stderr, "cannot parse pango markup: %s\n", error->message);
		g_error_free(error);
		pango_layout_set_text(layout, text, -1);
	}

	if (attrs == NULL) {
		attrs = pango_attr_list_new();
	}
	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);

	int buffer_text_height = 0;
	int buffer_text_width = 0;

	if (pango_layout_get_character_count(layout) > 0) {
		pango_layout_get_pixel_size(layout, &buffer_text_width, &buffer_text_height);
	}
	int text_height = buffer_text_height / scale;
	int text_width = buffer_text_width / scale;

	if (text_height > text_layout_height) {
		text_height = text_layout_height;
	}

	int notif_height = text_height + border_size + padding_height;
	if (icon && icon_vertical) {
		notif_height += icon->height;
		notif_height += style->icon_location == MAKO_ICON_LOCATION_TOP ?
			style->padding.top : style->padding.bottom;
	}
	if (icon != NULL && ! icon_vertical && icon->height > text_height) {
		notif_height = icon->height + border_size + padding_height;
	}
	if (notif_height < radius_top_left + radius_bottom_left) {
		notif_height = radius_top_left + radius_bottom_left + border_size;
	}
	if (notif_height < radius_top_right + radius_bottom_right) {
		notif_height = radius_top_right + radius_bottom_right + border_size;
	}

	int notif_background_width = notif_width - style->border_size;

	// Render box shadow if configured
	if (style->box_shadow_blur > 0 ||
	    style->box_shadow_offset.top != 0 ||
	    style->box_shadow_offset.right != 0 ||
	    style->box_shadow_offset.bottom != 0 ||
	    style->box_shadow_offset.left != 0) {

		double shadow_offset_x = (style->box_shadow_offset.right - style->box_shadow_offset.left) / 2.0;
		double shadow_offset_y = (style->box_shadow_offset.bottom - style->box_shadow_offset.top) / 2.0;

		double shadow_r = ((style->box_shadow_color >> 24) & 0xFF) / 255.0;
		double shadow_g = ((style->box_shadow_color >> 16) & 0xFF) / 255.0;
		double shadow_b = ((style->box_shadow_color >> 8) & 0xFF) / 255.0;
		double shadow_a = (style->box_shadow_color & 0xFF) / 255.0;

		if (style->box_shadow_blur > 0) {
			int blur_radius = style->box_shadow_blur;
			int num_layers = style->box_shadow_quality > 0 ? style->box_shadow_quality : 60;
			if (num_layers < 10) num_layers = 10;
			if (num_layers > 200) num_layers = 200;
			double sigma = style->box_shadow_sigma > 0 ? style->box_shadow_sigma : 0.45;

			// Calculate normalization factor
			double gaussian_sum = 0.0;
			for (int i = 0; i < num_layers; i++) {
				double t = (double)i / (num_layers - 1);
				double gaussian = exp(-(t * t) / (2.0 * sigma * sigma));
				gaussian_sum += gaussian;
			}

			cairo_surface_t *shadow_surface = cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32,
				cairo_image_surface_get_width(cairo_get_target(cairo)),
				cairo_image_surface_get_height(cairo_get_target(cairo))
			);
			cairo_t *shadow_cr = cairo_create(shadow_surface);

			// Rendu de la couche la plus grande en premier (t=1, spread max)
			// puis les couches plus petites par-dessus avec OVER.
			// La première couche utilise SOURCE pour établir la base proprement.
			for (int i = num_layers - 1; i >= 0; i--) {
				double t = (double)i / (num_layers - 1);
				double spread = blur_radius * t;
				double gaussian = exp(-(t * t) / (2.0 * sigma * sigma));
				// Normalize alpha so total doesn't exceed shadow_a
				double alpha = shadow_a * gaussian / gaussian_sum;
				double r_spread = spread * 0.8;

				if (i == num_layers - 1) {
					cairo_set_operator(shadow_cr, CAIRO_OPERATOR_SOURCE);
				} else {
					cairo_set_operator(shadow_cr, CAIRO_OPERATOR_OVER);
				}

				set_rounded_rectangle(shadow_cr,
					offset_x + style->border_size / 2.0 + shadow_offset_x - spread,
					offset_y + style->border_size / 2.0 + shadow_offset_y - spread,
					notif_background_width + spread * 2,
					notif_height - style->border_size + spread * 2,
					scale,
					radius_top_left + r_spread,
					radius_top_right + r_spread,
					radius_bottom_right + r_spread,
					radius_bottom_left + r_spread);

				cairo_set_source_rgba(shadow_cr, shadow_r, shadow_g, shadow_b, alpha);
				cairo_fill(shadow_cr);
			}

			cairo_set_source_surface(cairo, shadow_surface, 0, 0);
			cairo_paint(cairo);

			cairo_destroy(shadow_cr);
			cairo_surface_destroy(shadow_surface);

		} else {
			set_rounded_rectangle(cairo,
				offset_x + style->border_size / 2.0 + shadow_offset_x,
				offset_y + style->border_size / 2.0 + shadow_offset_y,
				notif_background_width,
				notif_height - style->border_size,
				scale, radius_top_left, radius_top_right, radius_bottom_right, radius_bottom_left);

			cairo_set_source_rgba(cairo, shadow_r, shadow_g, shadow_b, shadow_a);
			cairo_fill(cairo);
		}
	}

	set_rounded_rectangle(cairo,
		offset_x + style->border_size / 2.0,
		offset_y + style->border_size / 2.0,
		notif_background_width,
		notif_height - style->border_size,
		scale, radius_top_left, radius_top_right, radius_bottom_right, radius_bottom_left);

	set_source_u32(cairo, style->colors.background);
	cairo_fill_preserve(cairo);

	cairo_path_t *border_path = cairo_copy_path(cairo);

	int progress_width =
		(notif_background_width - style->border_size) * progress / 100;
	if (progress_width < 0) {
		progress_width = 0;
	} else if (progress_width > notif_background_width) {
		progress_width = notif_background_width - style->border_size;
	}

	cairo_save(cairo);
	cairo_clip(cairo);
	cairo_set_operator(cairo, style->colors.progress.operator);
	set_source_u32(cairo, style->colors.progress.value);
	set_rounded_rectangle(cairo,
			offset_x + style->border_size,
			offset_y + style->border_size,
			progress_width,
			notif_height - style->border_size,
			scale, 0, 0, 0, 0);
	cairo_fill(cairo);
	cairo_restore(cairo);

	cairo_save(cairo);
	cairo_append_path(cairo, border_path);
	set_source_u32(cairo, style->colors.border);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_line_width(cairo, style->border_size * scale);
	cairo_stroke(cairo);
	cairo_restore(cairo);

	cairo_path_destroy(border_path);

	if (icon != NULL) {
		double xpos = -1;
		double ypos = -1;
		double ypos_center = offset_y + style->border_size +
			(notif_height - icon->height - border_size) / 2;
		double xpos_center = offset_x + style->border_size +
			(notif_width - icon->width - border_size) / 2;

		switch (style->icon_location) {
		case MAKO_ICON_LOCATION_LEFT:
			xpos = offset_x + style->border_size +
				style->padding.left;
			ypos = ypos_center;
			break;
		case MAKO_ICON_LOCATION_RIGHT:
			xpos = offset_x + notif_width - style->border_size -
				style->padding.right - icon->width;
			ypos = ypos_center;
			break;
		case MAKO_ICON_LOCATION_TOP:
			xpos = xpos_center;
			ypos = offset_y + style->border_size +
				style->padding.top;
			break;
		case MAKO_ICON_LOCATION_BOTTOM:
			xpos = xpos_center;
			ypos = offset_y + notif_height - style->border_size -
				style->padding.bottom - icon->height;
			break;
		}
		cairo_save(cairo);
		set_rounded_rectangle(cairo, xpos, ypos, icon->width, icon->height, scale, icon_radius, icon_radius, icon_radius, icon_radius);
		cairo_clip(cairo);
		draw_icon(cairo, icon, xpos, ypos, scale);
		cairo_restore(cairo);
	}

	if (icon_vertical) {
		text_x = (notif_width - text_width - border_size) / 2;
	} else {
		text_y = (notif_height - text_height - border_size) / 2;
	}

	set_source_u32(cairo, style->colors.text);
	move_to(cairo,
		offset_x + style->border_size + text_x,
		offset_y + style->border_size + text_y,
		scale);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);

	if (hotspot != NULL) {
		hotspot->x = offset_x;
		hotspot->y = offset_y;
		hotspot->width = notif_width;
		hotspot->height = notif_height;
	}

	g_object_unref(layout);

	int shadow_extra_height = 0;
	if (style->box_shadow_blur > 0 ||
	    style->box_shadow_offset.bottom != 0 ||
	    style->box_shadow_offset.top != 0) {
		double shadow_offset_y = (style->box_shadow_offset.bottom - style->box_shadow_offset.top) / 2.0;
		shadow_extra_height = style->box_shadow_blur / 2 + (shadow_offset_y > 0 ? shadow_offset_y : 0);
	}

	return notif_height + shadow_extra_height;
}

void render(struct mako_surface *surface, struct pool_buffer *buffer, int scale,
		int *rendered_width, int *rendered_height) {
	struct mako_state *state = surface->state;
	cairo_t *cairo = buffer->cairo;

	*rendered_width = *rendered_height = 0;

	if (wl_list_empty(&state->notifications)) {
		return;
	}

	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	size_t visible_count = 0;
	size_t hidden_count = 0;
	int total_height = 0;
	int max_width = 0;
	int pending_bottom_margin = 0;
	struct mako_notification *notif;
	size_t total_notifications = 0;
	wl_list_for_each(notif, &state->notifications, link) {
		if (notif->surface != surface) {
			continue;
		}
		++total_notifications;

		int rematch_count = apply_each_criteria(&state->config.criteria, notif);
		if (rematch_count == -1) {
			fprintf(stderr, "Failed to apply criteria\n");
			break;
		} else if (rematch_count == 0) {
			fprintf(stderr, "Notification matched zero criteria?!\n");
			break;
		}

		struct mako_style *style = &notif->style;

		if (style->max_visible >= 0 &&
				visible_count >= (size_t)style->max_visible) {
			++hidden_count;
			continue;
		}

		if (style->invisible) {
			continue;
		}

		size_t text_len =
			format_text(style->format, NULL, format_notif_text, notif);

		char *text = malloc(text_len + 1);
		if (text == NULL) {
			fprintf(stderr, "Unable to allocate memory to render notification\n");
			break;
		}
		format_text(style->format, text, format_notif_text, notif);

		if (style->margin.top > pending_bottom_margin) {
			total_height += style->margin.top;
		} else {
			total_height += pending_bottom_margin;
		}

		struct mako_icon *icon = (style->icons) ? notif->icon : NULL;
		int notif_height = render_notification(
			cairo, state, surface, style, text, icon, total_height, scale,
			&notif->hotspot, notif->progress);
		free(text);

		int notif_width =
			style->width + style->margin.left + style->margin.right;

		total_height += notif_height;
		if (max_width < notif_width) {
			max_width = notif_width;
		}
		pending_bottom_margin = style->margin.bottom;

		if (notif->group_index < 1) {
			++visible_count;
		}
	}

	if (hidden_count > 0) {
		struct mako_notification *hidden_notif = create_notification(state);
		hidden_notif->surface = surface;
		hidden_notif->hidden = true;
		apply_each_criteria(&state->config.criteria, hidden_notif);

		struct mako_style *style = &hidden_notif->style;

		if (!style->invisible) {
			if (style->margin.top > pending_bottom_margin) {
				total_height += style->margin.top;
			} else {
				total_height += pending_bottom_margin;
			}

			struct mako_hidden_format_data data = {
				.hidden = hidden_count,
				.count = total_notifications,
			};

			size_t text_ln =
				format_text(style->format, NULL, format_hidden_text, &data);
			char *text = malloc(text_ln + 1);
			if (text == NULL) {
				fprintf(stderr, "allocation failed");
				return;
			}

			format_text(style->format, text, format_hidden_text, &data);

			int hidden_height = render_notification(
				cairo, state, surface, style, text, NULL, total_height, scale, NULL, 0);
			free(text);

			total_height += hidden_height;
			pending_bottom_margin = style->margin.bottom;
		}
		destroy_notification(hidden_notif);
	}

	*rendered_width = max_width;
	*rendered_height = total_height;
}

/*
 *  OverheadMap_SDL.cpp -- Subclass of OverheadMapClass for rendering with SDL
 */

#include "cseries.h"

#include "OverheadMap_SDL.h"
#include "map.h"
#include "screen_drawing.h"


// From screen_sdl.cpp
extern SDL_Surface *world_pixels;


/*
 *  Draw polygon
 */

void OverheadMap_SDL_Class::draw_polygon(short vertex_count, short *vertices, rgb_color& color)
{
	// Reallocate vertex array if necessary
	static world_point2d *vertex_array = NULL;
	static int max_vertices = 0;
	if (vertex_count > max_vertices) {
		delete[] vertex_array;
		vertex_array = new world_point2d[vertex_count];
		max_vertices = vertex_count;
	}

	// Copy vertex array
	for (int i=0; i<vertex_count; i++)
		vertex_array[i] = GetVertex(vertices[i]);

	// Get color
	uint32 pixel = SDL_MapRGB(world_pixels->format, color.red >> 8, color.green >> 8, color.blue >> 8);

	// Draw polygon
	::draw_polygon(world_pixels, vertex_array, vertex_count, pixel);
}


/*
 *  Draw "thing" (object)
 */

void OverheadMap_SDL_Class::draw_thing(world_point2d &center, rgb_color &color, short shape, short radius)
{
	// Adjust object display so that objects get properly centered
	int raddown = int(0.75 * radius);
	int radup = int(1.5 * radius);
	SDL_Rect r = {center.x - raddown, center.y - raddown, radup, radup};

	// Get color
	uint32 pixel = SDL_MapRGB(world_pixels->format, color.red >> 8, color.green >> 8, color.blue >> 8);

	// Draw thing
	switch (shape) {
		case _rectangle_thing:
			SDL_FillRect(world_pixels, &r, pixel);
			break;
		case _circle_thing:
			//!!
			break;
	}
}


/*
 *  Draw player
 */

void OverheadMap_SDL_Class::draw_player(world_point2d &center, angle facing, rgb_color &color, short shrink, short front, short rear, short rear_theta)
{
	// Construct triangle vertices
	world_point2d triangle[3];
	triangle[0] = triangle[1] = triangle[2] = center;
	translate_point2d(triangle + 0, front >> shrink, facing);
	translate_point2d(triangle + 1, rear >> shrink, normalize_angle(facing + rear_theta));
	translate_point2d(triangle + 2, rear >> shrink, normalize_angle(facing - rear_theta));

	// Get color
	uint32 pixel = SDL_MapRGB(world_pixels->format, color.red >> 8, color.green >> 8, color.blue >> 8);

	// Draw triangle
	::draw_polygon(world_pixels, triangle, 3, pixel);
}


/*
 *  Draw text
 */

void OverheadMap_SDL_Class::draw_text(world_point2d &location, rgb_color &color, char *text, FontDataStruct &FontData, short which, short justify)
{
	// Load font
	TextSpec spec;
	spec.font = FontData.font;
	spec.style = FontData.face;
	spec.size = FontData.size;
	sdl_font_info *font = load_font(spec);

	// Find left-side location
	int xpos = location.x;
	if (justify = _justify_center)
		xpos -= text_width(text, font, spec.style) / 2;

	// Get color
	uint32 pixel = SDL_MapRGB(world_pixels->format, color.red >> 8, color.green >> 8, color.blue >> 8);

	// Draw text
	::draw_text(world_pixels, text, xpos, location.y, pixel, font, spec.style);
}

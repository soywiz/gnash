// shape.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Quadratic bezier outline shapes, the basis for most SWF rendering.


#include "shape_character_def.h"
#include "shape.h" // for mesh_set

#include "impl.h"
#include "log.h"
#include "render.h"
#include "stream.h"
#include "tesselate.h"
#include "movie_definition.h" // TODO: check if really needed
//#include "bitmap_character_def.h"
#include "sprite_instance.h"

#include "tu_file.h"

#include <float.h>


#define DEBUG_DISPLAY_SHAPE_PATHS
#ifdef DEBUG_DISPLAY_SHAPE_PATHS
// For debugging only!
bool	gnash_debug_show_paths = false;
#endif // DEBUG_DISPLAY_SHAPE_PATHS


namespace gnash {

static float	s_curve_max_pixel_error = 1.0f;


//
// helper functions.
//

void	set_curve_max_pixel_error(float pixel_error)
{
    s_curve_max_pixel_error = fclamp(pixel_error, 1e-6f, 1e6f);
}

float	get_curve_max_pixel_error()
{
    return s_curve_max_pixel_error;
}


// Read fill styles, and push them onto the given style array.
static void
read_fill_styles(std::vector<fill_style>& styles, stream* in,
		int tag_type, movie_definition* m)
{
    //assert(styles);

    // Get the count.
    int	fill_style_count = in->read_u8();
    if (tag_type > 2) {
	if (fill_style_count == 0xFF)
	    {
		fill_style_count = in->read_u16();
	    }
    }

    log_parse("  read_fill_styles: count = %d\n", fill_style_count);

    // Read the styles. 
    for (int i = 0; i < fill_style_count; i++) {
	styles.resize(styles.size() + 1);
	//styles[styles.size() - 1].read(in, tag_type, m);
	styles.back().read(in, tag_type, m);
    }
}


static void
read_line_styles(std::vector<line_style>& styles, stream* in, int tag_type)
    // Read line styles and push them onto the back of the given array.
{
    // Get the count.
    int	line_style_count = in->read_u8();
    
    log_parse("  read_line_styles: count = %d\n", line_style_count);

    // @@ does the 0xFF flag apply to all tag types?
    // if (tag_type > 2)
    // {
    if (line_style_count == 0xFF) {
	line_style_count = in->read_u16();
	log_parse("  read_line_styles: count2 = %d\n", line_style_count);
    }
    // }

    // Read the styles.
    for (int i = 0; i < line_style_count; i++) {
	styles.resize(styles.size() + 1);
	//styles[styles.size() - 1].read(in, tag_type);
	styles.back().read(in, tag_type);
    }
}


shape_character_def::shape_character_def()
{
}


shape_character_def::~shape_character_def()
{
    // Free our mesh_sets.
    for (unsigned int i = 0; i < m_cached_meshes.size(); i++) {
	delete m_cached_meshes[i];
    }
}


void
shape_character_def::read(stream* in, int tag_type, bool with_style,
	movie_definition* m)
{
    if (with_style) {
	m_bound.read(in);
	read_fill_styles(m_fill_styles, in, tag_type, m);
	read_line_styles(m_line_styles, in, tag_type);
    }

    int	num_fill_bits = in->read_uint(4);
    int	num_line_bits = in->read_uint(4);

    log_parse("  shape_character_def read: nfillbits = %d, nlinebits = %d", num_fill_bits, num_line_bits);

    // These are state variables that keep the
    // current position & style of the shape
    // outline, and vary as we read the edge data.
    //
    // At the moment we just store each edge with
    // the full necessary info to render it, which
    // is simple but not optimally efficient.
    int	fill_base = 0;
    int	line_base = 0;
    float	x = 0, y = 0;
    path	current_path;

#define SHAPE_LOG 0
    // SHAPERECORDS
    for (;;) {
	int	type_flag = in->read_uint(1);
	if (type_flag == 0) {
	    // Parse the record.
	    int	flags = in->read_uint(5);
	    if (flags == 0) {
		// End of shape records.

		// Store the current path if any.
		if (! current_path.is_empty())
		    {
			m_paths.push_back(current_path);
			current_path.m_edges.resize(0);
		    }

		break;
	    }
	    if (flags & 0x01) {
		// move_to = 1;

		// Store the current path if any, and prepare a fresh one.
		if (! current_path.is_empty()) {
		    m_paths.push_back(current_path);
		    current_path.m_edges.resize(0);
		}

		int	num_move_bits = in->read_uint(5);
		int	move_x = in->read_sint(num_move_bits);
		int	move_y = in->read_sint(num_move_bits);

		x = (float) move_x;
		y = (float) move_y;

		// Set the beginning of the path.
		current_path.m_ax = x;
		current_path.m_ay = y;

		if (SHAPE_LOG) {
		    log_parse("  shape_character read: moveto %4g %4g\n", x, y);
		}
	    }
	    if ((flags & 0x02) && num_fill_bits > 0) {
		// fill_style_0_change = 1;
		if (! current_path.is_empty()) {
		    m_paths.push_back(current_path);
		    current_path.m_edges.resize(0);
		    current_path.m_ax = x;
		    current_path.m_ay = y;
		}
		int	style = in->read_uint(num_fill_bits);
		if (style > 0) {
		    style += fill_base;
		}
		current_path.m_fill0 = style;
		if (SHAPE_LOG) {
		    log_parse("  shape_character read: fill0 = %d\n", current_path.m_fill0);
		}
		
	    }
	    if ((flags & 0x04) && num_fill_bits > 0) {
		// fill_style_1_change = 1;
		if (! current_path.is_empty()) {
		    m_paths.push_back(current_path);
		    current_path.m_edges.resize(0);
		    current_path.m_ax = x;
		    current_path.m_ay = y;
		}
		int	style = in->read_uint(num_fill_bits);
		if (style > 0) {
		    style += fill_base;
		}
		current_path.m_fill1 = style;
		if (SHAPE_LOG)
		    log_parse("  shape_character read: fill1 = %d\n", current_path.m_fill1);
	    }
	    if ((flags & 0x08) && num_line_bits > 0) {
		// line_style_change = 1;
		if (! current_path.is_empty()) {
		    m_paths.push_back(current_path);
		    current_path.m_edges.resize(0);
		    current_path.m_ax = x;
		    current_path.m_ay = y;
		}
		int	style = in->read_uint(num_line_bits);
		if (style > 0) {
		    style += line_base;
		}
		current_path.m_line = style;
		if (SHAPE_LOG)
		    log_parse("  shape_character_read: line = %d\n", current_path.m_line);
	    }
	    if (flags & 0x10) {
		if (tag_type == 2) {
		    tag_type+=20;
		}
		assert(tag_type >= 22);

		log_parse("  shape_character read: more fill styles\n");

		// Store the current path if any.
		if (! current_path.is_empty()) {
		    m_paths.push_back(current_path);
		    current_path.m_edges.resize(0);

		    // Clear styles.
		    current_path.m_fill0 = -1;
		    current_path.m_fill1 = -1;
		    current_path.m_line = -1;
		}
		// Tack on an empty path signalling a new shape.
		// @@ need better understanding of whether this is correct??!?!!
		// @@ i.e., we should just start a whole new shape here, right?
		m_paths.push_back(path());
		m_paths.back().m_new_shape = true;

		fill_base = m_fill_styles.size();
		line_base = m_line_styles.size();
		read_fill_styles(m_fill_styles, in, tag_type, m);
		read_line_styles(m_line_styles, in, tag_type);
		num_fill_bits = in->read_uint(4);
		num_line_bits = in->read_uint(4);
	    }
	} else {
	    // EDGERECORD
	    int	edge_flag = in->read_uint(1);
	    if (edge_flag == 0) {
		// curved edge
		int num_bits = 2 + in->read_uint(4);
		float	cx = x + in->read_sint(num_bits);
		float	cy = y + in->read_sint(num_bits);
		float	ax = cx + in->read_sint(num_bits);
		float	ay = cy + in->read_sint(num_bits);

		if (SHAPE_LOG)
		    log_parse("  shape_character read: curved edge   = %4g %4g - %4g %4g - %4g %4g\n", x, y, cx, cy, ax, ay);

		current_path.m_edges.push_back(edge(cx, cy, ax, ay));

		x = ax;
		y = ay;
	    } else {
		// straight edge
		int	num_bits = 2 + in->read_uint(4);
		int	line_flag = in->read_uint(1);
		float	dx = 0, dy = 0;
		if (line_flag) {
		    // General line.
		    dx = (float) in->read_sint(num_bits);
		    dy = (float) in->read_sint(num_bits);
		} else {
		    int	vert_flag = in->read_uint(1);
		    if (vert_flag == 0) {
			// Horizontal line.
			dx = (float) in->read_sint(num_bits);
		    } else {
			// Vertical line.
			dy = (float) in->read_sint(num_bits);
		    }
		}

		if (SHAPE_LOG)
		    log_parse("  shape_character_read: straight edge = %4g %4g - %4g %4g\n", x, y, x + dx, y + dy);

		current_path.m_edges.push_back(edge(x + dx, y + dy, x + dx, y + dy));

		x += dx;
		y += dy;
	    }
	}
    }
}


void	shape_character_def::display(character* inst)
    // Draw the shape using our own inherent styles.
{
//    GNASH_REPORT_FUNCTION;

    matrix	mat = inst->get_world_matrix();
    cxform	cx = inst->get_world_cxform();

    float	pixel_scale = inst->get_parent()->get_pixel_scale();
    display(mat, cx, pixel_scale, m_fill_styles, m_line_styles);
}


#ifdef DEBUG_DISPLAY_SHAPE_PATHS

#include "ogl.h"


static void	point_normalize(point* p)
{
    float	mag2 = p->m_x * p->m_x + p->m_y * p->m_y;
    if (mag2 < 1e-9f) {
	// Very short vector.
	// @@ log error

	// Arbitrary unit vector.
	p->m_x = 1;
	p->m_y = 0;
    }

    float	inv_mag = 1.0f / sqrtf(mag2);
    p->m_x *= inv_mag;
    p->m_y *= inv_mag;
}


static void	show_fill_number(const point& p, int fill_number)
{
    // We're inside a glBegin(GL_LINES)

    // Eh, let's do it in binary, least sig four bits...
    float	x = p.m_x;
    float	y = p.m_y;

    int	mask = 8;
    while (mask) {
	if (mask & fill_number)	{
	    // Vert line --> 1.
	    glVertex2f(x, y - 40.0f);
	    glVertex2f(x, y + 40.0f);
	} else {
	    // Rectangle --> 0.
	    glVertex2f(x - 10.0f, y - 40.0f);
	    glVertex2f(x + 10.0f, y - 40.0f);

	    glVertex2f(x + 10.0f, y - 40.0f);
	    glVertex2f(x + 10.0f, y + 40.0f);

	    glVertex2f(x - 10.0f, y + 40.0f);
	    glVertex2f(x + 10.0f, y + 40.0f);

	    glVertex2f(x - 10.0f, y - 40.0f);
	    glVertex2f(x - 10.0f, y + 40.0f);
	}
	x += 40.0f;
	mask >>= 1;
    }
}


static void	debug_display_shape_paths(
    const matrix& mat,
    float object_space_max_error,
    const std::vector<path>& paths,
    const std::vector<fill_style>& fill_styles,
    const std::vector<line_style>& line_styles)
{
    for (unsigned int i = 0; i < paths.size(); i++) {
//			if (i > 0) break;//xxxxxxxx
	const path&	p = paths[i];

	if (p.m_fill0 == 0 && p.m_fill1 == 0) {
	    continue;
	}

	gnash::render::set_matrix(mat);

	// Color the line according to which side has
	// fills.
	if (p.m_fill0 == 0) glColor4f(1, 0, 0, 0.5);
	else if (p.m_fill1 == 0) glColor4f(0, 1, 0, 0.5);
	else glColor4f(0, 0, 1, 0.5);

	// Offset according to which loop we are.
	float	offset_x = (i & 1) * 80.0f;
	float	offset_y = ((i & 2) >> 1) * 80.0f;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(offset_x, offset_y, 0.f);

	point	pt;

	glBegin(GL_LINE_STRIP);

	mat.transform(&pt, point(p.m_ax, p.m_ay));
	glVertex2f(pt.m_x, pt.m_y);

	for (unsigned int j = 0; j < p.m_edges.size(); j++)	{
	    mat.transform(&pt, point(p.m_edges[j].m_cx, p.m_edges[j].m_cy));
	    glVertex2f(pt.m_x, pt.m_y);
	    mat.transform(&pt, point(p.m_edges[j].m_ax, p.m_edges[j].m_ay));
	    glVertex2f(pt.m_x, pt.m_y);
	}

	glEnd();

	// Draw arrowheads.
	point	dir, right, p0, p1;
	glBegin(GL_LINES);
	{for (unsigned int j = 0; j < p.m_edges.size(); j++)
	    {
		mat.transform(&p0, point(p.m_edges[j].m_cx, p.m_edges[j].m_cy));
		mat.transform(&p1, point(p.m_edges[j].m_ax, p.m_edges[j].m_ay));
		dir = point(p1.m_x - p0.m_x, p1.m_y - p0.m_y);
		point_normalize(&dir);
		right = point(-dir.m_y, dir.m_x);	// perpendicular

		const float	ARROW_MAG = 60.f;	// TWIPS?
		if (p.m_fill0 != 0)
		    {
			glColor4f(0, 1, 0, 0.5);
			glVertex2f(p0.m_x,
				   p0.m_y);
			glVertex2f(p0.m_x - dir.m_x * ARROW_MAG - right.m_x * ARROW_MAG,
				   p0.m_y - dir.m_y * ARROW_MAG - right.m_y * ARROW_MAG);

			show_fill_number(point(p0.m_x - right.m_x * ARROW_MAG * 4,
					       p0.m_y - right.m_y * ARROW_MAG * 4),
					 p.m_fill0);
		    }
		if (p.m_fill1 != 0)
		    {
			glColor4f(1, 0, 0, 0.5);
			glVertex2f(p0.m_x,
				   p0.m_y);
			glVertex2f(p0.m_x - dir.m_x * ARROW_MAG + right.m_x * ARROW_MAG,
				   p0.m_y - dir.m_y * ARROW_MAG + right.m_y * ARROW_MAG);

			show_fill_number(point(p0.m_x + right.m_x * ARROW_MAG * 4,
					       p0.m_y + right.m_y * ARROW_MAG * 4),
					 p.m_fill1);
		    }
	    }}
	glEnd();

	glPopMatrix();
    }
}
#endif // DEBUG_DISPLAY_SHAPE_PATHS

		
void	shape_character_def::display(
    const matrix& mat,
    const cxform& cx,
    float pixel_scale,
    const std::vector<fill_style>& fill_styles,
    const std::vector<line_style>& line_styles) const
    // Display our shape.  Use the fill_styles arg to
    // override our default set of fill styles (e.g. when
    // rendering text).
{
//    GNASH_REPORT_FUNCTION;

    // Compute the error tolerance in object-space.
    float	max_scale = mat.get_max_scale();
    if (fabsf(max_scale) < 1e-6f) {
	// Scale is essentially zero.
	return;
    }

    float	object_space_max_error = 20.0f / max_scale / pixel_scale * s_curve_max_pixel_error;

#ifdef DEBUG_DISPLAY_SHAPE_PATHS
    // Render a debug view of shape path outlines, instead
    // of the tesselated shapes themselves.
    if (gnash_debug_show_paths) {
	debug_display_shape_paths(mat, object_space_max_error, m_paths, fill_styles, line_styles);

	return;
    }
#endif // DEBUG_DISPLAY_SHAPE_PATHS

    // See if we have an acceptable mesh available; if so then render with it.
    for (unsigned int i = 0, n = m_cached_meshes.size(); i < n; i++) {
	const mesh_set*	candidate = m_cached_meshes[i];

	if (object_space_max_error > candidate->get_error_tolerance() * 3.0f)
	    {
		// Mesh is too high-res; the remaining meshes are higher res,
		// so stop searching and build an appropriately scaled mesh.
		break;
	    }

	if (object_space_max_error > candidate->get_error_tolerance()) {
	    // Do it.
	    candidate->display(mat, cx, fill_styles, line_styles);
	    return;
	}
    }

    // Construct a new mesh to handle this error tolerance.
    mesh_set*	m = new mesh_set(this, object_space_max_error * 0.75f);
    m_cached_meshes.push_back(m);
    m->display(mat, cx, fill_styles, line_styles);
		
    sort_and_clean_meshes();
}


static int	sort_by_decreasing_error(const void* A, const void* B)
{
    const mesh_set*	a = *(const mesh_set* const *) A;
    const mesh_set*	b = *(const mesh_set* const *) B;

    if (a->get_error_tolerance() < b->get_error_tolerance()) {
	return 1;
    } else if (a->get_error_tolerance() > b->get_error_tolerance()) {
	return -1;
    } else {
	return 0;
    }
}


void	shape_character_def::sort_and_clean_meshes() const
    // Maintain cached meshes.  Clean out mesh_sets that haven't
    // been used recently, and make sure they're sorted from high
    // error to low error.
{
    // Re-sort.
    if (m_cached_meshes.size() > 0) {
	qsort(
	    &m_cached_meshes[0],
	    m_cached_meshes.size(),
	    sizeof(m_cached_meshes[0]),
	    sort_by_decreasing_error);

	// Check to make sure the sort worked as intended.
#ifndef NDEBUG
	for (unsigned int i = 0, n = m_cached_meshes.size() - 1; i < n; i++) {
	    const mesh_set*	a = m_cached_meshes[i];
	    const mesh_set*	b = m_cached_meshes[i + 1];

	    assert(a->get_error_tolerance() > b->get_error_tolerance());
	}
#endif // not NDEBUG
    }
}

		
void	shape_character_def::tesselate(float error_tolerance, tesselate::trapezoid_accepter* accepter) const
    // Push our shape data through the tesselator.
{
    tesselate::begin_shape(accepter, error_tolerance);
    for (unsigned int i = 0; i < m_paths.size(); i++) {
	if (m_paths[i].m_new_shape == true)	{
	    // Hm; should handle separate sub-shapes in a less lame way.
	    tesselate::end_shape();
	    tesselate::begin_shape(accepter, error_tolerance);
	} else {
	    m_paths[i].tesselate();
	}
    }
    tesselate::end_shape();
}


bool	shape_character_def::point_test_local(float x, float y)
    // Return true if the specified point is on the interior of our shape.
    // Incoming coords are local coords.
{
    if (m_bound.point_test(x, y) == false) {
	// Early out.
	return false;
    }

    // Try each of the paths.
    for (unsigned int i = 0; i < m_paths.size(); i++) {
	if (m_paths[i].point_test(x, y))
	    {
		return true;
	    }
    }

    return false;
}


float shape_character_def::get_height_local()
{
    return m_bound.height();
}

float shape_character_def::get_width_local()
{
    return m_bound.width();
}


void	shape_character_def::compute_bound(rect* r) const
    // Find the bounds of this shape, and store them in
    // the given rectangle.
{
    r->m_x_min = 1e10f;
    r->m_y_min = 1e10f;
    r->m_x_max = -1e10f;
    r->m_y_max = -1e10f;

    for (unsigned int i = 0; i < m_paths.size(); i++) {
	const path&	p = m_paths[i];
	r->expand_to_point(p.m_ax, p.m_ay);
	for (unsigned int j = 0; j < p.m_edges.size(); j++)	{
	    r->expand_to_point(p.m_edges[j].m_ax, p.m_edges[j].m_ay);
//					r->expand_to_point(p.m_edges[j].m_cx, p.m_edges[j].m_cy);
	}
    }
}


void	shape_character_def::output_cached_data(tu_file* out, const cache_options& options)
    // Dump our precomputed mesh data to the given stream.
{
    int	n = m_cached_meshes.size();
    out->write_le32(n);

    for (int i = 0; i < n; i++) {
	m_cached_meshes[i]->output_cached_data(out);
    }
}


void	shape_character_def::input_cached_data(tu_file* in)
    // Initialize our mesh data from the given stream.
{
    int	n = in->read_le32();

    m_cached_meshes.resize(n);

    for (int i = 0; i < n; i++)	{
	mesh_set*	ms = new mesh_set();
	ms->input_cached_data(in);
	m_cached_meshes[i] = ms;
    }
}

	
}	// end namespace gnash


// Local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

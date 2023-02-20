//
//  DRAWOGLES.c
//  3D
//
//  Created by Devin Tuchsen on 11/1/15.
//  Copyright © 2015 Devin Tuchsen. All rights reserved.
//
#include <math.h>
#include "drawogles.h"
#include "globvars.h"
#include "gr.h"
#include "3d.h"
#include "mem.h"

#ifdef OGLES1

extern ubyte gr_current_pal[256*3];
extern int Gr_scanline_darkening_level;

int g3_draw_tmap_ogles(int nv, g3s_point **pointlist, g3s_uvl *uvl_list, grs_bitmap *bm) {
	GLfloat *vertices, *colors, *texture_coords;
	int i;
	
	vertices = malloc(sizeof(GLfloat) * 3 * nv);
	colors = malloc(sizeof(GLfloat) * 4 * nv);
	texture_coords = malloc(sizeof(GLfloat) * 2 * nv);
	
	// Build vertex list
	for (i = 0; i < nv; ++i) {
		vertices[i * 3] = f2fl(pointlist[i]->p3_vec.x);
		vertices[i * 3 + 1] = f2fl(pointlist[i]->p3_vec.y);
		vertices[i * 3 + 2] = -f2fl(pointlist[i]->p3_vec.z);
	}
	
	// Build color list (lighting)
	for (i = 0; i < nv; ++i) {
		colors[i * 4] = f2fl(uvl_list[i].l);
		colors[i * 4 + 1] = f2fl(uvl_list[i].l);
		colors[i * 4 + 2] = f2fl(uvl_list[i].l);
		colors[i * 4 + 3] = 1.0f;
	}
	
	// Build texture coord list
	for (i = 0; i < nv; ++i) {
		texture_coords[i * 2] = f2fl(uvl_list[i].u);
		texture_coords[i * 2 + 1] = f2fl(uvl_list[i].v);
	}
	
	glEnable(GL_TEXTURE_2D);
	ogles_bm_bind_teximage_2d(bm);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glColorPointer(4, GL_FLOAT, 0, colors);
	glTexCoordPointer(2, GL_FLOAT, 0, texture_coords);
	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	free(vertices);
	free(colors);
	free(texture_coords);
	return 0;
}

int g3_draw_poly_ogles(int nv, g3s_point **pointlist) {
	GLfloat *vertices;
	GLubyte alpha;
	int i;
	
	vertices = malloc(sizeof(GLfloat) * 3 * nv);
	
	// Build vertex list
	for (i = 0; i < nv; ++i) {
		vertices[i * 3] = f2fl(pointlist[i]->p3_vec.x);
		vertices[i * 3 + 1] = f2fl(pointlist[i]->p3_vec.y);
		vertices[i * 3 + 2] = -f2fl(pointlist[i]->p3_vec.z);
	}
	
	if (Gr_scanline_darkening_level >= GR_FADE_LEVELS ) {
		alpha = 255;
	} else {
		alpha = 255 - ((float)Gr_scanline_darkening_level / (float)GR_FADE_LEVELS) * 255.0f;
	}
	
	glDisable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glColor4ub(gr_current_pal[grd_curcanv->cv_color * 3] * 4,
			  gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4,
			  gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4, alpha);
	glDrawArrays(GL_TRIANGLE_FAN, 0, nv);
	glDisableClientState(GL_VERTEX_ARRAY);

	free(vertices);
	return 0;
}

int g3_draw_line_ogles(g3s_point *p0, g3s_point *p1) {
	GLfloat x0, y0, z0, x1, y1, z1;
	
	x0 = f2fl(p0->x);
	y0 = f2fl(p0->y);
	z0 = -f2fl(p0->z);
	x1 = f2fl(p1->x);
	y1 = f2fl(p1->y);
	z1 = -f2fl(p1->z);
	
	GLfloat vertices[] = { x0, y0, z0, x1, y1, z1 };
	
	glDisable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glColor4ub(gr_current_pal[grd_curcanv->cv_color * 3] * 4,
			   gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4,
			   gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4, 255);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glDrawArrays(GL_LINES, 0, 2);
	glDisableClientState(GL_VERTEX_ARRAY);
	return 1;
}

// this is actually a circle.
// seems can share opengl code with  g3_draw_poly_ogles()
int g3_draw_sphere_ogles(g3s_point *pnt, fix rad) {
	GLfloat x, y, z, rx, ry;
	GLfloat vertices[60];
	int i = 0;

	x = f2fl(pnt->x);
	y = f2fl(pnt->y);
	z = -f2fl(pnt->z);
	rx = f2fl(fixmul(rad, Matrix_scale.x));
	ry = f2fl(rad);

	for (i = 19; i >= 0; --i) {
		float theta = 2.0f * M_PI * (float)i / 20.0f;

		vertices[i * 3] = x + sin(theta) * rx;
		vertices[i * 3 + 1] = y + cos(theta) * ry;
		vertices[i * 3 + 2] = z;
	}

	glDisable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glColor4ub(gr_current_pal[grd_curcanv->cv_color * 3] * 4,
			   gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4,
			   gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4, 255);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 20);
	glDisableClientState(GL_VERTEX_ARRAY);
	return 1;
}


#else // NON OGLES1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

extern GLint program_with_tex_id;

static const struct
{
    float r, g, b, a;
} colors[3] =
        {
                { 1.f, 0.f, 0.f, 1.f },
                { 0.f, 1.f, 0.f, 1.f },
                { 0.f, 0.f, 1.f, 1.f }
        };

void draw(GLuint program_id, int nv, GLfloat* vertices) {
    GLint vpos_location = glGetAttribLocation(program_id, "vertexPosition");
    glEnableVertexAttribArray(vpos_location);
    (glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, 0, (void*)vertices));

    GLint vcol_location = glGetAttribLocation(program_id, "vertexColor");
    glEnableVertexAttribArray(vcol_location);
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE, sizeof(colors[0]), (void*)&colors);

    glDrawArrays(GL_TRIANGLE_FAN, 0, nv);

    glDisableVertexAttribArray(vpos_location);
}

void draw_with_texture(int nv, GLfloat* vertices, GLfloat* tex_coords, GLfloat* colors, GLint texture_slot_id) {
    //glUseProgram(program_with_tex_id); // TODO: generalize
    glBindTexture(GL_TEXTURE_2D, texture_slot_id);

    GLint vpos_location = glGetAttribLocation(program_with_tex_id, "vertexPosition");
    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, 0, (void*)vertices);

    GLint vtex_coords_location = glGetAttribLocation(program_with_tex_id, "vTexCoords");
    glEnableVertexAttribArray(vtex_coords_location);
    glVertexAttribPointer(vtex_coords_location, 2, GL_FLOAT, GL_FALSE, 0, (void*)tex_coords);

    GLint vcolors_location = glGetAttribLocation(program_with_tex_id, "vertexColor");
    glEnableVertexAttribArray(vcolors_location);
    glVertexAttribPointer(vcolors_location, 3, GL_FLOAT, GL_FALSE, 0, (void*)colors);

    // Gets the location of the uniform holding texture sampler
    GLuint texUni = glGetUniformLocation(program_with_tex_id, "texSampler");

    // Sets the value of the uniform
    glUniform1i(texUni, 0); // WHY 0?

    glDrawArrays(GL_TRIANGLE_FAN, 0, nv);

    glDisableVertexAttribArray(vpos_location);
    glDisableVertexAttribArray(vtex_coords_location);
}

bool g3_draw_tmap_ogles(int nv, g3s_point** pointlist, g3s_uvl* uvl_list, grs_bitmap* bm) {

    GLfloat* vertices = (GLfloat*)malloc(sizeof(GLfloat) * 3 * nv);
    GLfloat* tex_coordinates = (GLfloat*)malloc(sizeof(GLfloat) * 2 * nv);
    GLfloat* colors = (GLfloat*)malloc(sizeof(GLfloat) * 3 * nv);

    // Build vertex list
    for (int i = 0; i < nv; ++i) {
        vertices[i * 3] = f2fl(pointlist[i]->p3_vec.x);
        vertices[i * 3 + 1] = f2fl(pointlist[i]->p3_vec.y);
        vertices[i * 3 + 2] = -f2fl(pointlist[i]->p3_vec.z);

        tex_coordinates[i * 2] = f2fl(uvl_list[i].u);
        tex_coordinates[i * 2 + 1] = f2fl(uvl_list[i].v);

        colors[i * 3 + 2] = colors[i * 3 + 1] = colors[i * 3] = f2fl(uvl_list[i].l);
    }

    ogles_bm_bind_teximage_2d(bm);
    draw_with_texture(nv, vertices, tex_coordinates, colors, bm->bm_ogles_tex_id);

    free(vertices);
    free(tex_coordinates);
    free(colors);

    return 1;
}

extern ubyte gr_current_pal[256*3];
int g3_draw_poly_ogles(int nv, g3s_point **pointlist) {
    GLfloat* vertices = (GLfloat*)malloc(sizeof(GLfloat) * 3 * nv);

    // Build vertex list
    for (int i = 0; i < nv; ++i) {
        vertices[i * 3] = f2fl(pointlist[i]->p3_vec.x);
        vertices[i * 3 + 1] = f2fl(pointlist[i]->p3_vec.y);
        vertices[i * 3 + 2] = -f2fl(pointlist[i]->p3_vec.z);
    }

    GLfloat alpha = 255; // shader accepts 3 component color at the moment

    // ALSO seems color scaling is wrong
    GLfloat colors[] = { gr_current_pal[grd_curcanv->cv_color * 3] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4 / 255.f,

                         gr_current_pal[grd_curcanv->cv_color * 3] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4 / 255.f,

                         gr_current_pal[grd_curcanv->cv_color * 3] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 1] * 4 / 255.f,
                         gr_current_pal[grd_curcanv->cv_color * 3 + 2] * 4 / 255.f,
    };
    GLfloat texCoords[] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, }; // magick number to say the shader not to use texture

    draw_with_texture(3, vertices, texCoords, colors, 13); // TODO: respect nv

    free(vertices); // can be on stack

    return 0;

}


int g3_draw_line_ogles(g3s_point *p0, g3s_point *p1) {
    return 1;

    GLfloat x0, y0, z0, x1, y1, z1;

    x0 = f2fl(p0->x);
    y0 = f2fl(p0->y);
    z0 = -f2fl(p0->z);
    x1 = f2fl(p1->x);
    y1 = f2fl(p1->y);
    z1 = -f2fl(p1->z);

    GLfloat vertices[] = { x0, y0, z0, x1, y1, z1 };

    glDisable(GL_TEXTURE_2D);
    // TODO: https://stackoverflow.com/questions/14486291/how-to-draw-line-in-opengl
    glDrawArrays(GL_LINES, 0, 2);

    return 1;
}

int g3_draw_sphere_ogles(g3s_point *pnt, fix rad) {return 1;}

#endif

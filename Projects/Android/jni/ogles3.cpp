#include "VrApi_Types.h"
#include "vr.h"
#include <unistd.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/norm.hpp>

#include <initializer_list>
#include <algorithm>

extern "C"
{
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <gr.h>
#include <3d.h>
#include <oglestex.h>
#include <globvars.h>

void ogles_bm_bind_teximage_2d_with_max_alpha(grs_bitmap* bm, ubyte max_alpha);

extern GLint program_with_tex_id;
extern ubyte gr_current_pal[256*3];
extern GLint ortho_program_id;

void draw_with_texture(int nv, GLfloat* vertices, GLfloat* tex_coords, GLfloat* colors, GLint texture_slot_id) {
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

extern ovrVector3f forward;
bool g3_draw_bitmap_ogles(g3s_point *pos, fix width, fix height, grs_bitmap *bm) {
    g3s_point pnt;
    fix w, h;

    w = fixmul(width, Matrix_scale.x) * 2;
    h = fixmul(height, Matrix_scale.y) * 2;

    GLfloat x0f, y0f, x1f, y1f, zf;

    // Calculate OGLES coords
    x0f = f2fl(pos->x - w / 2);
    y0f = f2fl(pos->y - h / 2);
    x1f = f2fl(pos->x + w / 2);
    y1f = f2fl(pos->y + h / 2);
    zf = -f2fl(pos->z);

    glm::vec3 cameraFront(forward.x, forward.y, forward.z);
    glm::vec3 pos1(f2fl(pos->x), f2fl(pos->y), -f2fl(pos->z));
    glm::vec3 originalNormal(0.0f, 0.0f, 1.0f);
    float norm1 = glm::l2Norm(cameraFront);
    float norm2 = glm::l2Norm(originalNormal);
    float angle = acos(glm::dot(originalNormal, cameraFront) / norm1 / norm2);

    glm::vec3 axis = glm::cross(originalNormal, cameraFront);
    axis /= glm::l2Norm(axis);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos1);
    model = glm::rotate(model, angle, axis);
    model = glm::translate(model, -pos1);

    glm::vec4 verts[] = { glm::vec4(x1f, y0f, zf, 1), glm::vec4(x0f, y0f, zf, 1), glm::vec4(x0f, y1f, zf, 1), glm::vec4(x1f, y1f, zf, 1) };
    GLfloat vertices[4];

    for(int i = 0; i < 4; i++)
    {
        verts[i] = model * verts[i] ;

        vertices[i * 3] = verts[i].x;
        vertices[i * 3 + 1] = verts[i].y;
        vertices[i * 3 + 2] = verts[i].z;
    }

    // Draw
    GLfloat texCoords[] = { 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    GLfloat colors[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, };

    ogles_bm_bind_teximage_2d_with_max_alpha(bm, 128);
    draw_with_texture(4, vertices, texCoords, colors, bm->bm_ogles_tex_id);

    return 0;

}

auto identity = glm::mat4(1.0f);;
void draw_char(int nv, GLfloat* vertices, GLfloat* tex_coords, GLfloat* colors, glm::mat4 &projection) {
    GL(glUseProgram(ortho_program_id));

    GL(glUniformMatrix4fv(glGetUniformLocation(ortho_program_id, "projection"), 1, GL_FALSE, &projection[0][0]));
    GL(glUniformMatrix4fv(glGetUniformLocation(ortho_program_id, "view"), 1, GL_FALSE, &identity[0][0]));
    GL(glUniformMatrix4fv(glGetUniformLocation(ortho_program_id, "model"), 1, GL_FALSE, &identity[0][0]));

    GLint vpos_location = glGetAttribLocation(ortho_program_id, "vPos");
    GL(glEnableVertexAttribArray(vpos_location));
    GL(glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE, 0, (void*)vertices));

    GLint vcolors_location = glGetAttribLocation(ortho_program_id, "vColors");
    GL(glEnableVertexAttribArray(vcolors_location));
    GL(glVertexAttribPointer(vcolors_location, 3, GL_FLOAT, GL_FALSE, 0, (void*)colors));

    GLint vtex_coords_location = glGetAttribLocation(program_with_tex_id, "vTexCoords");
    glEnableVertexAttribArray(vtex_coords_location);
    glVertexAttribPointer(vtex_coords_location, 2, GL_FLOAT, GL_FALSE, 0, (void*)tex_coords);

    // Gets the location of the uniform holding texture sampler
    GLuint texUni = glGetUniformLocation(program_with_tex_id, "texSampler");

    // Sets the value of the uniform
    glUniform1i(texUni, 0);

    GL(glDrawArrays(GL_TRIANGLE_FAN, 0, nv));

    glDisableVertexAttribArray(vpos_location);
    glDisableVertexAttribArray(vcolors_location);
    glDisableVertexAttribArray(vtex_coords_location);
    glUseProgram(0);
}

extern ubyte* gr_bitblt_fade_table;
extern grs_canvas * grd_curcanv;    //active canvas
void scale_bitmap_ogles(grs_bitmap* bm, int x0, int y0, int x1, int y1) {
    GLfloat alpha = 1.0f;
    float w = grd_curcanv->cv_bitmap.bm_w;
    float h = grd_curcanv->cv_bitmap.bm_h;

    GLfloat vertices[] = { (float)x0 - w / 2, (float)y1 - h / 2, 1.f,
                           (float)x1 - w / 2, (float)y1 - h / 2, 1.f,
                           (float)x1 - w / 2, (float)y0 - h / 2, 1.f,
                           (float)x0  - w / 2, (float)y0 - h / 2, 1.f };

    GLfloat texCoords[] = { 0.f, 1.f,
                            1.f, 1.f,
                            1.f, 0.f,
                            0.f, 0.f, };
    GLfloat colors[16];

    ogles_bm_bind_teximage_2d(bm);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (gr_bitblt_fade_table) {
        alpha = (float)gr_bitblt_fade_table[(int)fmax(y0, 0)] / 31.0f;
    }
    else {
        alpha = (float)Gr_scanline_darkening_level / (float)GR_FADE_LEVELS;
    }

    // TODO: color alpha
    // Magic value means this is a font that needs a color
    if (bm->avg_color == 255) {
        auto color_component = [](int i) {return gr_current_pal[grd_curcanv->cv_font_fg_color * 3 + i] / 63.0f; };
        auto colors1 = { color_component(0), color_component(1), color_component(2),
                         color_component(0), color_component(1), color_component(2),
                         color_component(0), color_component(1), color_component(2),
                         color_component(0), color_component(1), color_component(2)};

        std::copy(std::begin(colors1), std::end(colors1), std::begin(colors));
    }
    else {
        auto colors1 = { 1.0f, 1.0f, 1.0f,
                         1.0f, 1.0f, 1.0f,
                         1.0f, 1.0f, 1.0f,
                         1.0f, 1.0f, 1.0f};
        std::copy(std::begin(colors1), std::end(colors1), std::begin(colors));
    }

    auto projection = glm::ortho(-w/2, w/2, h/2, -h/2, -1000.0f, 1000.0f);
    draw_char(4, vertices, texCoords, colors, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

int g3_draw_line_ogles(g3s_point *p0, g3s_point *p1) { return 1; }

int g3_draw_sphere_ogles(g3s_point *pnt, fix rad) { return 1; }

}
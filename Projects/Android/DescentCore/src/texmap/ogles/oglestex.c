//
//  OGLTEXTURES.c
//  TEXMAP
//
//  Created by Devin Tuchsen on 10/29/15.
//  Copyright © 2015 Devin Tuchsen. All rights reserved.
//

#ifdef OGLES

#ifdef ANDROID_NDK
#ifdef OGLES1
#include <GLES/gl.h>
#else // NON OGLES1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif // OGLES1
#else // ANDROID_NDK
#include <OpenGLES/ES1/gl.h>
#endif // ANDROID_NDK
#include <string.h>
#include <stdlib.h>
#include "gr.h"
#include "rle.h"
#include "oglestex.h"

extern ubyte gr_current_pal[256*3];

void ogles_clear_canvas_textures() {
	//glDeleteTextures(NUM_OGL_TEXTURES, grd_curcanv->cv_ogles_textures);
}

void ogles_bm_bind_teximage_2d(grs_bitmap* bm) {
	GLubyte* image_data;
	ubyte* data, * sbits, * dbits;
	int i;

	if (!glIsTexture(bm->bm_ogles_tex_id)) {
		glGenTextures(1, &bm->bm_ogles_tex_id);
		glBindTexture(GL_TEXTURE_2D, bm->bm_ogles_tex_id);
		image_data = (GLubyte*)malloc(bm->bm_w * bm->bm_h * 4);
		if (bm->bm_flags & BM_FLAG_RLE) {
			ubyte* expanded = (ubyte*)malloc(bm->bm_w * bm->bm_h * 3);
			sbits = &bm->bm_data[4 + bm->bm_h];
			dbits = expanded;
			for (int i = 0; i < bm->bm_h; i++) {
				gr_rle_decode(sbits, dbits);
				sbits += (int)bm->bm_data[4 + i];
				dbits += bm->bm_w;
			}
			data = expanded;
		}
		else {
			data = bm->bm_data;
		}
		for (i = 0; i < bm->bm_w * bm->bm_h; ++i) {
			image_data[i * 4] = gr_current_pal[data[i] * 3] * 4;
			image_data[i * 4 + 1] = gr_current_pal[data[i] * 3 + 1] * 4;
			image_data[i * 4 + 2] = gr_current_pal[data[i] * 3 + 2] * 4;

			image_data[i * 4 + 3] = data[i] == TRANSPARENCY_COLOR ? 0 : 255;
		}
		if (bm->bm_flags & BM_FLAG_RLE) {
			free(data);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bm->bm_w, bm->bm_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
		glGenerateMipmap(GL_TEXTURE_2D); // this is critical part

		free(image_data);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, bm->bm_ogles_tex_id);
	}
}

#endif

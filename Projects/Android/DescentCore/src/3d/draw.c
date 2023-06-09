/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
* $Source: Smoke:miner:source:3d::RCS:draw.c $
* $Revision: 1.5 $
* $Author: allender $
* $Date: 1995/10/11 00:27:17 $
*
* Drawing routines
*
* $Log: draw.c $
* Revision 1.5  1995/10/11  00:27:17  allender
* remove free_num_point settings to 0
*
* Revision 1.4  1995/09/14  14:08:27  allender
* co -l 3d.h
* g3_draw_sphere need to return value in new PPC stuff
*
* Revision 1.3  1995/09/13  11:30:35  allender
* removed checkmuldiv in PPC implementation
*
* Revision 1.2  1995/06/25  21:57:41  allender
* free_point_num problem
*
* Revision 1.1  1995/05/05  08:50:26  allender
* Initial revision
*
* Revision 1.1  1995/04/17  05:13:45  matt
* Initial revision
*
*
*/

#pragma off (unreferenced)
static char rcsid[] = "$Id: draw.c 1.5 1995/10/11 00:27:17 allender Exp $";
#pragma on (unreferenced)

#include "error.h"

#include "3d.h"
#include "globvars.h"
#include "texmap.h"
#include "clipper.h"
#ifdef OGLES
#include "drawogles.h"
#endif

void(*tmap_drawer_ptr)(grs_bitmap *bm, int nv, g3s_point **vertlist) = draw_tmap;
void(*flat_drawer_ptr)(int nv, int *vertlist) = gr_upoly_tmap;
int(*line_drawer_ptr)(fix x0, fix y0, fix x1, fix y1) = gr_line;

//specifies 2d drawing routines to use instead of defaults.  Passing
//NULL for either or both restores defaults
void g3_set_special_render(void *tmap_drawer(),void *flat_drawer(),void *line_drawer()) {
	tmap_drawer_ptr = (tmap_drawer) ? tmap_drawer : draw_tmap;
	flat_drawer_ptr = (flat_drawer) ? flat_drawer : gr_upoly_tmap;
	line_drawer_ptr = (line_drawer) ? line_drawer : gr_line;
}

//deal with a clipped line
bool must_clip_line(g3s_point *p0, g3s_point *p1, ubyte codes_or) {
	bool ret;

	if ((p0->p3_flags&PF_TEMP_POINT) || (p1->p3_flags&PF_TEMP_POINT))

		ret = 0;		//line has already been clipped, so give up

	else {

		clip_line(&p0, &p1, codes_or);

		ret = g3_draw_line(p0, p1);
	}

	//free temp points

	if (p0->p3_flags & PF_TEMP_POINT)
		free_temp_point(p0);

	if (p1->p3_flags & PF_TEMP_POINT)
		free_temp_point(p1);

	return ret;
}

//draws a line. takes two points.  returns true if drew
bool g3_draw_line(g3s_point *p0, g3s_point *p1) {
	ubyte codes_or;
	
#ifdef OGLES
	if (grd_curcanv->cv_bitmap.bm_type == BM_OGLES) {
		return g3_draw_line_ogles(p0, p1);
	}
#endif

	if (p0->p3_codes & p1->p3_codes)
		return 0;

	codes_or = p0->p3_codes | p1->p3_codes;

	if (codes_or & CC_BEHIND)
		return must_clip_line(p0, p1, codes_or);

	if (!(p0->p3_flags&PF_PROJECTED))
		g3_project_point(p0);

	if (p0->p3_flags&PF_OVERFLOW)
		return must_clip_line(p0, p1, codes_or);


	if (!(p1->p3_flags&PF_PROJECTED))
		g3_project_point(p1);

	if (p1->p3_flags&PF_OVERFLOW)
		return must_clip_line(p0, p1, codes_or);

	return (bool)(*line_drawer_ptr)(p0->p3_sx, p0->p3_sy, p1->p3_sx, p1->p3_sy);
}

//returns true if a plane is facing the viewer. takes the unrotated surface 
//normal of the plane, and a point on it.  The normal need not be normalized
bool g3_check_normal_facing(vms_vector *v, vms_vector *norm) {
	vms_vector tempv;

	vm_vec_sub(&tempv, &View_position, v);

	return (vm_vec_dot(&tempv, norm) > 0);
}

bool do_facing_check(vms_vector *norm, g3s_point **vertlist, vms_vector *p) {
	if (norm) {		//have normal

		Assert(norm->x || norm->y || norm->z);

		return g3_check_normal_facing(p, norm);
	}
	else {	//normal not specified, so must compute

		vms_vector tempv;

		//get three points (rotated) and compute normal

		vm_vec_perp(&tempv, &vertlist[0]->p3_vec, &vertlist[1]->p3_vec, &vertlist[2]->p3_vec);

		return (vm_vec_dot(&tempv, &vertlist[1]->p3_vec) < 0);
	}
}

//like g3_draw_poly(), but checks to see if facing.  If surface normal is
//NULL, this routine must compute it, which will be slow.  It is better to 
//pre-compute the normal, and pass it to this function.  When the normal
//is passed, this function works like g3_check_normal_facing() plus
//g3_draw_poly().
//returns -1 if not facing, 1 if off screen, 0 if drew
bool g3_check_and_draw_poly(int nv, g3s_point **pointlist, vms_vector *norm, vms_vector *pnt) {
	if (do_facing_check(norm, pointlist, pnt))
		return g3_draw_poly(nv, pointlist);
	else
		return 255;
}

bool g3_check_and_draw_tmap(int nv, g3s_point **pointlist, g3s_uvl *uvl_list, grs_bitmap *bm, vms_vector *norm, vms_vector *pnt) {
	if (do_facing_check(norm, pointlist, pnt))
		return g3_draw_tmap(nv, pointlist, uvl_list, bm);
	else
		return 255;
}

//deal with face that must be clipped
bool must_clip_flat_face(int nv, g3s_codes cc) {
	int i;
	bool ret = false;
	g3s_point **bufptr;

	bufptr = clip_polygon(Vbuf0, Vbuf1, &nv, &cc);

	if (nv>0 && !(cc.uor &CC_BEHIND) && !cc.uand) {

		for (i = 0; i<nv; i++) {
			g3s_point *p = bufptr[i];

			if (!(p->p3_flags&PF_PROJECTED))
				g3_project_point(p);

			if (p->p3_flags&PF_OVERFLOW) {
				ret = 1;
				goto free_points;
			}

			Vertex_list[i * 2] = p->p3_sx;
			Vertex_list[i * 2 + 1] = p->p3_sy;
		}

		(*flat_drawer_ptr)(nv, (int *)Vertex_list);
	}
	else
		ret = 1;

	//free temp points
free_points:
	;

	for (i = 0; i<nv; i++)
		if (Vbuf1[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(Vbuf1[i]);

	//	Assert(free_point_num==0);

	return ret;
}

//draw a flat-shaded face.
//returns 1 if off screen, 0 if drew
bool g3_draw_poly(int nv, g3s_point **pointlist) {
#ifdef OGLES
	return g3_draw_poly_ogles(nv, pointlist);
#else
	int i;
	g3s_point **bufptr;
	g3s_codes cc;

	cc.uor = 0; cc.uand = 0xff;

	bufptr = Vbuf0;

	for (i = 0; i<nv; i++) {

		bufptr[i] = pointlist[i];

		cc.uand &= bufptr[i]->p3_codes;
		cc.uor |= bufptr[i]->p3_codes;
	}

	if (cc.uand)
		return 1;	//all points off screen

	if (cc.uor )
		return must_clip_flat_face(nv, cc);

	//now make list of 2d coords (& check for overflow)

	for (i = 0; i<nv; i++) {
		g3s_point *p = bufptr[i];

		if (!(p->p3_flags&PF_PROJECTED))
			g3_project_point(p);

		if (p->p3_flags&PF_OVERFLOW)
			return must_clip_flat_face(nv, cc);

		Vertex_list[i * 2] = p->p3_sx;
		Vertex_list[i * 2 + 1] = p->p3_sy;
	}

	(*flat_drawer_ptr)(nv, (int *)Vertex_list);

	return 0;	//say it drew
#endif
}

bool must_clip_tmap_face(int nv, g3s_codes cc, grs_bitmap *bm);
void draw_tmap_flat(grs_bitmap *bp,int nverts,g3s_point **vertbuf);

//draw a texture-mapped face.
//returns 1 if off screen, 0 if drew
bool g3_draw_tmap(int nv, g3s_point **pointlist, g3s_uvl *uvl_list, grs_bitmap *bm) {
#ifdef OGLES
	fix average_light;
	int i;
	
	if (tmap_drawer_ptr == draw_tmap_flat) {
		average_light = uvl_list[0].l;
		for (i=1; i<nv; i++)
			average_light += uvl_list[i].l;
		if (nv == 4)
			average_light = f2i(average_light * NUM_LIGHTING_LEVELS/4);
		else
			average_light = f2i(average_light * NUM_LIGHTING_LEVELS/nv);
		if (average_light < 0)
			average_light = 0;
		else if (average_light > NUM_LIGHTING_LEVELS-1)
			average_light = NUM_LIGHTING_LEVELS-1;
		gr_setcolor(gr_fade_table[average_light*256]);
		return g3_draw_poly_ogles(nv, pointlist);
	} else {
		return g3_draw_tmap_ogles(nv, pointlist, uvl_list, bm);
	}
#else
	int i;
	g3s_point **bufptr;
	g3s_codes cc;
	
	cc.uor = 0; cc.uand = 0xff;

	bufptr = Vbuf0;

	for (i = 0; i<nv; i++) {
		g3s_point *p;

		p = bufptr[i] = pointlist[i];

		cc.uand &= p->p3_codes;
		cc.uor |= p->p3_codes;

		p->p3_u = uvl_list[i].u;
		p->p3_v = uvl_list[i].v;
		p->p3_l = uvl_list[i].l;

		p->p3_flags |= PF_UVS + PF_LS;

	}

	if (cc.uand)
		return 1;	//all points off screen

	if (cc.uor )
		return must_clip_tmap_face(nv, cc, bm);

	//now make list of 2d coords (& check for overflow)

	for (i = 0; i<nv; i++) {
		g3s_point *p = bufptr[i];

		if (!(p->p3_flags&PF_PROJECTED))
			g3_project_point(p);

		if (p->p3_flags&PF_OVERFLOW) {
			Int3();		//should not overflow after clip
			return 255;
		}
	}

	(*tmap_drawer_ptr)(bm, nv, bufptr);

	return 0;	//say it drew
#endif
}

bool must_clip_tmap_face(int nv, g3s_codes cc, grs_bitmap *bm) {
	g3s_point **bufptr;
	int i;

	bufptr = clip_polygon(Vbuf0, Vbuf1, &nv, &cc);

	if (nv && !(cc.uor &CC_BEHIND) && !cc.uand) {

		for (i = 0; i<nv; i++) {
			g3s_point *p = bufptr[i];

			if (!(p->p3_flags&PF_PROJECTED))
				g3_project_point(p);

			if (p->p3_flags&PF_OVERFLOW) {
				Int3();		//should not overflow after clip
				goto free_points;
			}
		}

		(*tmap_drawer_ptr)(bm, nv, bufptr);
	}

free_points:
	;

	for (i = 0; i<nv; i++)
		if (bufptr[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(bufptr[i]);

	//	Assert(free_point_num==0);

	return 0;

}

#ifndef __powerc
int checkmuldiv(fix *r, fix a, fix b, fix c);
#endif

//draw a sortof sphere - i.e., the 2d radius is proportional to the 3d
//radius, but not to the distance from the eye
int g3_draw_sphere(g3s_point *pnt, fix rad) {
#ifdef OGLES
	if (grd_curcanv->cv_bitmap.bm_type == BM_OGLES) {
		return g3_draw_sphere_ogles(pnt, rad);
	}
#endif
	if (!(pnt->p3_codes & CC_BEHIND)) {

		if (!(pnt->p3_flags & PF_PROJECTED))
			g3_project_point(pnt);

		if (!(pnt->p3_codes & PF_OVERFLOW)) {
			fix r2, t;

			r2 = fixmul(rad, Matrix_scale.x);
#ifndef __powerc
			if (checkmuldiv(&t, r2, Canv_w2, pnt->p3_z))
				return gr_disk(pnt->p3_sx, pnt->p3_sy, t);
#else
			if (pnt->p3_z == 0)
				return 0;
			return gr_disk(pnt->p3_sx, pnt->p3_sy, fl2f(((f2fl(r2) * fCanv_w2) / f2fl(pnt->p3_z))));
#endif
		}
	}

	return 0;
}

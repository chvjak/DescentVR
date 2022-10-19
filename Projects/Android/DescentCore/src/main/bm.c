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
 * $Source: f:/miner/source/main/rcs/bm.c $
 * $Revision: 2.3 $
 * $Author: john $
 * $Date: 1995/03/14 16:22:04 $
 *
 * Bitmap and palette loading functions.
 *
 * $Log: bm.c $
 * Revision 2.3  1995/03/14  16:22:04  john
 * Added cdrom alternate directory stuff.
 * 
 * Revision 2.2  1995/03/07  16:51:48  john
 * Fixed robots not moving without edtiro bug.
 * 
 * Revision 2.1  1995/03/06  15:23:06  john
 * New screen techniques.
 * 
 * Revision 2.0  1995/02/27  11:27:05  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: bm.c 2.3 1995/03/14 16:22:04 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "mem.h"
#include "cflib.h"
#include "mono.h"
#include "error.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff.h"
#include "cfile.h"
#include "hostage.h"
#include "powerup.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"

ubyte Sounds[MAX_SOUNDS];
ubyte AltSounds[MAX_SOUNDS];

int Num_total_object_types;

byte	ObjType[MAX_OBJTYPE];
byte	ObjId[MAX_OBJTYPE];
fix	ObjStrength[MAX_OBJTYPE];

//for each model, a model number for dying & dead variants, or -1 if none
int Dying_modelnums[MAX_POLYGON_MODELS];
int Dead_modelnums[MAX_POLYGON_MODELS];

//right now there's only one player ship, but we can have another by 
//adding an array and setting the pointer to the active ship.
player_ship only_player_ship,*Player_ship=&only_player_ship;

//----------------- Miscellaneous bitmap pointers ---------------
int					Num_cockpits = 0;
bitmap_index		cockpit_bitmap[N_COCKPIT_BITMAPS];

//---------------- Variables for wall textures ------------------
int 					Num_tmaps;
tmap_info 			TmapInfo[MAX_TEXTURES];

//---------------- Variables for object textures ----------------

int					First_multi_bitmap_num=-1;

bitmap_index		ObjBitmaps[MAX_OBJ_BITMAPS];
ushort				ObjBitmapPtrs[MAX_OBJ_BITMAPS];		// These point back into ObjBitmaps, since some are used twice.

//-----------------------------------------------------------------
// Initializes all bitmaps from BITMAPS.TBL file.
int bm_init()
{
	init_polygon_models();
	piggy_init();				// This calls bm_read_all
	piggy_read_sounds();
	init_endlevel();		//this is here so endlevel bitmaps go into pig
	return 0;
}

// For 64-bit compatibility
void read_polymodels(CFILE *fp) {
	int model_data;

	for (int i = 0; i < N_polygon_models; ++i) {
		cfread(&Polygon_models[i].n_models, sizeof(int), 1, fp);
		cfread(&Polygon_models[i].model_data_size, sizeof(int), 1, fp);
		cfread(&model_data, sizeof(int), 1, fp);
		cfread(&Polygon_models[i].submodel_ptrs, sizeof(int), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_offsets, sizeof(vms_vector), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_norms, sizeof(vms_vector), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_pnts, sizeof(vms_vector), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_rads, sizeof(fix), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_parents, sizeof(ubyte), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_mins, sizeof(vms_vector), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].submodel_maxs, sizeof(vms_vector), MAX_SUBMODELS, fp);
		cfread(&Polygon_models[i].mins, sizeof(vms_vector), 1, fp);
		cfread(&Polygon_models[i].maxs, sizeof(vms_vector), 1, fp);
		cfread(&Polygon_models[i].rad, sizeof(fix), 1, fp);
		cfread(&Polygon_models[i].n_textures, sizeof(ubyte), 1, fp);
		cfread(&Polygon_models[i].first_texture, sizeof(ushort), 1, fp);
		cfread(&Polygon_models[i].simpler_model, sizeof(ubyte), 1, fp);
		Polygon_models[i].model_data = (void*)model_data;
	}
}

void bm_read_all(CFILE * fp)
{
	int i;

	cfread( &NumTextures, sizeof(int), 1, fp );
	cfread( Textures, sizeof(bitmap_index), MAX_TEXTURES, fp );
	cfread( TmapInfo, sizeof(tmap_info), MAX_TEXTURES, fp );
	
	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	cfread( &Num_vclips, sizeof(int), 1, fp );
	cfread( Vclip, sizeof(vclip), VCLIP_MAXNUM, fp );

	cfread( &Num_effects, sizeof(int), 1, fp );
	cfread( Effects, sizeof(eclip), MAX_EFFECTS, fp );

	cfread( &Num_wall_anims, sizeof(int), 1, fp );
	cfread( WallAnims, sizeof(wclip), MAX_WALL_ANIMS, fp );

	cfread( &N_robot_types, sizeof(int), 1, fp );
	cfread( Robot_info, sizeof(robot_info), MAX_ROBOT_TYPES, fp );

	cfread( &N_robot_joints, sizeof(int), 1, fp );
	cfread( Robot_joints, sizeof(jointpos), MAX_ROBOT_JOINTS, fp );

	cfread( &N_weapon_types, sizeof(int), 1, fp );
	cfread( Weapon_info, sizeof(weapon_info), MAX_WEAPON_TYPES, fp );

	cfread( &N_powerup_types, sizeof(int), 1, fp );
	cfread( Powerup_info, sizeof(powerup_type_info), MAX_POWERUP_TYPES, fp );
	
	cfread( &N_polygon_models, sizeof(int), 1, fp );
	read_polymodels(fp);

	for (i=0; i<N_polygon_models; i++ )	{
		Polygon_models[i].model_data = malloc(Polygon_models[i].model_data_size);
		Assert( Polygon_models[i].model_data != NULL );
		cfread( Polygon_models[i].model_data, sizeof(ubyte), Polygon_models[i].model_data_size, fp );
	}

	cfread( Gauges, sizeof(bitmap_index), MAX_GAUGE_BMS, fp );

	cfread( Dying_modelnums, sizeof(int), MAX_POLYGON_MODELS, fp );
	cfread( Dead_modelnums, sizeof(int), MAX_POLYGON_MODELS, fp );

	cfread( ObjBitmaps, sizeof(bitmap_index), MAX_OBJ_BITMAPS, fp );
	cfread( ObjBitmapPtrs, sizeof(ushort), MAX_OBJ_BITMAPS, fp );

	cfread( &only_player_ship, sizeof(player_ship), 1, fp );
	
	// This is a hack to make the ship turn faster
	// Seems to make the game more enjoyable on iOS
	only_player_ship.max_rotthrust *= 2;

	cfread( &Num_cockpits, sizeof(int), 1, fp );
	cfread( cockpit_bitmap, sizeof(bitmap_index), N_COCKPIT_BITMAPS, fp );

	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	cfread( &Num_total_object_types, sizeof(int), 1, fp );
	cfread( ObjType, sizeof(byte), MAX_OBJTYPE, fp );
	cfread( ObjId, sizeof(byte), MAX_OBJTYPE, fp );
	cfread( ObjStrength, sizeof(fix), MAX_OBJTYPE, fp );

	cfread( &First_multi_bitmap_num, sizeof(int), 1, fp );

	cfread( &N_controlcen_guns, sizeof(int), 1, fp );
	cfread( controlcen_gun_points, sizeof(vms_vector), MAX_CONTROLCEN_GUNS, fp );
	cfread( controlcen_gun_dirs, sizeof(vms_vector), MAX_CONTROLCEN_GUNS, fp );
	cfread( &exit_modelnum, sizeof(int), 1, fp );
	cfread( &destroyed_exit_modelnum, sizeof(int), 1, fp );
}

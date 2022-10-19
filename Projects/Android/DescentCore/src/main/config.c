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
 * $Source: f:/miner/source/main/rcs/config.c $
 * $Revision: 2.2 $
 * $Author: john $
 * $Date: 1995/03/27 09:42:59 $
 * 
 * contains routine(s) to read in the configuration file which contains
 * game configuration stuff like detail level, sound card, etc
 * 
 * $Log: config.c $
 * Revision 2.2  1995/03/27  09:42:59  john
 * Added VR Settings in config file.
 * 
 * Revision 2.1  1995/03/16  11:20:40  john
 * Put in support for Crystal Lake soundcard.
 * 
 * Revision 2.0  1995/02/27  11:30:13  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.14  1995/02/11  16:19:36  john
 * Added code to make the default mission be the one last played.
 * 
 * Revision 1.13  1995/01/18  13:23:24  matt
 * Made curtom detail level vars initialize properly at load
 * 
 * Revision 1.12  1995/01/04  22:15:36  matt
 * Fixed stupid bug using scanf() to read bytes
 * 
 * Revision 1.11  1995/01/04  13:14:21  matt
 * Made custom detail level settings save in config file
 * 
 * Revision 1.10  1994/12/12  21:35:09  john
 * *** empty log message ***
 * 
 * Revision 1.9  1994/12/12  21:31:51  john
 * Made volume work better by making sure volumes are valid
 * and set correctly at program startup.
 * 
 * Revision 1.8  1994/12/12  13:58:01  john
 * MAde -nomusic work.
 * Fixed GUS hang at exit by deinitializing digi before midi.
 * 
 * Revision 1.7  1994/12/08  10:01:33  john
 * Changed the way the player callsign stuff works.
 * 
 * Revision 1.6  1994/12/01  11:24:07  john
 * Made volume/gamma/joystick sliders all be the same length.  0-->8.
 * 
 * Revision 1.5  1994/11/29  02:01:07  john
 * Added code to look at -volume command line arg.
 * 
 * Revision 1.4  1994/11/14  20:14:11  john
 * Fixed some warnings.
 * 
 * Revision 1.3  1994/11/14  19:51:01  john
 * Added joystick cal values to descent.cfg.
 * 
 * Revision 1.2  1994/11/14  17:53:09  allender
 * read and write descent.cfg file
 * 
 * Revision 1.1  1994/11/14  16:28:08  allender
 * Initial revision
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "game.h"
#include "digi.h"
#include "kconfig.h"
#include "palette.h"
#include "joy.h"
#include "args.h"
#include "player.h"
#include "mission.h"
#include "cfile.h"

#pragma pack (4)						// Use 32-bit packing!
#pragma off (check_stack);			// No stack checking!
//#include "sos.h"//These sos headers are part of a commercial library, and aren't included-KRB
//#include "sosm.h"

#pragma off (unreferenced)
static char rcsid[] = "$Id: config.c 2.2 1995/03/27 09:42:59 john Exp $";
#pragma on (unreferenced)

static char *digi_dev_str = "DigiDeviceID";
static char *digi_port_str = "DigiPort";
static char *digi_irq_str = "DigiIrq";
static char *digi_dma_str = "DigiDma";
static char *digi_volume_str = "DigiVolume";
static char *midi_volume_str = "MidiVolume";
static char *midi_dev_str = "MidiDeviceID";
static char *midi_port_str = "MidiPort";
static char *detail_level_str = "DetailLevel";
static char *gamma_level_str = "GammaLevel";
static char *stereo_rev_str = "StereoReverse";
static char *joystick_min_str = "JoystickMin";
static char *joystick_max_str = "JoystickMax";
static char *joystick_cen_str = "JoystickCen";
static char *last_player_str = "LastPlayer";
static char *last_mission_str = "LastMission";
static char *config_vr_type_str = "VR_type";
static char *config_vr_tracking_str = "VR_tracking";


char config_last_player[CALLSIGN_LEN+1] = "";
char config_last_mission[MISSION_NAME_LEN+1] = "";

int Config_digi_type = 0;
int Config_midi_type = 0;

int Config_vr_type = 0;
int Config_vr_tracking = 0;

extern byte	Object_complexity, Object_detail, Wall_detail, Wall_render_depth, Debris_amount, SoundChannels;

void set_custom_detail_vars(void);


#define CL_MC0 0xF8F
#define CL_MC1 0xF8D

void CrystalLakeWriteMCP( ushort mc_addr, ubyte mc_data )
{
}

ubyte CrystalLakeReadMCP( ushort mc_addr )
{
	ubyte value;
	return value;
}

void CrystalLakeSetSB()
{
	ubyte tmp;
	tmp = CrystalLakeReadMCP( CL_MC1 );
	tmp &= 0x7F;
	CrystalLakeWriteMCP( CL_MC1, tmp );
}

void CrystalLakeSetWSS()
{
	ubyte tmp;
	tmp = CrystalLakeReadMCP( CL_MC1 );
	tmp |= 0x80;
	CrystalLakeWriteMCP( CL_MC1, tmp );
}

int ReadConfigFile()
{
	FILE *infile;
	char line[80], filename[FILENAME_MAX], *token, *value, *ptr;
	ubyte gamma;
	int joy_axis_min[4];
	int joy_axis_center[4];
	int joy_axis_max[4];
	int i;

	strcpy( config_last_player, "" );

	joy_axis_min[0] = joy_axis_min[1] = joy_axis_min[2] = joy_axis_min[3] = 0;
	joy_axis_max[0] = joy_axis_max[1] = joy_axis_max[2] = joy_axis_max[3] = 0;
	joy_axis_center[0] = joy_axis_center[1] = joy_axis_center[2] = joy_axis_center[3] = 0;
	joy_set_cal_vals(joy_axis_min, joy_axis_center, joy_axis_max);

	digi_driver_board = 0;
	digi_driver_port = 0;
	digi_driver_irq = 0;
	digi_driver_dma = 0;

	digi_midi_type = 0;
	digi_midi_port = 0;

	Config_digi_volume = 4;
	Config_midi_volume = 4;
	Config_control_type = 5;
	Config_channels_reversed = 0;

	sprintf(filename, "%s/%s", Document_path, "descent.cfg");
	infile = fopen(filename, "rt");
	if (infile == NULL) {
		return 1;
	}
	while (!feof(infile)) {
		memset(line, 0, 80);
		fgets(line, 80, infile);
		ptr = &(line[0]);
		while (isspace(*ptr))
			ptr++;
		if (*ptr != '\0') {
			token = strtok(ptr, "=");
			value = strtok(NULL, "=");
			if (!strcmp(token, digi_dev_str))
				digi_driver_board = strtol(value, NULL, 16);
			else if (!strcmp(token, digi_port_str))
				digi_driver_port = strtol(value, NULL, 16);
			else if (!strcmp(token, digi_irq_str))
				digi_driver_irq = strtol(value, NULL, 10);
			else if (!strcmp(token, digi_dma_str))
				digi_driver_dma = strtol(value, NULL, 10);
			else if (!strcmp(token, digi_volume_str))
				Config_digi_volume = strtol(value, NULL, 10);
			else if (!strcmp(token, midi_dev_str))
				digi_midi_type = strtol(value, NULL, 16);
			else if (!strcmp(token, midi_port_str))
				digi_midi_port = strtol(value, NULL, 16);
			else if (!strcmp(token, midi_volume_str))
				Config_midi_volume = strtol(value, NULL, 10);
			else if (!strcmp(token, stereo_rev_str))
				Config_channels_reversed = strtol(value, NULL, 10);
			else if (!strcmp(token, gamma_level_str)) {
				gamma = strtol(value, NULL, 10);
				gr_palette_set_gamma( gamma );
			}
			else if (!strcmp(token, detail_level_str)) {
				Detail_level = strtol(value, NULL, 10);
				if (Detail_level == NUM_DETAIL_LEVELS-1) {
					int count,dummy,oc,od,wd,wrd,da,sc;

					count = sscanf (value, "%d,%d,%d,%d,%d,%d,%d\n",&dummy,&oc,&od,&wd,&wrd,&da,&sc);

					if (count == 7) {
						Object_complexity = oc;
						Object_detail = od;
						Wall_detail = wd;
						Wall_render_depth = wrd;
						Debris_amount = da;
						SoundChannels = sc;
						set_custom_detail_vars();
					}
				}
			}
			else if (!strcmp(token, joystick_min_str))	{
				sscanf( value, "%d,%d,%d,%d", &joy_axis_min[0], &joy_axis_min[1], &joy_axis_min[2], &joy_axis_min[3] );
			} 
			else if (!strcmp(token, joystick_max_str))	{
				sscanf( value, "%d,%d,%d,%d", &joy_axis_max[0], &joy_axis_max[1], &joy_axis_max[2], &joy_axis_max[3] );
			}
			else if (!strcmp(token, joystick_cen_str))	{
				sscanf( value, "%d,%d,%d,%d", &joy_axis_center[0], &joy_axis_center[1], &joy_axis_center[2], &joy_axis_center[3] );
			}
			else if (!strcmp(token, last_player_str))	{
				char * p;
				strncpy( config_last_player, value, CALLSIGN_LEN );
				p = strchr( config_last_player, '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, last_mission_str))	{
				char * p;
				strncpy( config_last_mission, value, MISSION_NAME_LEN );
				p = strchr( config_last_mission, '\n');
				if ( p ) *p = 0;
			} else if (!strcmp(token, config_vr_type_str)) {
				Config_vr_type = strtol(value, NULL, 10);
			} else if (!strcmp(token, config_vr_tracking_str)) {
				Config_vr_tracking = strtol(value, NULL, 10);
			}
		}
	}

	fclose(infile);

	i = FindArg( "-volume" );
	
	if ( i > 0 )	{
		i = atoi( Args[i+1] );
		if ( i < 0 ) i = 0;
		if ( i > 100 ) i = 100;
		Config_digi_volume = (i*8)/100;
		Config_midi_volume = (i*8)/100;
	}

	if ( Config_digi_volume > 8 ) Config_digi_volume = 8;

	if ( Config_midi_volume > 8 ) Config_midi_volume = 8;

	joy_set_cal_vals(joy_axis_min, joy_axis_center, joy_axis_max);
	digi_set_volume( (Config_digi_volume*32768)/8, (Config_midi_volume*128)/8 );
/*
	printf( "DigiDeviceID: 0x%x\n", digi_driver_board );
	printf( "DigiPort: 0x%x\n", digi_driver_port		);
	printf( "DigiIrq: 0x%x\n",  digi_driver_irq		);
	printf( "DigiDma: 0x%x\n",	digi_driver_dma	);
	printf( "MidiDeviceID: 0x%x\n", digi_midi_type	);
	printf( "MidiPort: 0x%x\n", digi_midi_port		);
  	key_getch();
*/

	Config_midi_type = digi_midi_type;
	Config_digi_type = digi_driver_board;

	// HACK!!! 
	//Hack to make the Crytal Lake look like Microsoft Sound System
	if ( digi_driver_board == 0xe200 )	{
		ubyte tmp;
		tmp = CrystalLakeReadMCP( CL_MC1 );
		if ( !(tmp & 0x80) )
			atexit( CrystalLakeSetSB );		// Restore to SB when done.
	 	CrystalLakeSetWSS();
		digi_driver_board = 0;//_MICROSOFT_8_ST;<was this microsoft thing, but its irrelevant, because we have no sound here yet,being that its also undefined, I set it to 0 -KRB
	}

	return 0;
}

int WriteConfigFile()
{
	FILE *infile;
	char str[256], filename[FILENAME_MAX];
	int joy_axis_min[4];
	int joy_axis_center[4];
	int joy_axis_max[4];
	ubyte gamma = gr_palette_get_gamma();
	
	joy_get_cal_vals(joy_axis_min, joy_axis_center, joy_axis_max);

	sprintf(filename, "%s/%s", Document_path, "descent.cfg");
	infile = fopen(filename, "wt");
	if (infile == NULL) {
		return 1;
	}
	sprintf (str, "%s=0x%x\n", digi_dev_str, Config_digi_type);
	fputs(str, infile);
	sprintf (str, "%s=0x%x\n", digi_port_str, digi_driver_port);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", digi_irq_str, digi_driver_irq);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", digi_dma_str, digi_driver_dma);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", digi_volume_str, Config_digi_volume);
	fputs(str, infile);
	sprintf (str, "%s=0x%x\n", midi_dev_str, Config_midi_type);
	fputs(str, infile);
	sprintf (str, "%s=0x%x\n", midi_port_str, digi_midi_port);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", midi_volume_str, Config_midi_volume);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", stereo_rev_str, Config_channels_reversed);
	fputs(str, infile);
	sprintf (str, "%s=%d\n", gamma_level_str, gamma);
	fputs(str, infile);
	if (Detail_level == NUM_DETAIL_LEVELS-1)
		sprintf (str, "%s=%d,%d,%d,%d,%d,%d,%d\n", detail_level_str, Detail_level,
				Object_complexity,Object_detail,Wall_detail,Wall_render_depth,Debris_amount,SoundChannels);
	else
		sprintf (str, "%s=%d\n", detail_level_str, Detail_level);
	fputs(str, infile);
	sprintf (str, "%s=%d,%d,%d,%d\n", joystick_min_str, joy_axis_min[0], joy_axis_min[1], joy_axis_min[2], joy_axis_min[3] );
	fputs(str, infile);
	sprintf (str, "%s=%d,%d,%d,%d\n", joystick_cen_str, joy_axis_center[0], joy_axis_center[1], joy_axis_center[2], joy_axis_center[3] );
	fputs(str, infile);
	sprintf (str, "%s=%d,%d,%d,%d\n", joystick_max_str, joy_axis_max[0], joy_axis_max[1], joy_axis_max[2], joy_axis_max[3] );
	fputs(str, infile);
	sprintf (str, "%s=%s\n", last_player_str, Players[Player_num].callsign );
	fputs(str, infile);
	sprintf (str, "%s=%s\n", last_mission_str, config_last_mission );
	fputs(str, infile);
	sprintf (str, "%s=%d\n", config_vr_type_str, Config_vr_type );
	fputs(str, infile);
	sprintf (str, "%s=%d\n", config_vr_tracking_str, Config_vr_tracking );
	fputs(str, infile);
	fclose(infile);
	return 0;
}

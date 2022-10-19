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
 * $Source: f:/miner/source/bios/rcs/dpmi.h $
 * $Revision: 1.9 $
 * $Author: john $
 * $Date: 1995/01/14 19:20:14 $
 * 
 * Prototypes for DPMI services.
 * 
 * $Log: dpmi.h $
 * Revision 1.9  1995/01/14  19:20:14  john
 * Added function to set a selector's base address.
 * 
 * Revision 1.8  1994/11/28  20:22:03  john
 * Added some variables that return the amount of available 
 * memory.
 * 
 * Revision 1.7  1994/11/15  18:26:38  john
 * Added verbose flag.
 * 
 * Revision 1.6  1994/11/07  11:35:05  john
 * Added prototype for real_free
 * 
 * Revision 1.5  1994/10/27  19:54:48  john
 * Added unlock region function,.
 * 
 * Revision 1.4  1994/09/27  18:27:56  john
 * Added pragma to make inp,outp,enable,disable intrinsic
 * 
 * Revision 1.3  1994/09/27  11:54:45  john
 * Added DPMI init function.
 * 
 * Revision 1.2  1994/08/24  18:53:51  john
 * Made Cyberman read like normal mouse; added dpmi module; moved
 * mouse from assembly to c. Made mouse buttons return time_down.
 * 
 * Revision 1.1  1994/08/24  10:22:48  john
 * Initial revision
 * 
 * 
 */

#ifndef _DPMI_H
#define _DPMI_H

#ifdef far
#undef far
#endif
#define far

#include "types.h"

typedef struct dpmi_real_regs {
    uint edi;
    uint esi;
    uint ebp;
    uint reserved_by_system;
    uint ebx;
    uint edx;
    uint ecx;
    uint eax;
    ushort flags;
    ushort es,ds,fs,gs,ip,cs,sp,ss;
} dpmi_real_regs;

#pragma intrinsic( inp );
#pragma intrinsic( outp );
#pragma intrinsic( _enable );
#pragma intrinsic( _disable );

#define DPMI_real_segment(P)	((((ulong) (P)) >> 4) & 0xFFFF)
#define DPMI_real_offset(P)	(((ulong) (P)) & 0xF)

// Initializes dpmi. Returns zero if failed.
extern int dpmi_init(int verbose);
// Returns a pointer to a temporary dos memory block. Size must be < 1024 bytes.
extern void *dpmi_get_temp_low_buffer( int size );
extern void *dpmi_real_malloc( int size, ushort *selector );
extern void dpmi_real_free( ushort selector );
extern void dpmi_real_int386x( ubyte intno, dpmi_real_regs * rregs );
extern void dpmi_real_call(dpmi_real_regs * rregs);
extern int dpmi_lock_region(void *address, unsigned length);
extern int dpmi_unlock_region(void *address, unsigned length);
// returns 0 if failed...
extern int dpmi_allocate_selector( void * address, int size, ushort * selector );
extern int dpmi_modify_selector_base( ushort selector, void * address );
extern int dpmi_modify_selector_limit( ushort selector, int size  );


// Sets the PM handler. Returns 0 if succssful
extern int dpmi_set_pm_handler(unsigned intnum, void far * isr );

extern unsigned int dpmi_virtual_memory;
extern unsigned int dpmi_available_memory;
extern unsigned int dpmi_physical_memory;
extern long dpmi_dos_memory;

#endif

/* mib_definitions.h
 * ALl of the MIB related structures and constant values shared between various mib
 * modules are stored here.  All declarations that are not constant values using #define
 * must be surrounded by #ifndef __DEFINES_ONLY__ guards to allow for this file to be
 * included by asm modules.
 */

#ifndef __mib_definitions_h__
#define __mib_definitions_h__

#include "common_types.h"

//Defined Flags

/*
 * The structure returned by a mib module upon registration, describing itself
 * Fields have the following meanings:
 * module_type - a module class specific identifier
 * hardware_type - the type of processor this application is running on
 * mib_revision - the version of the mib protocol that this module speaks
 * feature_count - the number of features supported by the module
 * flags - a bit array of special interest flags about the module
 * name - a 7 byte string with a user readable name for the module
 */

//Module descriptor size: 11 bytes
typedef struct 
{
	uint8	hardware_type;
	uint8 	module_type;
	uint8	mib_revision : 4;
	uint8	flags : 4;

	uint8 	name[7];
	uint8	feature_count;
} momo_module_descriptor;

#define kModuleDescriptorSize sizeof(momo_module_descriptor)

//Macros for defining parameter lists
#define plist_ints(count)		((count&0b11) << 5)
#define plist_buffer()          0b10000000

#define plist_spec_empty()      0
#define plist_spec(ints,buffer) (((buffer&0b1)<<7) | plist_ints(ints))
#define plist_spec_mask         0b11100000
#define plist_buffer_mask		0b00011111

#define plist_with_buffer(ints,buffer_length) (plist_spec(ints, 1)|((buffer_length)&0x1F))
#define plist_no_buffer(ints)	plist_ints(ints)
#define plist_empty()			plist_spec_empty()

#define plist_matches(plist,spec)     ((plist & plist_spec_mask) == spec)

#define plist_set_int32(n, val)			((uint16*)mib_data.buffer)[n>>1] = val
#define plist_set_int16(n, val)			((int*)(mib_unified.mib_buffer))[n] = val
#define plist_set_int8(n, hi, val)		mib_unified.mib_buffer[(n<<1) + hi] = val
#define plist_get_int16(n)				(*(int*)((mib_unified.mib_buffer + (2*n))))
#define plist_get_int8(n)				mib_unified.mib_buffer[n<<1]
#define plist_get_buffer(n)				(mib_unified.mib_buffer + (n << 1))
#define plist_get_buffer_length()		(mib_unified.bus_command.param_spec & 0b00011111)

#endif
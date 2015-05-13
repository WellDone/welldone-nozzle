#ifndef __executive_state_h__
#define __executive_state_h__

#include "protocol.h"

//3 bytes long
typedef struct 
{
	//Master variables
	uint8   slave_address;
	uint8	send_address;

	//Slave variables
	uint8 	curr_loc;
} MIBState;

//1 Byte
//These bits are referenced in c and assembly code so they cannot change position without
//also updating asm_locations.h
typedef union
{
	struct
	{
		uint8 respond_busy 		: 1;
		uint8 valid_app 		: 1;
		uint8 async_callback 	: 1;
		uint8 registered		: 1;
		uint8 dirty_reset 		: 1; 
		uint8 master_waiting 	: 1;
		uint8 first_read		: 1;
		uint8 trapped			: 1;
	};

	uint8_t status;
} MIBExecutiveStatus;

extern MIBPacket						mib_packet;
extern MIBState 						mib_state;
extern __persistent MIBExecutiveStatus	status;

#endif

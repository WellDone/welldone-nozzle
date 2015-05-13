;Assembly routines for porting executive.c over from xc8 c to assembly code.
;We need to do this because xc8 is terrible and uses too much memory for
;stupid things like moving W to a GPR register and back just to make a
;zero flag appear (when that's required, a simple movwf WREG would accomplish
;the same thing without wasting a byte of RAM).


#include <xc.inc>
#define _DEFINES_ONLY
#include "bootloader.h"
#undef  __DEFINES_ONLY__

#include "i2c_defines.h"
#include "asm_locations.h"
#include "constants.h"
#include "protocol_defines.h"


ASM_INCLUDE_GLOBALS()

global _exec_readmem, _exec_status, _mib_to_fsr0, _copy_fsr, _verify_application
global _bus_slave_returndata

PSECT text_executive_asm,local,class=CODE,delta=2

;Function _exec_readmem
;MIB Endpoint to read 20 bytes from data memory
;First int parameter is the indirect address to start copying
;20 bytes from.  Those bytes are returned to the rpc caller.
BEGINFUNCTION _exec_readmem
	call 	_mib_to_fsr0
	moviw	[0]FSR0
	movwf 	FSR1L
	moviw 	[1]FSR0
	movwf 	FSR1H

	movlw	20
	call 	_copy_fsr
	
	movlw   20
	call 	_bus_slave_returndata
	retlw 	0x00
ENDFUNCTION _exec_readmem

BEGINFUNCTION _exec_async_response
	banksel _status

	;FIXME: If we were not waiting for an async response, tell the caller that
	bcf BANKMASK(_status), MasterWaitingBit

	;Move the length into the right place as if the call were not asynchronous
  	movf BANKMASK(bus_spec),w
  	movwf BANKMASK(bus_length)
  	
	retlw 0x00
ENDFUNCTION _exec_async_response

BEGINFUNCTION _exec_status
	call 	_mib_to_fsr0
	movlw	kExecutiveSerialNumber
	movwi 	[0]FSR0
	movlw 	kModuleHWType
	movwi 	[1]FSR0
	movlw 	kFirstApplicationRow
	movwi 	[2]FSR0
	banksel _status 
	movf 	BANKMASK(_status), w
	movwi 	[3]FSR0

	movlw 4
	call _bus_slave_returndata

	retlw 0x00
ENDFUNCTION _exec_status


;Verify the application checksum and return it as a little endian 16 bit integer 
BEGINFUNCTION _exec_verify
	banksel mib_buffer
	call _verify_application
	banksel mib_buffer
	movwf BANKMASK(mib_buffer+0)
	clrf BANKMASK(mib_buffer+1)

	movlw 2
	call _bus_slave_returndata
	retlw 0x00
ENDFUNCTION _exec_verify
#include "bus_slave.h"
#include <string.h>

//static prototypes that are only to be used in this file
static void bus_slave_startcommand();
static void bus_slave_searchcommand();
static void bus_slave_callcommand();

/*
 * MIB Slave Logic 
 */

/*
 * Set the return status, the high order 3 bits define the status, the low order 5 bits set the 
 * length of the return value
 */
void bus_slave_setreturn(uint8 status)
{
	mib_state.bus_returnstatus.return_status = status;
}

inline void bus_slave_set_returnbuffer_length( uint8 length ) {
	bus_slave_setreturn( pack_return_status( kNoMIBError, length ) );	
}

void bus_slave_return_buffer( const void* buff, uint8 length ) {
	if ( length > kBusMaxMessageSize ) {
		bus_slave_seterror( kUnknownError ); //TODO: Better
		return;
	}
	memcpy( plist_get_buffer(0), buff, length );
	bus_slave_set_returnbuffer_length( length );
}

void bus_slave_return_int16( int16 val ) {
	plist_set_int16( 0, val );
	bus_slave_set_returnbuffer_length( kIntSize );
}

static void bus_slave_startcommand()
{
	//Initialize all the state
	set_slave_state(kMIBSearchCommand);
	mib_state.slave_handler = kInvalidMIBIndex;
	mib_state.num_reads = 0;

	bus_slave_setreturn(kUnknownError); //Make sure that if nothing else happens we return an error status.

	bus_slave_receive((unsigned char *)&mib_unified.bus_command, sizeof(MIBCommandPacket) + 24, 0);
}

static void bus_slave_searchcommand()
{
	if (i2c_slave_lasterror() != kI2CNoError)
	{
		bus_slave_seterror(kCommandChecksumError); //Make sure the parameter checksum was valid.
		return;
	}
	
	mib_state.slave_handler = find_handler();
	
	if (mib_state.slave_handler == kInvalidMIBIndex)
	{
		bus_slave_seterror(kUnsupportedCommand);
		return;
	}

	if (plist_param_length(mib_unified.bus_command.param_spec) > kBusMaxMessageSize)
	{
		bus_slave_seterror(kParameterTooLong);
		return;
	}

	set_slave_state(kMIBFinishCommand);
}

/*
 * @preconditions: mib_unified.mib_buffer is full of a packet of parameters or nothing
 * @return: 1 if the parameters are valid types and 0 otherwise
 * @side effects: sets mib slave error state appropriately
 */
static uint8 bus_slave_validateparams()
{
	if (i2c_slave_lasterror() != kI2CNoError)
	{
		bus_slave_seterror(kParameterChecksumError); //Make sure the parameter checksum was valid.
		return 0;
	}

	if (!validate_param_spec(mib_state.slave_handler))
	{
		bus_slave_seterror(kWrongParameterType); //Make sure the parameter checksum was valid.
		return 0;
	}

	return 1;
}

static void bus_slave_callcommand()
{	
	if (mib_state.slave_handler != kInvalidMIBIndex)
	{
		if (bus_slave_validateparams()) {
			bus_slave_setreturn( pack_return_status( kNoMIBError, 0 ) );
			call_handler(mib_state.slave_handler);
		}
	}
}

void bus_slave_reset()
{
	//A write after any number of reads is a protocol reset so we reset ourselves back to idle
	//This is a failsafe to make sure we can't get into a state where the slave locks up
	set_slave_state(kMIBIdleState);
	mib_state.num_reads = 0;
	i2c_slave_setidle();
}

void bus_slave_callback()
{
	if (i2c_address_received())
	{
		if (i2c_slave_is_read())
		{
			bus_inc_numreads();

			//Odd reads are for the return status
			//Even reads are for the return value
			if (bus_numreads_odd())
			{
				/*
				 * To allow for bus error recovery, we keep resending the return status and return value (if any) on all
				 * subsequent reads until the master is satisfied that it has received one with a valid checksum.
				 */

				bus_slave_searchcommand();
				bus_slave_callcommand();

				//slave callback should set the return status via bus_slave_setreturn and set mib_unified.mib_buffer to point to the return value if any
				bus_slave_send(&mib_state.bus_returnstatus.return_status, sizeof(MIBReturnValueHeader), kSendImmediately);

				//If we don't expect to send a return value, finish the command after this transmission
				if (mib_state.bus_returnstatus.len == 0)
				{
					set_slave_state(kMIBFinishCommand);
				}
			}
			else
			{
				//even reads are for the return value, which if there is one should be in the mib_unified.mib_buffer
				//if there wasn't one, then this is a protocol error (or a failed return status checksum) 
				//since there shouldn't have been a second read so we can send garbage since it will be 
				//ignored
				if (bus_has_returnvalue())
				{
					bus_slave_send((unsigned char *)mib_unified.mib_buffer, mib_state.bus_returnstatus.len, kSendImmediately);
				}
				else
				{
					bus_slave_send((unsigned char *)mib_unified.mib_buffer, 1, kSendImmediately); //protocol error so just send 1 byte, doesn't matter
				}
				
				set_slave_state(kMIBFinishCommand);

				//Make sure we can never overflow
				//Just toggle back and forth between 1, 2 and 3. Clear the slave handler though so
				//that we don't recall the function
			}

			if (bus_numreads_full())
			{
				mib_state.slave_handler = kInvalidMIBIndex;
				mib_state.num_reads = 1; 
			}
		}
		else
		{
			//We received a write
			bus_slave_startcommand(); //A write always indicates a new command
		}

		//Always release the clock.  The slave should never hold the clock forever.
		i2c_release_clock();
		return;
	}

	i2c_release_clock();
}
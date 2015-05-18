//mib_command.c 

#include "bus.h"
#include "bus_master.h"
#include "bus_slave.h"
#include "mib_feature.h"
#include <string.h>
#include "ioport.h"
#include <xc.h>

extern const feature_map** the_features;
extern unsigned int the_feature_count;

uint8 find_handler(void)
{
	uint8_t i, j, num_cmds;
	uint8_t feature = mib_unified.packet.call.command & 0xFF;
	uint8_t command = mib_unified.packet.call.command >> 8;

	for (i=0; i<the_feature_count; ++i)
	{
		if (the_features[i]->id == feature)
		{
			break;
		}
	}

	if (i == the_feature_count)
		return kInvalidMIBIndex;

	mib_state.feature_index = i;

	num_cmds = the_features[i]->command_count;

	for (j=0; j<num_cmds; ++j) 
	{
		if ( the_features[i]->commands[j].id == command)
			return j;
	}

	return kInvalidMIBIndex;
}

uint8_t  call_handler(uint8_t handler_index)
{
	return the_features[mib_state.feature_index]->commands[handler_index].handler(mib_unified.packet.call.length);
}

void bus_init(uint8 address)
{
	I2CConfig config;

	config.address = address;
	config.priority = 0b010;
	config.callback = bus_master_callback;
	config.slave_callback = bus_slave_callback;

	i2c_init_flags(&config);
	i2c_set_flag(&config, kEnableGeneralCallFlag);
	i2c_set_flag(&config, kEnableSoftwareClockStretchingFlag);

	mib_state.first_read = 1;
	mib_state.master_state = kMIBIdleState;

	i2c_configure(&config);
	i2c_enable();

	mib_state.my_address = address;

	bus_master_init();
}

void bus_slave_seterror(uint8 error)
{
	bus_slave_setreturn(error);
}

#define kNumCheckCycles ((kClockspeed / 1000000)*16*10)

uint8 bus_is_idle()
{
	unsigned int i = kNumCheckCycles;

	//Make sure we are not curently using the I2C hardware.
	if (I2C1CON & 0b11111)
		return 0;

	//Poll the bus for a fixed amount of time to make sure that there is no activity
	while (i-- > 0)
	{
		if (PIN(SDA) == 0 || PIN(SCL) == 0)
			return 0;
	}

	return 1;
}
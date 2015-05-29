#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "serial_commands.h"
#include "rtcc.h"
#include "utilities.h"
#include "oscillator.h"
#include "adc.h"
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "reset_manager.h"
#include "pic24asm.h"
#include "base64.h"
#include "debug.h"
#include "bus_master.h"
#include "eeprom.h"

extern volatile unsigned int adc_buffer[kADCBufferSize];
ScheduledTask test_task;

CommandStatus handle_echo_params(command_params *params)
{
  unsigned int i;

  if (params->num_params == 0) {
    print( "No parameters were passed.\r\n");
    return kFailure;
  }
  else
  {
    for (i=0; i<params->num_params; ++i)
    {
      print( get_param_string(params, i));
      print( "\r\n");
    }
  }
  return kSuccess;
}

CommandStatus handle_device(command_params *params)
{
    char *cmd;

    if (params->num_params < 1)
    {
        print( "You must pass a subcommand to the device command.\r\n");
        return kFailure;
    }

    cmd = get_param_string(params, 0);

    if (strcmp(cmd, "reset") == 0)
    {
        print( "Resetting the device...\r\n");
        set_command_result( kSuccess );
        asm_reset();
        return kNone;
    }
    else if (strcmp(cmd, "rtype") == 0)
    {
        sendf(DEBUG_UART, "Last reset type: %d\r\n", last_reset_type());
    }
    else if (strcmp(cmd, "sleep") == 0)
    {
        print("Sleeping\r\n");
        asm_sleep();
    }
    return kSuccess;
}

CommandStatus handle_rtcc(command_params *params)
{
    char *cmd;

    if (params->num_params < 1)
    {
        print( "You must pass a subcommand to the rtcc command.\r\n");
        return kFailure;
    }

    cmd = get_param_string(params, 0);

    //Farm out the subcommand to our subhandlers

    if (strcmp(cmd, "status") == 0)
    {
        unsigned int enabled = rtcc_enabled();


        sendf(DEBUG_UART, "Realtime Clock Status: %s\r\n", enabled? "Enabled" : "Disabled");
        rtcc_datetime time;

        rtcc_get_time(&time);

        sendf(DEBUG_UART, "Current Time: %d/%d/%d %d:%d:%d\r\n", time.month, time.day, time.year, time.hours, time.minutes, time.seconds);
    }
    else if (strcmp(cmd, "set") == 0)
    {
        rtcc_datetime time_spec;
        char *date, *time;
        char temp[3];
        int m;

        temp[2] = '\0';

        if (params->num_params < 3)
        {
            print( "usage: rtcc set mm/dd/yy hh:mm:ss\r\n");
            return kFailure;
        }

        date = get_param_string(params, 1);
        time = get_param_string(params, 2);

        sendf(DEBUG_UART, "Date string: %s\r\nTime string: %s\r\n", date, time);

        time_spec.month = get_2byte_number(date);
        time_spec.day = get_2byte_number(date+3);
        time_spec.year = get_2byte_number(date+6);
        time_spec.hours = get_2byte_number(time);
        time_spec.minutes = get_2byte_number(time+3);
        time_spec.seconds = get_2byte_number(time+6);

        m = time_spec.month;

        sendf(DEBUG_UART, "Input month was: %d\r\n", m);

        rtcc_set_time(&time_spec);
    }
    else if (strcmp(cmd, "enable") == 0)
    {
        enable_rtcc();
        print("RTCC Enabled\r\n");
    }
    else
    {
        sendf(DEBUG_UART, "Unknown rtcc command: %s\r\n", cmd);
        return kFailure;
    }
    return kSuccess;
}

static void rpc_callback(unsigned char status) 
{
    int i;
    uint8_t len=0;
    put(DEBUG_UART, status);

    if (status_is_error(status))
    {
        set_command_result( false );
        return;
    }

    if (packet_has_data(status))
        len = mib_unified.packet.response.length;

    //Command was successful
    put(DEBUG_UART, len);

    for (i=0; i<len; ++i)
        put(DEBUG_UART, plist_get_buffer(0)[i]);

    set_command_result( true );
}

CommandStatus handle_binrpc(command_params *params)
{
    unsigned char buffer[24];
    int i,length;
    char* str;
    MIBUnified data;

    if (params->num_params != 1) {
        put(DEBUG_UART, 254);
        print( "You must pass a single base64 encoded buffer with all parameters to binrpc.\n");
        return kFailure;
    }

    str = get_param_string(params, 0);

    if (strlen(str) != 32)
    {
        put(DEBUG_UART, 254);
        print( "You must pass a base64 encoded buffer with length 32.\n");
        return kFailure;
    }

    length = base64_decode(str, strlen(str), buffer, 24);

    if (length != 24)
    {
        put(DEBUG_UART, 254);
        print( "Could not decode base64 buffer\n");
        return kFailure;
    }

    if (!momo_attached())
    {
        put(DEBUG_UART, 254);
        print( "No MoMo unit attached to FSU, cannot send RPC.\n");
        return kFailure;
    }

    data.address = buffer[0];
    data.packet.call.command = (((uint16_t)buffer[1]) << 8) | buffer[2];
    data.packet.call.length = buffer[3];

    for(i=0; i<kMIBBufferSize; ++i)
        data.packet.data[i] = buffer[i+4];

    bus_master_rpc_async(rpc_callback, &data);
    return kPending;
}

CommandStatus handle_attached(command_params *params)
{
    if (momo_attached())
        print("1\n");
    else
        print("0\n");

    return kSuccess;
}

CommandStatus handle_alarm(command_params *params)
{
    char *cmd;

    if (params->num_params != 1) 
    {
        print( "Usage: alarm [yes|no|status]\n");
        return kFailure;
    }

    cmd = get_param_string(params, 0);

    if (strcmp(cmd, "status") == 0)
    {
        if (ALARM_PIN == 0)
            print("0\n");
        else
            print("1\n");

        return kSuccess;
    }
    else if (strcmp(cmd, "yes") == 0)
    {
        ALARM_PIN = 0;
        ALARM_TRIS = 0;
        eeprom_write(0, 0xAA);
        return kSuccess;
    }
    else if (strcmp(cmd, "no") == 0)
    {
        ALARM_TRIS = 1;
        ALARM_PIN = 0;
        eeprom_write(0, 0xFF);
        return kSuccess;
    }

    return kFailure;
}

CommandStatus handle_i2cstatus(command_params *params)
{
    put(DEBUG_UART, I2C1STAT&0xFF);
    put(DEBUG_UART, I2C1STAT>>8);
    return kSuccess;
}
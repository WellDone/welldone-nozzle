#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "serial_commands.h"
#include "rtcc.h"
#include "utilities.h"
#include "sensor.h"
#include "memory.h"
#include "oscillator.h"
#include "gsm.h"

void handle_echo_params(command_params *params)
{
  unsigned int i;

  if (params->num_params == 0)
    print( "No parameters were passed.\r\n");
  else
  {
    for (i=0; i<params->num_params; ++i)
    {
      print( get_param_string(params, i));
      print( "\r\n");
    }
  }
}

void handle_gsm(command_params *params)
{
    char *cmd;

    if (params->num_params < 1)
    {
        print( "You must pass a subcommand to the device command.\r\n");
        return;
    }

    cmd = get_param_string(params, 0);

    if (strcmp(cmd, "module") == 0)
    {
        if (params->num_params < 2)
        {
            print( "usage: gsm module [on|off]\r\n");
            return;
        }

        if (strcmp(get_param_string(params, 1),"on") == 0)
        {
            gsm_on();
            print( "GSM module turned on.\r\n");
        }
        else if (strcmp(get_param_string(params, 1),"off") == 0)
        {
            gsm_off();
            print( "GSM module turned off.\r\n");
        }
        else
            print( "Invalid option to gsm module command.\r\n");
    }
    else if (strcmp(cmd, "cmd") == 0)
    {
        if (params->num_params >= 2)
        {
            gsm_send_at_cmd(get_param_string(params,1));
            print( "command sent\r\n");
        }
        else
            print( "usage: gsm cmd <cmd>\r\n");
    }
    else if (strcmp(cmd, "msg") == 0)
    {
        if (params->num_params == 3)
        {
            gsm_send_sms( get_param_string(params, 1), get_param_string(params, 2) ); //TODO: Allow multi-word messages.  "quoted string" support?
        }
        else
            print( "usage: gsm msg <address> <message>\r\n");
    }
    else if (strcmp(cmd, "dump") == 0)
    {
        dump_gsm_buffer();
    }
    else if (strcmp(cmd, "report") == 0)
    {
        if (params->num_params >= 1)
        {
            gsm_send_sms("+15107358486", get_param_string(params,1));
        }
        else
            print( "usage: gsm report <status>\r\n");
    }
}

void handle_device(command_params *params)
{
    char *cmd;

    if (params->num_params < 1)
    {
        print( "You must pass a subcommand to the device command.\r\n");
        return;
    }

    cmd = get_param_string(params, 0);

    if (strcmp(cmd, "reset") == 0)
    {
        print( "Resetting the device...\r\n");
        asm_reset();
    }
    if (strcmp(cmd, "get") == 0)
    {
        if (params->num_params < 2)
        {
            print( "You must pass a subcommand to the device get command.\r\n");
            return;
        }

        cmd = get_param_string(params, 1);

        if (strcmp(cmd, "sosc") == 0)
        {
            int status = get_sosc_status();

            if (status == 1)
                print( "Secondary oscillator is enabled\r\n");
            else
                print( "Secondary oscillator is disabled\r\n");
        }
    }

    if (strcmp(cmd, "enable") == 0)
    {
        if (params->num_params < 2)
        {
            print( "You must pass a subcommand to the device enable command.\r\n");
            return;
        }

        cmd = get_param_string(params, 1);

        if (strcmp(cmd, "sosc") == 0)
        {
            set_sosc_status(1);
            print( "Secondary oscillator enabled.\r\n");
        }
    }
}

void handle_rtcc(command_params *params)
{
    char *cmd;

    if (params->num_params < 1)
    {
        print( "You must pass a subcommand to the rtcc command.\r\n");
        return;
    }

    cmd = get_param_string(params, 0);

    //Farm out the subcommand to our subhandlers

    if (strcmp(cmd, "status") == 0)
    {
        unsigned int enabled = rtcc_enabled();


        sendf(U2, "Realtime Clock Status: %s\r\n", enabled? "Enabled" : "Disabled");
        rtcc_time time;

        rtcc_get_time(&time);

        sendf(U2, "Current Time: %d/%d/%d %d:%d:%d\r\n", time.month, time.day, time.year, time.hours, time.minutes, time.seconds);
    }

    else if (strcmp(cmd, "set") == 0)
    {
        rtcc_time time_spec;
        char *date, *time;
        char temp[3];
        int m;

        temp[2] = '\0';

        if (params->num_params < 3)
        {
            print( "usage: rtcc set mm/dd/yy hh:mm:ss\r\n");
            return;
        }

        date = get_param_string(params, 1);
        time = get_param_string(params, 2);

        sendf(U2, "Date string: %s\r\nTime string: %s\r\n", date, time);

        time_spec.month = get_2byte_number(date);
        time_spec.day = get_2byte_number(date+3);
        time_spec.year = get_2byte_number(date+6);
        time_spec.hours = get_2byte_number(time);
        time_spec.minutes = get_2byte_number(time+3);
        time_spec.seconds = get_2byte_number(time+6);

        m = time_spec.month;

        sendf(U2, "Input month was: %d\r\n", m);

        rtcc_set_time(&time_spec);
    }
    else
        sendf(U2, "Unknown rtcc command: %s\r\n", cmd);
}

void handle_sensor(command_params *params) {
  IEC1bits.INT2IE = 1; //enable interrupt
  sends(U2, "Good night, Sweet Prince");
  Sleep();
  sends(U2, "I can't do that Dave");
  IEC1bits.INT2IE = 0; //disable interrupt
}

static BYTE memory_buffer[256];
void handle_memory(command_params *params) {
    char* cmd;
    char* data;
    int l = 0;
    cmd = get_param_string(params, 0);
    if (strcmp(cmd, "write") == 0) {
      data = get_param_string(params,1);
      mem_write(0x0A, data, strlen(data));
    } else if (strcmp(cmd, "read") == 0) {
      atoi_small( get_param_string(params, 1), &l);
      l &= 255;
      sendf(U2, "%d bytes\r\n", l);
      mem_read(0x0A, memory_buffer, l);
      memory_buffer[l+1] = 0x0;

      print("MEMORY READ\r\n");
      print(memory_buffer);
      print("\r\n");
    } else if (strcmp(cmd, "readall") == 0) {
        mem_read_all( 0x0A );
    } else if (strcmp(cmd, "clear") == 0) {
        mem_clear();
    }
}

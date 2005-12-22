//
// ZoneMinder Control Utility, $Date$, $Revision$
// Copyright (C) 2003, 2004, 2005  Philip Coombes
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 

#include <getopt.h>

#include "zm.h"
#include "zm_db.h"
#include "zm_user.h"
#include "zm_monitor.h"
#include "zm_local_camera.h"

void Usage( int status=-1 )
{
	fprintf( stderr, "zmu <-d device_path> [-v] [function] [-U<username> -P<password>]\n" );
	fprintf( stderr, "zmu <-m monitor_id> [-v] [function] [-U<username> -P<password>]\n" );
	fprintf( stderr, "General options:\n" );
	fprintf( stderr, "  -h, --help                     : This screen\n" );
	fprintf( stderr, "  -v, --verbose                  : Produce more verbose output\n" );
	fprintf( stderr, "  -l, --list                     : List the current status of active (or all with -v) monitors\n" );
	fprintf( stderr, "Options for use with devices:\n" );
	fprintf( stderr, "  -d, --device <device_path>     : Get the current video device settings for <device_path>\n" );
	fprintf( stderr, "  -q, --query                    : Query the current settings for the device\n" );
	fprintf( stderr, "Options for use with monitors:\n" );
	fprintf( stderr, "  -m, --monitor <monitor_id>     : Specify which monitor to address, default 1 if absent\n" );
	fprintf( stderr, "  -q, --query                    : Query the current settings for the monitor\n" );
	fprintf( stderr, "  -s, --state                    : Output the current monitor state, 0 = idle, 1 = prealarm, 2 = alarm,\n" );
	fprintf( stderr, "                                   3 = alert, 4 = tape\n" );
	fprintf( stderr, "  -B, --brightness [value]       : Output the current brightness, set to value if given \n" );
	fprintf( stderr, "  -C, --contrast [value]         : Output the current contrast, set to value if given \n" );
	fprintf( stderr, "  -H, --hue [value]              : Output the current hue, set to value if given \n" );
	fprintf( stderr, "  -O, --colour [value]           : Output the current colour, set to value if given \n" );
	fprintf( stderr, "  -i, --image [image_index]      : Write captured image to disk as <monitor_name>.jpg, last image captured\n" );
	fprintf( stderr, "                                   or specified ring buffer index if given.\n" );
	fprintf( stderr, "  -S, --scale <scale_%%ge>        : With --image specify any scaling (in %%) to be applied to the image\n" );
	fprintf( stderr, "  -t, --timestamp [image_index]  : Output captured image timestamp, last image captured or specified\n" );
	fprintf( stderr, "                                   ring buffer index if given\n" );
	fprintf( stderr, "  -R, --read_index               : Output ring buffer read index\n" );
	fprintf( stderr, "  -W, --write_index              : Output ring buffer write index\n" );
	fprintf( stderr, "  -e, --event                    : Output last event index\n" );
	fprintf( stderr, "  -f, --fps                      : Output last Frames Per Second captured reading\n" );
	fprintf( stderr, "  -z, --zones                    : Write last captured image overlaid with zones to <monitor_name>-Zones.jpg\n" );
	fprintf( stderr, "  -a, --alarm                    : Force alarm in monitor, this will trigger recording until cancelled with -c\n" );
	fprintf( stderr, "  -n, --noalarm                  : Force no alarms in monitor, this will prevent alarms until cancelled with -c\n" );
	fprintf( stderr, "  -c, --cancel                   : Cancel a forced alarm/noalarm in monitor, required after being enabled with -a or -n\n" );
	fprintf( stderr, "  -L, --reload                   : Signal monitor to reload settings\n" );
	fprintf( stderr, "  -E, --enable                   : Enable detection, wake monitor up\n" );
	fprintf( stderr, "  -D, --disable                  : Disble detection, put monitor to sleep\n" );
	fprintf( stderr, "  -u, --suspend                  : Suspend detection, useful to prevent bogus alarms when panning etc\n" );
	fprintf( stderr, "  -r, --resume                   : Resume detection after a suspend\n" );
	fprintf( stderr, "  -U, --username <username>      : When running in authenticated mode the username and\n" );
	fprintf( stderr, "  -P, --password <password>      : password combination of the given user\n" );
	fprintf( stderr, "  -A, --auth <authentication>    : Pass authentication hash string instead of user details\n" );

	exit( status );
}

typedef enum {
	BOGUS      = 0x00000000,
	STATE      = 0x00000001,
	IMAGE      = 0x00000002,
	TIME       = 0x00000004,
	READ_IDX   = 0x00000008,
	WRITE_IDX  = 0x00000010,
	EVENT      = 0x00000020,
	FPS        = 0x00000040,
	ZONES      = 0x00000080,
	ALARM      = 0x00000100,
	NOALARM    = 0x00000200,
	CANCEL     = 0x00000400,
	QUERY      = 0x00000800,
	BRIGHTNESS = 0x00001000,
	CONTRAST   = 0x00002000,
	HUE        = 0x00004000,
	COLOUR     = 0x00008000,
	RELOAD     = 0x00010000,
	ENABLE     = 0x00100000,
	DISABLE    = 0x00200000,
	SUSPEND    = 0x00400000,
	RESUME     = 0x00800000,
	LIST       = 0x10000000,
} Function;

bool ValidateAccess( User *user, int mon_id, int function )
{
	bool allowed = true;
	if ( function & (STATE|IMAGE|TIME|READ_IDX|WRITE_IDX|FPS) )
	{
		if ( user->getStream() < User::PERM_VIEW )
			allowed = false;
	}
	if ( function & EVENT )
	{
		if ( user->getEvents() < User::PERM_VIEW )
			allowed = false;
	}
	if ( function & (ZONES|QUERY|LIST) )
	{
		if ( user->getMonitors() < User::PERM_VIEW )
			allowed = false;
	}
	if ( function & (ALARM|NOALARM|CANCEL|RELOAD|ENABLE|DISABLE|SUSPEND|RESUME|BRIGHTNESS|CONTRAST|HUE|COLOUR) )
	{
		if ( user->getMonitors() < User::PERM_EDIT )
			allowed = false;
	}
	if ( mon_id > 0 )
	{
		if ( !user->canAccess( mon_id ) )
		{
			allowed = false;
		}
	}
	if ( !allowed )
	{
		fprintf( stderr, "Error, insufficient privileges for requested action\n" );
		exit( -1 );
	}
	return( allowed );
}

int main( int argc, char *argv[] )
{
	static struct option long_options[] = {
		{"device", 1, 0, 'd'},
		{"monitor", 1, 0, 'm'},
		{"verbose", 0, 0, 'v'},
		{"image", 2, 0, 'i'},
		{"scale", 1, 0, 'S'},
		{"timestamp", 2, 0, 't'},
		{"state", 0, 0, 's'},
		{"brightness", 2, 0, 'B'},
		{"contrast", 2, 0, 'C'},
		{"hue", 2, 0, 'H'},
		{"contrast", 2, 0, 'O'},
		{"read_index", 0, 0, 'R'},
		{"write_index", 0, 0, 'W'},
		{"event", 0, 0, 'e'},
		{"fps", 0, 0, 'f'},
		{"zones", 2, 0, 'z'},
		{"alarm", 0, 0, 'a'},
		{"noalarm", 0, 0, 'n'},
		{"cancel", 0, 0, 'c'},
		{"reload", 0, 0, 'L'},
		{"enable", 0, 0, 'E'},
		{"disable", 0, 0, 'D'},
		{"suspend", 0, 0, 'u'},
		{"resume", 0, 0, 'r'},
		{"query", 0, 0, 'q'},
		{"username", 1, 0, 'U'},
		{"password", 1, 0, 'P'},
		{"help", 0, 0, 'h'},
		{"list", 0, 0, 'l'},
		{0, 0, 0, 0}
	};

	const char *device = "";
	int mon_id = 0;
	bool verbose = false;
	int function = BOGUS;

	int image_idx = -1;
	int scale = -1;
	int brightness = -1;
	int contrast = -1;
	int hue = -1;
	int colour = -1;
	char *zone_string = 0;
	char *username = 0;
	char *password = 0;
	char *auth = 0;
	while (1)
	{
		int option_index = 0;

		int c = getopt_long (argc, argv, "d:m:vsEDurwei::S:t::fz::ancqhlB::C::H::O::U:P:A:", long_options, &option_index);
		if (c == -1)
		{
			break;
		}

		switch (c)
		{
			case 'd':
				device = optarg;
				break;
			case 'm':
				mon_id = atoi(optarg);
				break;
			case 'v':
				verbose = true;
				break;
			case 's':
				function |= STATE;
				break;
			case 'i':
				function |= IMAGE;
				if ( optarg )
				{
					image_idx = atoi( optarg );
				}
				break;
			case 'S':
				scale = atoi(optarg);
				break;
			case 't':
				function |= TIME;
				if ( optarg )
				{
					image_idx = atoi( optarg );
				}
				break;
			case 'R':
				function |= READ_IDX;
				break;
			case 'W':
				function |= WRITE_IDX;
				break;
			case 'e':
				function |= EVENT;
				break;
			case 'f':
				function |= FPS;
				break;
			case 'z':
				function |= ZONES;
				if ( optarg )
				{
					zone_string = optarg;
				}
				break;
			case 'a':
				function |= ALARM;
				break;
			case 'n':
				function |= NOALARM;
				break;
			case 'c':
				function |= CANCEL;
				break;
			case 'L':
				function |= RELOAD;
				break;
			case 'E':
				function |= ENABLE;
				break;
			case 'D':
				function |= DISABLE;
				break;
			case 'u':
				function |= SUSPEND;
				break;
			case 'r':
				function |= RESUME;
				break;
			case 'q':
				function |= QUERY;
				break;
			case 'B':
				function |= BRIGHTNESS;
				if ( optarg )
				{
					brightness = atoi( optarg );
				}
				break;
			case 'C':
				function |= CONTRAST;
				if ( optarg )
				{
					contrast = atoi( optarg );
				}
				break;
			case 'H':
				function |= HUE;
				if ( optarg )
				{
					hue = atoi( optarg );
				}
				break;
			case 'O':
				function |= COLOUR;
				if ( optarg )
				{
					colour = atoi( optarg );
				}
				break;
			case 'U':
				username = optarg;
				break;
			case 'P':
				password = optarg;
				break;
			case 'A':
				auth = optarg;
				break;
			case 'h':
				Usage( 0 );
				break;
			case 'l':
				function |= LIST;
				break;
			case '?':
				Usage();
				break;
			default:
				//fprintf( stderr, "?? getopt returned character code 0%o ??\n", c );
				break;
		}
	}

	if (optind < argc)
	{
		fprintf( stderr, "Extraneous options, " );
		while (optind < argc)
			fprintf( stderr, "%s ", argv[optind++]);
		fprintf( stderr, "\n");
		Usage();
	}

	if ( device[0] && !(function&QUERY) )
	{
		fprintf( stderr, "Error, -d option cannot be used with this option\n" );
		Usage();
	}
	if ( scale != -1 && !(function&IMAGE) )
	{
		fprintf( stderr, "Error, -S option cannot be used with this option\n" );
		Usage();
	}
	//printf( "Monitor %d, Function %d\n", mon_id, function );

	zmDbgInit( "zmu", "", -1 );

	zmLoadConfig();

	User *user = 0;

	if ( config.opt_use_auth )
	{
		if ( !(username && password) && !auth )
		{
			fprintf( stderr, "Error, username and password or auth string must be supplied\n" );
			exit( -1 );
		}

		//if ( strcmp( config.auth_relay, "hashed" ) == 0 )
		{
			if ( auth )
			{
				user = zmLoadAuthUser( auth, false );
			}
		}
		//else if ( strcmp( config.auth_relay, "plain" ) == 0 )
		{
			if ( username && password )
			{
				user = zmLoadUser( username, password );
			}
		}
		if ( !user )
		{
			fprintf( stderr, "Error, unable to authenticate user\n" );
			exit( -1 );
		}
		ValidateAccess( user, mon_id, function );
	}
	

	if ( device[0] )
	{
		if ( function & QUERY )
		{
			char vid_string[BUFSIZ] = "";
			bool ok = LocalCamera::GetCurrentSettings( device, vid_string, verbose );
			printf( "%s", vid_string );
			exit( ok?0:-1 );
		}
	}
	else if ( mon_id > 0 )
	{
		Monitor *monitor = Monitor::Load( mon_id, function&(QUERY|ZONES) );
		if ( monitor )
		{
			if ( verbose )
			{
				printf( "Monitor %d(%s)\n", monitor->Id(), monitor->Name() );
			}
			char separator = ' ';
			bool have_output = false;
			if ( function & STATE )
			{
				Monitor::State state = monitor->GetState();
				if ( verbose )
					printf( "Current state: %s\n", state==Monitor::ALARM?"Alarm":(state==Monitor::ALERT?"Alert":"Idle") );
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%d", state );
					have_output = true;
				}
			}
			if ( function & TIME )
			{
				struct timeval timestamp = monitor->GetTimestamp( image_idx );
				if ( verbose )
				{
					char timestamp_str[64] = "None";
					if ( timestamp.tv_sec )
						strftime( timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", localtime( &timestamp.tv_sec ) );
					if ( image_idx == -1 )
						printf( "Time of last image capture: %s.%02ld\n", timestamp_str, timestamp.tv_usec/10000 );
					else
						printf( "Time of image %d capture: %s.%02ld\n", image_idx, timestamp_str, timestamp.tv_usec/10000 );
				}
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%ld.%02ld", timestamp.tv_sec, timestamp.tv_usec/10000 );
					have_output = true;
				}
			}
			if ( function & READ_IDX )
			{
				if ( verbose )
					printf( "Last read index: %d\n", monitor->GetLastReadIndex() );
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%d", monitor->GetLastReadIndex() );
					have_output = true;
				}
			}
			if ( function & WRITE_IDX )
			{
				if ( verbose )
					printf( "Last write index: %d\n", monitor->GetLastWriteIndex() );
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%d", monitor->GetLastWriteIndex() );
					have_output = true;
				}
			}
			if ( function & EVENT )
			{
				if ( verbose )
					printf( "Last event id: %d\n", monitor->GetLastEvent() );
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%d", monitor->GetLastEvent() );
					have_output = true;
				}
			}
			if ( function & FPS )
			{
				if ( verbose )
					printf( "Current capture rate: %.2f frames per second\n", monitor->GetFPS() );
				else
				{
					if ( have_output ) printf( "%c", separator );
					printf( "%.2f", monitor->GetFPS() );
					have_output = true;
				}
			}
			if ( function & IMAGE )
			{
				if ( verbose )
				{
					if ( image_idx == -1 )
						printf( "Dumping last image captured to %s.jpg", monitor->Name() );
					else
						printf( "Dumping buffer image %d to %s.jpg", image_idx, monitor->Name() );
					if ( scale != -1 )
						printf( ", scaling by %d%%", scale );
					printf( "\n" );
				}
				monitor->GetImage( image_idx, scale>0?scale:100 );
			}
			if ( function & ZONES )
			{
				if ( verbose )
					printf( "Dumping zone image to %s-Zones.jpg\n", monitor->Name() );
				monitor->DumpZoneImage( zone_string );
			}
			if ( function & ALARM )
			{
				if ( verbose )
					printf( "Forcing alarm on\n" );
				monitor->ForceAlarmOn( config.forced_alarm_score, "Forced Web" );
			}
			if ( function & NOALARM )
			{
				if ( verbose )
					printf( "Forcing alarm off\n" );
				monitor->ForceAlarmOff();
			}
			if ( function & CANCEL )
			{
				if ( verbose )
					printf( "Cancelling forced alarm on/off\n" );
				monitor->CancelForced();
			}
			if ( function & RELOAD )
			{
				if ( verbose )
					printf( "Reloading monitor settings\n" );
				monitor->actionReload();
			}
			if ( function & ENABLE )
			{
				if ( verbose )
					printf( "Enabling event generation\n" );
				monitor->actionEnable();
			}
			if ( function & DISABLE )
			{
				if ( verbose )
					printf( "Disabling event generation\n" );
				monitor->actionDisable();
			}
			if ( function & SUSPEND )
			{
				if ( verbose )
					printf( "Suspending event generation\n" );
				monitor->actionSuspend();
			}
			if ( function & RESUME )
			{
				if ( verbose )
					printf( "Resuming event generation\n" );
				monitor->actionResume();
			}
			if ( function & QUERY )
			{
				char mon_string[1024] = "";
				monitor->DumpSettings( mon_string, verbose );
				printf( "%s\n", mon_string );
			}
			if ( function & BRIGHTNESS )
			{
				if ( verbose )
				{
					if ( brightness >= 0 )
						printf( "New brightness: %d\n", monitor->actionBrightness( brightness ) );
					else
						printf( "Current brightness: %d\n", monitor->actionBrightness() );
				}
				else
				{
					if ( have_output ) printf( "%c", separator );
					if ( brightness >= 0 )
						printf( "%d", monitor->actionBrightness( brightness ) );
					else
						printf( "%d", monitor->actionBrightness() );
					have_output = true;
				}
			}
			if ( function & CONTRAST )
			{
				if ( verbose )
				{
					if ( contrast >= 0 )
						printf( "New brightness: %d\n", monitor->actionContrast( contrast ) );
					else
						printf( "Current contrast: %d\n", monitor->actionContrast() );
				}
				else
				{
					if ( have_output ) printf( "%c", separator );
					if ( contrast >= 0 )
						printf( "%d", monitor->actionContrast( contrast ) );
					else
						printf( "%d", monitor->actionContrast() );
					have_output = true;
				}
			}
			if ( function & HUE )
			{
				if ( verbose )
				{
					if ( hue >= 0 )
						printf( "New hue: %d\n", monitor->actionHue( hue ) );
					else
						printf( "Current hue: %d\n", monitor->actionHue() );
				}
				else
				{
					if ( have_output ) printf( "%c", separator );
					if ( hue >= 0 )
						printf( "%d", monitor->actionHue( hue ) );
					else
						printf( "%d", monitor->actionHue() );
					have_output = true;
				}
			}
			if ( function & COLOUR )
			{
				if ( verbose )
				{
					if ( colour >= 0 )
						printf( "New colour: %d\n", monitor->actionColour( colour ) );
					else
						printf( "Current colour: %d\n", monitor->actionColour() );
				}
				else
				{
					if ( have_output ) printf( "%c", separator );
					if ( colour >= 0 )
						printf( "%d", monitor->actionColour( colour ) );
					else
						printf( "%d", monitor->actionColour() );
					have_output = true;
				}
			}
			if ( have_output )
			{
				printf( "\n" );
			}
			if ( !function )
			{
				Usage();
			}
			delete monitor;
		}
		else
		{
			fprintf( stderr, "Error, invalid monitor id %d\n", mon_id );
			exit( -1 );
		}
	}
	else
	{
		if ( function & LIST )
		{
			char sql[BUFSIZ];
			strncpy( sql, "select Id, Function+0 from Monitors", sizeof(sql) );
			if ( !verbose )
			{
				strncat( sql, " where Function != 'None'", sizeof(sql)-strlen(sql) );
			}
			strncat( sql, " order by Id asc", sizeof(sql)-strlen(sql) );

			if ( mysql_query( &dbconn, sql ) )
			{
				Error(( "Can't run query: %s", mysql_error( &dbconn ) ));
				exit( mysql_errno( &dbconn ) );
			}

			MYSQL_RES *result = mysql_store_result( &dbconn );
			if ( !result )
			{
				Error(( "Can't use query result: %s", mysql_error( &dbconn ) ));
				exit( mysql_errno( &dbconn ) );
			}
			int n_monitors = mysql_num_rows( result );
			Debug( 1, ( "Got %d monitors", n_monitors ));

			printf( "%4s%5s%6s%9s%14s%6s%6s%8s%8s\n", "Id", "Func", "State", "TrgState", "LastImgTim", "RdIdx", "WrIdx", "LastEvt", "FrmRate" );
			for( int i = 0; MYSQL_ROW dbrow = mysql_fetch_row( result ); i++ )
			{
				int mon_id = atoi(dbrow[0]);
				int function = atoi(dbrow[1]);
				if ( !user || user->canAccess( mon_id ) )
				{
					if ( function > 1 )
					{
						Monitor *monitor = Monitor::Load( mon_id );
						if ( monitor )
						{
							struct timeval tv = monitor->GetTimestamp();
							printf( "%4d%5d%6d%9d%11ld.%02ld%6d%6d%8d%8.2f\n",
								monitor->Id(),
								function,
								monitor->GetState(),
								monitor->GetTriggerState(),
								tv.tv_sec, tv.tv_usec/10000,
								monitor->GetLastReadIndex(),
								monitor->GetLastWriteIndex(),
								monitor->GetLastEvent(),
								monitor->GetFPS()
							);
							delete monitor;
						}
						delete monitor;
					}
					else
					{
						struct timeval tv = { 0, 0 };
						printf( "%4d%5d%6d%9d%11ld.%02ld%6d%6d%8d%8.2f\n",
							mon_id,
							function,
							0,
							0,
							tv.tv_sec, tv.tv_usec/10000,
							0,
							0,
							0,
							0.0
						);
					}
				}
			}
			mysql_free_result( result );
		}
	}
	delete user;

	return( 0 );
}

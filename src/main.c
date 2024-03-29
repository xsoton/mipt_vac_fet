#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <argp.h>
#include <error.h>
#include "gpib.h"

// === [DATE] ===
struct tm start_time_struct;

// === [ARGUMENTS] ===
const char *argp_program_version = "vacfet 0.1";
const char *argp_program_bug_address = "<killingrain@gmail.com>";
static char doc[] =
	"VACFET -- a program for measuring of charge carrier mobility of "
	"thin films in field effect transistor structure\v"
	"TODO: This part of the documentation comes *after* the options; "
	"note that the text is automatically filled, but it's possible "
	"to force a line-break, e.g.\n<-- here.";
static char args_doc[] = "SAMPLE_NAME";

// Keys for options without short-options
#define OPT_VG_START 1 // --Vg_start
#define OPT_VG_STOP  2 // --Vg_stop
#define OPT_VG_STEP  3 // --Vg_step
#define OPT_IG_MAX   4 // --Ig_max
#define OPT_VD       5 // --Vd
#define OPT_ID_MAX   6 // --Id_max
#define OPT_RF       7 // --Rf
#define OPT_TMS      8 // --Tms

// The options we understand
static struct argp_option options[] =
{
	{0,0,0,0, "GATE-SOURCE parameters:"},
	{"Vg_start", OPT_VG_START, "double", 0, "Start voltage, V   (0.0   - 60.0, default  0.0 )"},
	{"Vg_stop" , OPT_VG_STOP , "double", 0, "Stop voltage, V    (0.0   - 60.0, default 10.0 )"},
	{"Vg_step" , OPT_VG_STEP , "double", 0, "Voltage step, V    (0.001 -  1.0, default  0.1 )"},
	{"Ig_max"  , OPT_IG_MAX  , "double", 0, "Maximum current, A (0.001 -  0.1, default  0.01)"},
	{0,0,0,0, "DRAIN-SOURCE parameters:"},
	{"Vd"      , OPT_VD      , "double", 0, "Voltage, V         (0.0   - 60.0, default  0.5 )"},
	{"Id_max"  , OPT_ID_MAX  , "double", 0, "Maximum current, A (0.001 -  0.1, default  0.01)"},
	{0,0,0,0, "Required:"},
	{"Rf"      , OPT_RF      , "double", 0, "Feedback resistance of TIA, Ohm (1.0e3 - 1.0e6)"},
	{"Tms"     , OPT_TMS     , "double", 0, "Scanning delay time, ms (1.0e3 - 1.0e4)"},
	{0,0,0,0, "Common:"},
	{0}
};

// parse arguments
struct arguments
{
	int    sample_name_flag;
	char  *sample_name;
	double Vg_start;
	double Vg_stop;
	double Vg_step;
	double Ig_max;
	double Vd;
	double Id_max;
	int    Rf_flag;
	double Rf;
	int    Tms_flag;
	double Tms;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	struct arguments *a = state->input;
	double t;

	switch (key)
	{
		case OPT_VG_START:
			t = atof(arg);
			if ((t < 0.0) || (t > 60.0))
			{
				fprintf(stderr, "# E: <Vg_start> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Vg_start = t;
			break;
		case OPT_VG_STOP:
			t = atof(arg);
			if ((t < 0.0) || (t > 60.0))
			{
				fprintf(stderr, "# E: <Vg_stop> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Vg_stop = t;
			break;
		case OPT_VG_STEP:
			t = atof(arg);
			if ((t < 0.001) || (t > 1.0))
			{
				fprintf(stderr, "# E: <Vg_step> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Vg_step = t;
			break;
		case OPT_IG_MAX:
			t = atof(arg);
			if ((t < 0.001) || (t > 0.1))
			{
				fprintf(stderr, "# E: <Ig_max> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Ig_max = t;
			break;
		case OPT_VD:
			t = atof(arg);
			if ((t < 0.0) || (t > 60.0))
			{
				fprintf(stderr, "# E: <Vd> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Vd = t;
			break;
		case OPT_ID_MAX:
			t = atof(arg);
			if ((t < 0.001) || (t > 0.1))
			{
				fprintf(stderr, "# E: <Id_max> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Id_max = t;
			break;
		case OPT_RF:
			t = atof(arg);
			if ((t < 1.0e3) || (t > 1.0e6))
			{
				fprintf(stderr, "# E: <Rf> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Rf = t;
			a->Rf_flag = 1;
			break;
		case OPT_TMS:
			t = atof(arg);
			if ((t < 1.0e3) || (t > 1.0e4))
			{
				fprintf(stderr, "# E: <Tms> is out of range. See \"vacfet --help\"\n");
				return ARGP_ERR_UNKNOWN;
			}
			a->Tms = t;
			a->Tms_flag = 1;
			break;
		case ARGP_KEY_ARG:
			a->sample_name = arg;
			a->sample_name_flag = 1;
			break;
		case ARGP_KEY_NO_ARGS:
			fprintf(stderr, "# E: <sample_name> has not specified. See \"vacfet --help\"\n");
			a->sample_name_flag = 0;
			//argp_usage (state);
			return ARGP_ERR_UNKNOWN;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

// === [GPIB] ===
#define PPS_GPIB_NAME "AKIP-1142/3G"
#define VM_GPIB_NAME "AKIP-V7-78/1"

// === [SOURCE] ===
#define VG_START 0.0
#define VG_STOP  10.0
#define VG_STEP  0.1
#define IG_MAX   0.01
#define VG       0.5
#define ID_MAX   0.001

// === [VOLTMETER: RANGE CHECKER] ===
#define V_100mV 0
#define V_1V    1
#define V_10V   2
#define V_100V  3

static double V_hi[4] = {0.095, 0.95, 9.5, 95.0};
static double V_lo[4] = {0.085, 0.85, 8.5, 85.0};

static int V_range = V_100mV;

// returns new range
static int V_check_range(int range, double V)
{
	if ((range < 0) || (range > 3))
	{
		return -1;
	}

	if (range == 0)
	{
		if (fabs(V) <= V_hi[range])
		{
			return range;
		}
		else
		{
			range++;
		}
	}

	while((range >= 1) && (range <= 3))
	{
		if (fabs(V) < V_lo[range-1])
		{
			range--;
			continue;
		}

		if (fabs(V) > V_hi[range])
		{
			range++;
			continue;
		}

		break;
	}

	return range;
}

// === threads ====
static void *commander(void *);
static void *worker(void *);

// === utils ===
static int get_run();
static void set_run(int run_new);
static double get_time();

// === global variables
static char dir_str[200];
static pthread_rwlock_t run_lock;
static int run;
static char filename_vac[250];
struct arguments arg = {0};

// #define DEBUG

// === program entry point
int main(int argc, char const *argv[])
{
	int ret = 0;
	int status;

	time_t start_time;
	// struct tm start_time_struct;

	pthread_t t_commander;
	pthread_t t_worker;

	// === parse input parameters
	arg.sample_name_flag = 0;
	arg.sample_name = NULL;
	arg.Vg_start = VG_START;
	arg.Vg_stop = VG_STOP;
	arg.Vg_step = VG_STEP;
	arg.Ig_max = IG_MAX;
	arg.Vd = VG;
	arg.Id_max = ID_MAX;
	arg.Rf_flag = 0;
	arg.Rf = 0.0;
	arg.Tms_flag = 0;
	arg.Tms = 0.0;

	status = argp_parse(&argp, argc, argv, 0, 0, &arg);
	if ((status != 0) || (arg.sample_name_flag != 1) || (arg.Rf_flag != 1) || (arg.Tms_flag != 1))
	{
		fprintf(stderr, "# E: Error while parsing. See \"vacfet --help\"\n");
		ret = -1;
		goto main_exit;
	}

	#ifdef DEBUG
	fprintf(stderr, "sample_name_flag = %d\n" , arg.sample_name_flag);
	fprintf(stderr, "sample_name      = %s\n" , arg.sample_name);
	fprintf(stderr, "Vg_start         = %le\n", arg.Vg_start);
	fprintf(stderr, "Vg_stop          = %le\n", arg.Vg_stop);
	fprintf(stderr, "Vg_step          = %le\n", arg.Vg_step);
	fprintf(stderr, "Ig_max           = %le\n", arg.Ig_max);
	fprintf(stderr, "Vd               = %le\n", arg.Vd);
	fprintf(stderr, "Id_max           = %le\n", arg.Id_max);
	fprintf(stderr, "Rf_flag          = %d\n" , arg.Rf_flag);
	fprintf(stderr, "Rf               = %le\n", arg.Rf);
	fprintf(stderr, "Tms_flag         = %d\n" , arg.Tms_flag);
	fprintf(stderr, "Tms              = %le\n", arg.Tms);
	#endif

	// === get start time of experiment ===
	start_time = time(NULL);
	localtime_r(&start_time, &start_time_struct);

	// === we need actual information w/o buffering
	setlinebuf(stdout);
	setlinebuf(stderr);

	// === initialize run state variable
	pthread_rwlock_init(&run_lock, NULL);
	run = 1;

	// === create dirictory in "20191012_153504_<experiment_name>" format
	snprintf(dir_str, 200, "%04d-%02d-%02d_%02d-%02d-%02d_%s",
		start_time_struct.tm_year + 1900,
		start_time_struct.tm_mon + 1,
		start_time_struct.tm_mday,
		start_time_struct.tm_hour,
		start_time_struct.tm_min,
		start_time_struct.tm_sec,
		arg.sample_name
	);
	status = mkdir(dir_str, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (status == -1)
	{
		fprintf(stderr, "# E: unable to create experiment directory (%s)\n", strerror(errno));
		ret = -2;
		goto main_exit;
	}

	// === create file names
	snprintf(filename_vac, 250, "%s/vac.dat", dir_str);
	// printf("filename_vac \"%s\"\n", filename_vac);

	// === now start threads
	pthread_create(&t_commander, NULL, commander, NULL);
	pthread_create(&t_worker, NULL, worker, NULL);

	// === and wait ...
	pthread_join(t_worker, NULL);

	// === cancel commander thread becouse we don't need it anymore
	// === and wait for cancelation finish
	pthread_cancel(t_commander);
	pthread_join(t_commander, NULL);

	fprintf(stdout, "\r\n");

	main_exit:
	return ret;
}

// === commander function
static void *commander(void *a)
{
	(void) a;

	char str[100];
	char *s;
	int ccount;

	while(get_run())
	{
		fprintf(stdout, "> ");

		s = fgets(str, 100, stdin);
		if (s == NULL)
		{
			fprintf(stderr, "# E: Exit\n");
			set_run(0);
			break;
		}

		switch(str[0])
		{
			case 'h':
				printf(
					"Help:\n"
					"\th -- this help;\n"
					"\tq -- exit the program;\n");
				break;
			case 'q':
				set_run(0);
				break;
			default:
				ccount = strlen(str)-1;
				fprintf(stderr, "# E: Unknown command (%.*s)\n", ccount, str);
				break;
		}
	}

	return NULL;
}

// === worker function
static void *worker(void *a)
{
	(void) a;

	int r;

	int pps_fd;
	int vm_fd;

	int    vac_index;
	double vac_time;
	double pps_voltage1;
	double pps_voltage2;
	double pps_current1;
	double pps_current2;
	double vm_voltage;
	double vac_voltage;
	double vac_current;

	double voltage;

	FILE  *vac_fp;
	FILE  *gp;
	char   buf[300];

	int new_V_range;

	r = gpib_open(PPS_GPIB_NAME);
	if(r == -1)
	{
		fprintf(stderr, "# E: Unable to open power supply (%d)\n", r);
		goto worker_pps_ibfind;
	}
	pps_fd = r;

	r = gpib_open(VM_GPIB_NAME);
	if(r == -1)
	{
		fprintf(stderr, "# E: Unable to open voltmeter (%d)\n", r);
		goto worker_vm_ibfind;
	}
	vm_fd = r;

	// === init pps
	gpib_write(pps_fd, "output 0");
	gpib_write(pps_fd, "instrument:nselect 1");
	gpib_print(pps_fd, "voltage:limit %.1lfV", 60.1);
	gpib_print(pps_fd, "voltage %.3lf", arg.Vd);
	gpib_print(pps_fd, "current %.3lf", arg.Id_max);
	gpib_write(pps_fd, "instrument:nselect 2");
	gpib_print(pps_fd, "voltage:limit %.1lfV", 60.1);
	gpib_print(pps_fd, "voltage %.3lf", arg.Vg_start);
	gpib_print(pps_fd, "current %.3lf", arg.Ig_max);
	gpib_write(pps_fd, "instrument:nselect 1");
	gpib_write(pps_fd, "channel:output 1");
	gpib_write(pps_fd, "instrument:nselect 2");
	gpib_write(pps_fd, "channel:output 1");
	// gpib_print_error(pps_fd);

	// === init vm
	gpib_write(vm_fd, "function \"voltage:dc\"");
	gpib_write(vm_fd, "voltage:dc:range:auto off");
	gpib_write(vm_fd, "voltage:dc:range 0.1");
	gpib_write(vm_fd, "voltage:dc:nplcycles 10");
	gpib_write(vm_fd, "trigger:source immediate");
	gpib_write(vm_fd, "trigger:delay:auto off");
	gpib_write(vm_fd, "trigger:delay 0");
	gpib_write(vm_fd, "trigger:count 1");
	gpib_write(vm_fd, "sample:count 1");
	gpib_write(vm_fd, "sense:zero:auto on");
	// gpib_print_error(vm_fd);

	// === create vac file
	vac_fp = fopen(filename_vac, "w+");
	if(vac_fp == NULL)
	{
		fprintf(stderr, "# E: Unable to open file \"%s\" (%s)\n", filename_vac, strerror(ferror(vac_fp)));
		goto worker_vac_fopen;
	}
	setlinebuf(vac_fp);

	// === write vac header
	r = fprintf(vac_fp,
		"# Measuring of charge carrier mobility of thin films in field effect transistor structure\n"
		"# Id vs Vg\n"
		"# Date: %04d.%02d.%02d %02d:%02d:%02d\n"
		"# Start parameters:\n"
		"#   sample_name      = %s\n"
		"#   Vg_start         = %le\n"
		"#   Vg_stop          = %le\n"
		"#   Vg_step          = %le\n"
		"#   Ig_max           = %le\n"
		"#   Vd               = %le\n"
		"#   Id_max           = %le\n"
		"#   Rf               = %le\n"
		"#   Tms              = %le\n"
		"# 1: index\n"
		"# 2: time, s\n"
		"# 3: Vg, V\n"
		"# 4: Id, A\n"
		"# 5: pps:ch1 voltage, V\n"
		"# 7: pps:ch1 current, A\n"
		"# 6: pps:ch2 voltage, V\n"
		"# 8: pps:ch2 current, A\n"
		"# 9: vm voltage, V\n",
		start_time_struct.tm_year + 1900,
		start_time_struct.tm_mon + 1,
		start_time_struct.tm_mday,
		start_time_struct.tm_hour,
		start_time_struct.tm_min,
		start_time_struct.tm_sec,
		arg.sample_name,
		arg.Vg_start,
		arg.Vg_stop,
		arg.Vg_step,
		arg.Ig_max,
		arg.Vd,
		arg.Id_max,
		arg.Rf,
		arg.Tms
	);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_vac, strerror(r));
		goto worker_vac_header;
	}

	// === open gnuplot
	snprintf(buf, 300, "gnuplot > %s/gnuplot.log 2>&1", dir_str);
	gp = popen(buf, "w");
	if (gp == NULL)
	{
		fprintf(stderr, "# E: unable to open gnuplot pipe (%s)\n", strerror(errno));
		goto worker_gp_popen;
	}
	setlinebuf(gp);

	// === prepare gnuplot
	r = fprintf(gp,
		"set term qt noraise\n"
		"set xzeroaxis lt -1\n"
		"set yzeroaxis lt -1\n"
		"set grid\n"
		"set key right bottom\n"
		"set xrange [%le:%le]\n"
		"set xlabel \"Vg, V\"\n"
		"set ylabel \"Id, A\"\n"
		"set format y \"%%.3s%%c\"\n",
		arg.Vg_start,
		arg.Vg_stop
	);
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
		goto worker_gp_settings;
	}

	// === let the action begins!
	vac_index = 0;

	while(get_run())
	{
		voltage = arg.Vg_start + vac_index * arg.Vg_step;
		if (voltage > arg.Vg_stop)
		{
			set_run(0);
			break;
		}

		snprintf(buf, 300, "voltage %.3lf", voltage);
		gpib_write(pps_fd, buf);

		usleep(arg.Tms * 1e3);

		vac_time = get_time();
		if (vac_time < 0)
		{
			fprintf(stderr, "# E: Unable to get time\n");
			set_run(0);
			break;
		}

		gpib_write(pps_fd, "measure:voltage:all?");
		gpib_read(pps_fd, buf, 300);
		sscanf(buf, "%lf, %lf", &pps_voltage1, &pps_voltage2);

		gpib_write(pps_fd, "measure:current:all?");
		gpib_read(pps_fd, buf, 300);
		sscanf(buf, "%lf, %lf", &pps_current1, &pps_current2);

		gpib_write(vm_fd, "read?");
		gpib_read(vm_fd, buf, 300);
		vm_voltage = atof(buf);

		vac_voltage = pps_voltage2;
		vac_current = vm_voltage / arg.Rf;

		r = fprintf(vac_fp, "%d\t%le\t%le\t%le\t%.3le\t%.3le\t%.3le\t%.3le\t%.8le\n",
			vac_index,
			vac_time,
			vac_voltage,
			vac_current,
			pps_voltage1,
			pps_current1,
			pps_voltage2,
			pps_current2,
			vm_voltage
		);
		if(r < 0)
		{
			fprintf(stderr, "# E: Unable to print to file \"%s\" (%s)\n", filename_vac, strerror(r));
			set_run(0);
			break;
		}

		r = fprintf(gp,
			"set title \"i = %d, t = %.3lf s\"\n"
			"plot \"%s\" u 3:4 w l lw 1 title \"Vg = %.3lf V, Id = %le A\"\n",
			vac_index,
			vac_time,
			filename_vac,
			vac_voltage,
			vac_current
		);
		if(r < 0)
		{
			fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
			set_run(0);
			break;
		}

		new_V_range = V_check_range(V_range, vm_voltage);
		//fprintf(stderr, "# D: V_range = %d, vm_voltage = %le, new_V_range = %d\n", V_range, vm_voltage, new_V_range);
		if (new_V_range == -1)
		{
			set_run(0);
			break;
		}

		if (new_V_range != V_range)
		{
			V_range = new_V_range;
			switch(V_range)
			{
				case V_100mV:
					gpib_write(vm_fd, "voltage:dc:range 0.1");
					break;
				case V_1V:
					gpib_write(vm_fd, "voltage:dc:range 1");
					break;
				case V_10V:
					gpib_write(vm_fd, "voltage:dc:range 10");
					break;
				case V_100V:
					gpib_write(vm_fd, "voltage:dc:range 100");
					break;
			}
		}

		vac_index++;
	}

	gpib_write(pps_fd, "instrument:nselect 1");
	gpib_write(pps_fd, "voltage 0");
	gpib_write(pps_fd, "instrument:nselect 2");
	gpib_write(pps_fd, "voltage 0");
	gpib_write(pps_fd, "output 0");

	gpib_write(pps_fd, "system:beeper");

	r = fprintf(gp, "exit;\n");
	if(r < 0)
	{
		fprintf(stderr, "# E: Unable to print to gp (%s)\n", strerror(r));
	}

	worker_gp_settings:

	r = pclose(gp);
	if (r == -1)
	{
		fprintf(stderr, "# E: Unable to close gnuplot pipe (%s)\n", strerror(errno));
	}
	worker_gp_popen:


	worker_vac_header:

	r = fclose(vac_fp);
	if (r == EOF)
	{
		fprintf(stderr, "# E: Unable to close file \"%s\" (%s)\n", filename_vac, strerror(errno));
	}
	worker_vac_fopen:

	gpib_close(vm_fd);
	worker_vm_ibfind:

	gpib_close(pps_fd);
	worker_pps_ibfind:

	return NULL;
}

// === utils
static int get_run()
{
	int run_local;
	pthread_rwlock_rdlock(&run_lock);
		run_local = run;
	pthread_rwlock_unlock(&run_lock);
	return run_local;
}

static void set_run(int run_new)
{
	pthread_rwlock_wrlock(&run_lock);
		run = run_new;
	pthread_rwlock_unlock(&run_lock);
}

static double get_time()
{
	static int first = 1;
	static struct timeval t_first = {0};
	struct timeval t = {0};
	double ret;
	int r;

	if (first == 1)
	{
		r = gettimeofday(&t_first, NULL);
		if (r == -1)
		{
			fprintf(stderr, "# E: unable to get time (%s)\n", strerror(errno));
			ret = -1;
		}
		else
		{
			ret = 0.0;
			first = 0;
		}
	}
	else
	{
		r = gettimeofday(&t, NULL);
		if (r == -1)
		{
			fprintf(stderr, "# E: unable to get time (%s)\n", strerror(errno));
			ret = -2;
		}
		else
		{
			ret = (t.tv_sec - t_first.tv_sec) * 1e6 + (t.tv_usec - t_first.tv_usec);
			ret /= 1e6;
		}
	}

	return ret;
}

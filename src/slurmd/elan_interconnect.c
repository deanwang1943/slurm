/*
 * $Id$
 * $Source$
 *
 * Demo the routines in common/qsw.c
 * This can run mping on the local node (uses shared memory comms).
 * ./runqsw /usr/lib/mpi-test/mping 1 1024
 */
#define HAVE_LIBELAN3 

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <src/common/bitstring.h>
#include <src/common/qsw.h>
#include <src/common/xerrno.h>

#include <src/common/slurm_protocol_api.h>
#include <src/slurmd/task_mgr.h>
#include <src/slurmd/interconnect.h>

static int setenvf(const char *fmt, ...) ;
static int do_env(int nodeid, int procid, int nprocs) ;
/* exported module funtion to launch tasks */
/*launch_tasks should really be named launch_job_step*/
int launch_tasks ( launch_tasks_request_msg_t * launch_msg )
{
	return interconnect_init ( launch_msg );
}

/* Contains interconnect specific setup instructions and then calls 
 * fan_out_task_launch */
int interconnect_init ( launch_tasks_request_msg_t * launch_msg )
{
	pid_t pid;

	/* Process 1: */
	switch ((pid = fork())) 
	{
		case -1:
			xperror("fork");
			exit(1);
		case 0: /* child falls thru */
			break;
		default: /* parent */
			if (waitpid(pid, NULL, 0) < 0) 
			{
				xperror("wait");
				exit(1);
			}
			if (qsw_prgdestroy( launch_msg -> qsw_job ) < 0) {
				xperror("qsw_prgdestroy");
				exit(1);
			}
			exit(0);
	}

	/* Process 2: */
	if (qsw_prog_init(launch_msg -> qsw_job , launch_msg -> uid) < 0) 
	{
		xperror("qsw_prog_init");
		exit(1);
	}
	
	return fan_out_task_launch ( launch_msg ) ;
}

int interconnect_set_capabilities ( task_start_t * task_start )
{
	pid_t pid;
	/* all the following variables need to be set from the task_start struct 
	 * but currently are not */
	int nodeid , nprocs , i ; 

	if (qsw_setcap( task_start -> launch_msg -> qsw_job, i) < 0) {
		xperror("qsw_setcap");
		exit(1);
	}
	if (do_env(i, nodeid, nprocs) < 0) {
		xperror("do_env");
		exit(1);
	}

	pid = fork();
	switch (pid) {
		case -1:        /* error */
			xperror("fork");
			exit(1);
		case 0:         /* child falls thru */
			return SLURM_SUCCESS ;
			break;
		default:        /* parent */
			if (waitpid(pid, NULL, 0) < 0) 
			{
				xperror("waitpid");
				exit(1);
			}
			exit(0);
	}
}

/*
 * Set a variable in the callers environment.  Args are printf style.
 * XXX Space is allocated on the heap and will never be reclaimed.
 * Example: setenvf("RMS_RANK=%d", rank);
 */
static int setenvf(const char *fmt, ...) 
{
	va_list ap;
	char buf[BUFSIZ];
	char *bufcpy;
		    
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	bufcpy = strdup(buf);
	if (bufcpy == NULL)
		return -1;
	return putenv(bufcpy);
}

/*
 * Set environment variables needed by QSW MPICH / libelan.
 */
static int do_env(int nodeid, int procid, int nprocs)
{
	if (setenvf("RMS_RANK=%d", procid) < 0)
		return -1;
	if (setenvf("RMS_NODEID=%d", nodeid) < 0)
		return -1;
	if (setenvf("RMS_PROCID=%d", procid) < 0)
		return -1;
	if (setenvf("RMS_NNODES=%d", 1) < 0)
		return -1;
	if (setenvf("RMS_NPROCS=%d", nprocs) < 0)
		return -1;
	return 0;
}

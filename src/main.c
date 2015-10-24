/*
 * main.c: desktop session starter
 *
 * (C) Copyright 2009 Intel Corporation
 * Authors:
 *     Auke Kok <auke@linux.intel.com>
 *     Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <linux/limits.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <time.h>

#include <errno.h>
#include <systemd/sd-daemon.h>
#include <X11/Xlib.h>
#include <glib-2.0/glib.h>

#define NOTIFY_TIME 10000 /* 10 seconds */
#define SERVICE_NAME "xorg-launch-helper"

#ifdef ENABLE_TTRACE
#define TTRACE_BEGIN(NAME) traceBegin(TTRACE_TAG_INPUT, NAME)
#define TTRACE_END() traceEnd(TTRACE_TAG_INPUT)fe
#else //ENABLE_TTRACE
#define TTRACE_BEGIN(NAME)
#define TTRACE_END()
#endif //ENABLE_TTRACE

Display *xdpy;
static char displayname[256] = ":0";   /* ":0" */
static int tty = 1; /* tty1 */

static pthread_mutex_t notify_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t notify_condition = PTHREAD_COND_INITIALIZER;

 /* Callback to invoke every Watchdog time period */
gboolean notify_callback(gpointer data)
{
	/* Check Xorg is alive */
	XSync(xdpy, False);

	/* Notify alive status every watchdog time */
	sd_notifyf(0, "WATCHDOG=1\n" "sd_notify WATCHDOG Proc : %s(%d)", SERVICE_NAME, (int)getpid());

	return TRUE;
}

/* X I/O Error Handler */
static int x_io_error_handler(Display * dpy)
{
	fprintf(stderr, "[%s] Connection has been lost from Xorg ! [%s]\n", __FUNCTION__, strerror(errno));
	fprintf(stderr, "[%s] Kill self to trigger the failure of xorg.service !\n", __FUNCTION__);

	kill(getpid(), SIGKILL);

	return 0;
}

/* Create a watch dog for Xorg's crash/hang-ups */
static int watch_dog()
{
	GMainLoop *loop = NULL;

	/* Create a g_main_loop */
	loop = g_main_loop_new(NULL , FALSE);

	/* Open an X display */
	xdpy = NULL;
	if(getenv("DISPLAY")) xdpy = XOpenDisplay(NULL);
	else xdpy = XOpenDisplay(displayname);

	if(!loop || !xdpy)
	{
		fprintf(stderr, "Failed to create a watch dog for Xorg ! (loop=%d, xdpy=%d)\n", (!loop) ? 0 : 1, (!xdpy) ? 0 : 1);

		/* return a failure */
		return EXIT_FAILURE;
	}

	/* Set X error handler to check I/O error from Xorg */
	XSetIOErrorHandler(x_io_error_handler);

	/* notify ready status to systemd */
	sd_notifyf(0, "READY=1\n" "sd_notify READY Proc : %s(%d)", SERVICE_NAME, (int)getpid());

	g_timeout_add(NOTIFY_TIME, notify_callback, loop);

	/* start g_main_loop */
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	/* return success */
	return EXIT_SUCCESS;
}

static void usr1handler(int foo)
{
	/* Got the signal from the X server that it's ready */
	if (foo++) foo--; /*  shut down warning */

	pthread_mutex_lock(&notify_mutex);
	pthread_cond_signal(&notify_condition);
	pthread_mutex_unlock(&notify_mutex);
}



int main(int argc, char **argv)
{
	struct sigaction usr1;
	char *xserver = NULL;
	int ret;
	char vt[80];
	char xorg_log[PATH_MAX];
	struct stat statbuf;
	char *ptrs[32];
	int count = 0;
	char all[PATH_MAX] = "";
	int i;

	TTRACE_BEGIN("XORG:LAUNCH:START");

	/* Add watchdog routine for Xorg under systemd-daemon */
	if (getenv("XORG_WATCH_DOG_ONLY"))
	{
		/* Notify systemd-daemon that xorg-launch-helper is ready. */
		sd_notifyf(0, "READY=1\n" "sd_notify READY Proc : %s(%d)", SERVICE_NAME, (int)getpid());

		/* Run Xorg watch dog */
		ret = watch_dog();
		TTRACE_END();
		return ret;
	}

	/* Step 1: arm the signal */
	memset(&usr1, 0, sizeof(struct sigaction));
	usr1.sa_handler = usr1handler;
	sigaction(SIGUSR1, &usr1, NULL);

	/* Step 2: fork */
	ret = fork();
	if (ret) {
		struct timespec tv;

		fprintf(stderr, "Started Xorg[%d]", ret);

		/* setup sighandler for main thread */
		clock_gettime(CLOCK_REALTIME, &tv);
		tv.tv_sec += 10;

		pthread_mutex_lock(&notify_mutex);
		ret = pthread_cond_timedwait(&notify_condition, &notify_mutex, &tv);
		pthread_mutex_unlock(&notify_mutex);

		/* Add watchdog routine for Xorg under systemd-daemon */
		if(getenv("XORG_WATCH_DOG") && (ETIMEDOUT != ret) && (EINVAL != ret))
		{
			/* Release pthread resources */
			pthread_cond_destroy(&notify_condition);
			pthread_mutex_destroy(&notify_mutex);

			/* Notify systemd-daemon that xorg-launch-helper is ready. */
			sd_notifyf(0, "READY=1\n" "sd_notify READY Proc : %s(%d)", SERVICE_NAME, (int)getpid());

			/* Run Xorg watch dog */
			ret = watch_dog();

			TTRACE_END();
			return ret;
		}

		//FIXME - return an error code if timer expired instead.
		TTRACE_END();
		return EXIT_SUCCESS;
	}

	/* if we get here we're the child */

	/* Step 3: find the X server */

	/*
	 * set the X server sigchld to SIG_IGN, that's the
         * magic to make X send the parent the signal.
	 */
	signal(SIGUSR1, SIG_IGN);

	if (!xserver) {
		if (!access("/usr/bin/Xorg", X_OK))
			xserver = "/usr/bin/Xorg";
		else if (!access("/usr/bin/X", X_OK))
			xserver = "/usr/bin/X";
		else {
			fprintf(stderr, "No X server found!");
			TTRACE_END();
			return EXIT_FAILURE;
		}
	}

	/* assemble command line */
	memset(ptrs, 0, sizeof(ptrs));

	ptrs[0] = xserver;

	ptrs[++count] = displayname;

	/* non-suid root Xorg? */
	ret = stat(xserver, &statbuf);
	if (!(!ret && (statbuf.st_mode & S_ISUID))) {
		//add environment variable(20140923)
		char *xorgLogPath= NULL;
		xorgLogPath = getenv("XORG_LOG_PATH");

		if(xorgLogPath==NULL || access(xorgLogPath,X_OK)==-1)
			xorgLogPath = "/var/log";

		snprintf(xorg_log, PATH_MAX, "%s/Xorg.0.log", xorgLogPath);

		ptrs[++count] = strdup("-logfile");
		ptrs[++count] = xorg_log;
	} else {
		fprintf(stderr, "WARNING: Xorg is setuid root - bummer.");
	}

	ptrs[++count] = strdup("-nolisten");
	ptrs[++count] = strdup("tcp");

	ptrs[++count] = strdup("-noreset");

	for (i = 1; i < argc; i++)
		ptrs[++count] = strdup(argv[i]);

	for (i = 0; i <= count; i++) {
		strncat(all, ptrs[i], PATH_MAX - strlen(all) - 1);
		if (i < count)
			strncat(all, " ", PATH_MAX - strlen(all) - 1);
	}
	fprintf(stderr, "starting X server with: \"%s\"", all);

	execv(ptrs[0], ptrs);
	TTRACE_END();
	return EXIT_FAILURE;
}

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>

#include <signal.h>
#include <syslog.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/file.h>

static sigjmp_buf sigbuf;
static int sigbuf_valid;

static void
sigalrm_handler(int signo)
{
	(void)signo;	/* so that gcc doesn't complain */
	if (sigbuf_valid)
		siglongjmp(sigbuf, 1);
}


int
open_locked(const char *fname, int flags, ...)
{
	int mode = 0;

	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	
#ifndef O_EXLOCK
	int fd, save_errno;

	fd = open(fname, flags, mode);
	if (fd < 0)
		return(fd);
	if (flock(fd, LOCK_EX|((flags & O_NONBLOCK)? LOCK_NB: 0)) < 0) {
		save_errno = errno;
		close(fd);
		errno = save_errno;
		return(-1);
	}
	return(fd);
#else
	return(open(fname, flags|O_EXLOCK, mode));
#endif
}

int
do_timeout(int timeout, int dojmp)
{
	struct sigaction act;
	int ret = 0;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (timeout) {
		act.sa_handler = sigalrm_handler;
		if (sigaction(SIGALRM, &act, NULL) != 0)
			syslog(LOG_WARNING, "can not set signal handler: %m");
		if (dojmp) {
			ret = sigsetjmp(sigbuf, 1);
			if (ret)
				goto disable;
			/* else just programmed */
			sigbuf_valid = 1;
		}

		alarm(timeout);
	} else {
disable:
		alarm(0);

		act.sa_handler = SIG_IGN;
		if (sigaction(SIGALRM, &act, NULL) != 0)
			syslog(LOG_WARNING, "can not remove signal handler: %m");
		sigbuf_valid = 0;
	}

	return (ret);
}
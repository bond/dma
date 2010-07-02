#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>

void
errlog(int exitcode, const char *fmt, ...)
{
	int oerrno = errno;
	va_list ap;
	char *outs = NULL;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&outs, fmt, ap) == -1)
			outs = NULL;
		va_end(ap);
	}

	if (outs != NULL) {
		syslog(LOG_ERR, "%s: %m", outs);
		fprintf(stderr, "%s: %s: %s\n", getprogname(), outs, strerror(oerrno));
	} else {
		syslog(LOG_ERR, "%m");
		fprintf(stderr, "%s: %s\n", getprogname(), strerror(oerrno));
	}

	exit(exitcode);
}

void
errlogx(int exitcode, const char *fmt, ...)
{
	va_list ap;
	char *outs = NULL;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&outs, fmt, ap) == -1)
			outs = NULL;
		va_end(ap);
	}

	if (outs != NULL) {
		syslog(LOG_ERR, "%s", outs);
		fprintf(stderr, "%s: %s\n", getprogname(), outs);
	} else {
		syslog(LOG_ERR, "Unknown error");
		fprintf(stderr, "%s: Unknown error\n", getprogname());
	}

	exit(exitcode);
}
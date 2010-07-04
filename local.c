#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "dma.h"

int
deliver_local(struct qitem *it, const char **errmsg)
{
	char fn[PATH_MAX+1];
	char line[1000];
	const char *sender;
	const char *newline = "\n";
	char *args[2];
	size_t linelen;
	pid_t pid;
	int mbox;
	int error;
	int hadnl = 0;
	off_t mboxlen;
	time_t now = time(NULL);
	struct stat st;

	error = snprintf(fn, sizeof(fn), "%s/%s", _PATH_MAILDIR, it->addr);
	if (error < 0 || (size_t)error >= sizeof(fn)) {
		syslog(LOG_NOTICE, "local delivery deferred: %m");
		return (1);
	}

	/* If mbox dosn't exist, we spawn suid-program to create it */
	if (stat(fn, &st) == -1) {
		switch(errno) {
			case ENOENT:
				syslog(LOG_INFO, "spawning dma-create-mbox: %s", dma_create_mbox);
				pid = fork();
				if (pid == 0) {
					/* child */

					/* build args for dma-create-mbox */
					args[0] = strdup("dma-create-mbox");
					args[1] = strdup(it->addr);
					args[2] = NULL;

					if(args[0] == NULL || args[1] == NULL)  {
						syslog(LOG_NOTICE, "Unable to allocate memory (strdup): %m");
						return (1);
					}

					execve(dma_create_mbox, args, NULL);
					syslog(LOG_NOTICE, "Unable to execve(dma-create-mbox):%m");
					return (1);

				} else if (pid < 0) {
					/* error */
					syslog(LOG_NOTICE, "local delivery deferred: Unable to fork() for dma-create-mbox: %m");
					return (1);
				}
				waitpid(pid, &error, 0);
				break;
			case EACCES:
				syslog(LOG_NOTICE, "Local delivery deferred: Unable to open `%s': %m", fn);
				return (1);
			default:
				syslog(LOG_NOTICE, "Unable to stat `%s'", fn);
		}
	}

	/* wait for a maximum of 100s to get the lock to the file */
	do_timeout(100, 0);

	/* mailx removes users mailspool file if empty, so open with O_CREAT */
	mbox = open_locked(fn, O_WRONLY|O_APPEND|O_CREAT, 0660);
	if (mbox < 0) {
		int e = errno;

		do_timeout(0, 0);
		if (e == EINTR)
			syslog(LOG_NOTICE, "local delivery deferred: can not lock `%s'", fn);
		else
			syslog(LOG_NOTICE, "local delivery deferred: can not open `%s': %m", fn);
		return (1);
	}
	do_timeout(0, 0);

	mboxlen = lseek(mbox, 0, SEEK_END);

	/* New mails start with \nFrom ...., unless we're at the beginning of the mbox */
	if (mboxlen == 0)
		newline = "";

	/* If we're bouncing a message, claim it comes from MAILER-DAEMON */
	sender = it->sender;
	if (strcmp(sender, "") == 0)
		sender = "MAILER-DAEMON";

	if (fseek(it->mailf, 0, SEEK_SET) != 0) {
		syslog(LOG_NOTICE, "local delivery deferred: can not seek: %m");
		goto out;
	}

	error = snprintf(line, sizeof(line), "%sFrom %s\t%s", newline, sender, ctime(&now));
	if (error < 0 || (size_t)error >= sizeof(line)) {
		syslog(LOG_NOTICE, "local delivery deferred: can not write header: %m");
		goto out;
	}
	if (write(mbox, line, error) != error)
		goto wrerror;

	while (!feof(it->mailf)) {
		if (fgets(line, sizeof(line), it->mailf) == NULL)
			break;
		linelen = strlen(line);
		if (linelen == 0 || line[linelen - 1] != '\n') {
			syslog(LOG_CRIT, "local delivery failed: corrupted queue file");
			*errmsg = "corrupted queue file";
			error = -1;
			goto chop;
		}

		if (hadnl && strncmp(line, "From ", 5) == 0) {
			const char *gt = ">";

			if (write(mbox, gt, 1) != 1)
				goto wrerror;
			hadnl = 0;
		} else if (strcmp(line, "\n") == 0) {
			hadnl = 1;
		} else {
			hadnl = 0;
		}
		if ((size_t)write(mbox, line, linelen) != linelen)
			goto wrerror;
	}
	close(mbox);
	return (0);

wrerror:
	syslog(LOG_ERR, "local delivery failed: write error: %m");
	error = 1;
chop:
	if (ftruncate(mbox, mboxlen) != 0)
		syslog(LOG_WARNING, "error recovering mbox `%s': %m", fn);
out:
	close(mbox);
	return (error);
}

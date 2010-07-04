#include "dma.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <paths.h>
#include <string.h>
#include <strings.h>
#include <err.h>
#include <errno.h>
#include <syslog.h>
#include <grp.h>

int
main(int argc, char **argv)
{
	int error, fh;
	struct stat st;
	struct passwd *pwent = NULL;
	struct group *grent = NULL;
	char mbox[PATH_MAX+1];
	char *mbox_group = "mail";

	bzero(mbox, sizeof(mbox));
	if(argv[1] == NULL)
		errlogx(ENOENT, "No username given");

	/* check username is valid */
	errno = 0;
	pwent = getpwnam(argv[1]);
	if(pwent == NULL) {
		if(errno == 0)
			errlogx(ENOENT, "Unknown username '%s'", argv[1]);
		else
			errlogx(errno, "Unable to lookup username '%s': errno %d", argv[1], errno);
	}

	/* build mbox-path */
	error = snprintf(mbox, sizeof(mbox), "%s/%s", _PATH_MAILDIR, pwent->pw_name);
	if (error < 0 || (size_t)error >= sizeof(mbox))
		errlogx(ENAMETOOLONG, "Unable to copy mbox-path to memory (mbox-path > PATH_MAX ?)");


	/* check mbox-path exists */
	if (stat(mbox, &st) == 0)
		return (0);
		
	switch(errno) {
		case ENOENT:
			/* lookup gid */
			grent = getgrnam(mbox_group);
			if(grent == NULL) {
				if(errno == 0)
					errlogx(ENOENT, "Unknown group '%s'", mbox_group);
				else
					errlogx(errno, "Unable to lookup group '%s': errno %d", mbox_group, errno);
			}
		
			/* create mbox */
			fh = open_locked(mbox, O_WRONLY|O_APPEND|O_CREAT, 0660);
			if (fh < 0) {
				int e = errno;

				do_timeout(0, 0);
				if (e == EINTR) {
					syslog(LOG_NOTICE, "dma-create-mbox: can not lock `%s'", mbox);
					return (ENOLCK);
				} else {
					syslog(LOG_NOTICE, "dma-create-mbox: can not open `%s': %m", mbox);
					return (e);
				}
			}
			/* set correct perms */
			if(fchown(fh, pwent->pw_uid, grent->gr_gid) == -1)
				errlogx(errno, "Unable to chown mbox-file %s, errno: %d (%s)", mbox, errno, strerror(errno));
			if(fchmod(fh, (mode_t)0660) == -1)
				errlogx(errno, "Unable to chmod mbox-file %s, errno: %d (%s)", mbox, errno, strerror(errno));
			break;
		default:
			errx(errno, "Unable to check if mbox (%s) exists: %s", mbox, strerror(errno));
	}

	return (0);
}
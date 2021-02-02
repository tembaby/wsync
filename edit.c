/* $Id: edit.c,v 1.11 2004/04/21 22:39:23 te Exp $ */ 

/*
 * Copyright (c) 2002, 2003, 2004 Tamer Embaby <tsemba@menanet.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <err.h>
#if defined (__OpenBSD__)
# include <paths.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <wsync.h>

#define TMP_TEMPLATE		"ws.XXXX"
static	char tempname_dst[PATH_MAX];
static	char tempname_exc[PATH_MAX];

static	void cleanup(void);
static	int call_editor(char *);
static	int install_dest_file(char *,int,const char *);
static	int install_exclude_file(char *,int,const char *);

static int
call_editor(temp)
	char	*temp;
{
	int status;
	char *editor;
	struct stat begin, end;
	char execbuf[128];

	if (lstat(temp, &begin) < 0) {
		warn("call_editor: lstat(%s)", temp);
		return (-1);
	}
	if (S_ISLNK(begin.st_mode)) {
		warnx("call_editor: %s: is symbolic link", temp);
		return (-1);
	}
	if ((editor = getenv("EDITOR")) == NULL &&
	    (editor = getenv("VISUAL")) == NULL) {
		warnx("call_editor: neither $EDITOR nor $VISUAL defined");
		editor = _PATH_VI;
		warnx("call_editor: using system default: %s", editor);
		/*return (-1);*/
	}
	if (snprintf(execbuf, sizeof(execbuf), "%s %s", 
	    editor, temp) >= sizeof(execbuf)) {
		warnx("call_editor: exec buffer overrun");
		return (-1);
	}
	DPRINT(2, ("executing %s\n", execbuf));
	if ((status = system(execbuf)) < 0) {
		warn("call_editor: system(%s)", execbuf);
		return (-1);
	}
	if (WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 1) {
		warnx("call_editor: command was not successful: "
		    "error code: %d", WEXITSTATUS(status));
		return (-1);
	}
	if (lstat(temp, &end) < 0) {
		warn("call_editor: lstat(%s)", temp);
		return (-1);
	}
	if (S_ISLNK(end.st_mode)) {
		warnx("call_editor: %s: is symbolic link", temp);
		return (-1);
	}
	if (begin.st_mtime == end.st_mtime &&
	    begin.st_size == end.st_size) {
		fprintf(stderr, "no changes made\n");
		return (-1);
	}
	return (0);
}

static int
install_dest_file(temp, fd, secname)
	char	*temp;
	int	fd;
	const	char *secname;
{
	FILE *fp;

	if (!(fp = fdopen(fd, "w"))) {
		warn("install_dest_file: fdopen(%d)", fd);
		return (-1);
	}
	fprintf(fp, "# $Id: edit.c,v 1.11 2004/04/21 22:39:23 te Exp $\n#\n");
	fprintf(fp, "# Editing (@)%s section.\n", secname);
	fprintf(fp, "# Key/value pair is white space delimited, "
	    "and they must both exist\n");
	fprintf(fp, "#\n# DO NOT DELETE NEXT LINE\n");
	fprintf(fp, "(@)destination\n\n");
	fprintf(fp, "service\t\t# ftp/local\n");
	fprintf(fp, "connection active\t\t# active/passive/none (FTP only)\n");
	fprintf(fp, "host none\n");
	fprintf(fp, "port 21\n");
	fprintf(fp, "proxy none\n");
	fprintf(fp, "proxy-port 0\n");
	fprintf(fp, "user none\t\t# FTP user name\n");
	fprintf(fp, "password none\n");
	fprintf(fp, "root /\n");
	fprintf(fp, "#file /path/to/file_that_contain_destination_info\n");
	fchown(fd, getuid(), getgid());
	fclose(fp);
	return (0);
}

static int
install_exclude_file(temp, fd, secname)
	char	*temp;
	int	fd;
	const	char *secname;
{
	FILE *fp;

	if (!(fp = fdopen(fd, "w"))) {
		warn("install_dest_file: fdopen(%d)", fd);
		return (-1);
	}
	fprintf(fp, "# $Id: edit.c,v 1.11 2004/04/21 22:39:23 te Exp $\n#\n");
	fprintf(fp, "# Editing (@)%s section.\n", secname);
	fprintf(fp, "# Enter a file name in a line by itself, "
	    "or wild card of the form:\n#   *.ext, e.g., *.o\n");
	fprintf(fp, "# Configuration files are excluded by default.\n");
	fprintf(fp, "#\n# DO NOT DELETE NEXT LINE\n");
	fprintf(fp, "(@)exclude\n\n");
	fprintf(fp, "#*.core\n#*.o\n#*.swp");
	fchown(fd, getuid(), getgid());
	fclose(fp);
	return (0);
}

int
installsection(secname, file, filelen)
	const	char *secname;
	char	*file;
	size_t	filelen;
{
	int fd, dst;
	char tempname[PATH_MAX];
	
	if (strcmp(secname, "destination") == 0)
		dst = 1;
	else if (strcmp(secname, "exclude") == 0)
		dst = 0;
	else {
		warnx("Invalid section name: %s", secname);
		return (-1);
	}
	
	snprintf(tempname, sizeof(tempname), "%s%s", _PATH_TMP, TMP_TEMPLATE);
	if ((fd = mkstemp(tempname)) < 0) {
		return (-1);
	}
	if (fcntl(fd, F_SETFD, 1) < 0) {
		return (-1);
	}
	atexit(cleanup);
	if (dst == 1) {
		if (install_dest_file(tempname, fd, secname) < 0)
			return (-1);
		(void)strlcpy(tempname_dst, tempname, sizeof(tempname_dst));
	} else {
		if (install_exclude_file(tempname, fd, secname) < 0)
			return (-1);
		(void)strlcpy(tempname_exc, tempname, sizeof(tempname_exc));
	}
	if (call_editor(tempname) < 0)
		return (-1);
	(void)strlcpy(file, tempname, filelen);
	return (0);
}

static void
cleanup()
{

	DPRINT(2, ("cleaning temporary files\n"));
	if (tempname_dst[0] != '\0')
		(void)unlink(tempname_dst);
	if (tempname_exc[0] != '\0')
		(void)unlink(tempname_exc);
	return;
}

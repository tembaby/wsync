/* $Id: fsops.c,v 1.13 2004/04/21 22:39:23 te Exp $ */ 

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
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#if defined (USE_MMAP)
# include <sys/mman.h>
#endif

#if !defined (TM_YEAR_BASE)
# define TM_YEAR_BASE		1900
#endif	/* TM_YEAR_BASE */

#include <wsync.h>
#include <strlib.h>
#include <ftplib.h>

/*
 * Local filesystem operations.
 */
static	int loc_init(void *);
static	void loc_deinit(void *);
static	int loc_chdir(const char *,void *);
static	int loc_mkdir(const char *,void *);
static	int loc_put(const char *,const char *,void *);
static	int loc_dele(const char *,void *);
static	time_t loc_mtime(const char *,void *);
static	off_t loc_size(const char *,void *);

struct fsops localops = {
	NULL,
	NULL,
	loc_init,
	loc_deinit,
	loc_chdir,
	loc_mkdir,
	loc_put,
	loc_dele,
	loc_mtime,
	loc_size
};

/*
 * FTP file operations.
 */
static	int ftp_fsreg(void *);
static	void ftp_fsunreg(void *);
static	int ftp_init(void *);
static	void ftp_deinit(void *);
static	int ftp_chdir(const char *,void *);
static	int ftp_mkdir(const char *,void *);
static	int ftp_put(const char *,const char *,void *);
static	int ftp_dele(const char *,void *);
static	time_t ftp_mtime(const char *,void *);
static	off_t ftp_size(const char *,void *);
static	void ftp_hash(netbuf *,int,void *);

static	void ftp_debug(const char *);
static	char filemode(const char *);

struct fsops ftpops = {
	ftp_fsreg,
	ftp_fsunreg,
	ftp_init,
	ftp_deinit,
	ftp_chdir,
	ftp_mkdir,
	ftp_put,
	ftp_dele,
	ftp_mtime,
	ftp_size
};

#define VTOFS(v)	((struct fs_desc *)(v))
#define FSTOWS(fs)	((struct wsync_target *)((fs)->fsd_wstp))
#define FSTONB(fs)	((netbuf *)((fs)->fsd_data))

/*
 * FTP file operations.
 */

static int
ftp_fsreg(vp)
	void	*vp;
{

	DPRINT(3, ("ftp_fsreg:\n"));
	ftplib_init();
	return (0);
}

static void
ftp_fsunreg(vp)
	void	*vp;
{

	DPRINT(3, ("ftp_fsunreg:\n"));
	ftplib_deinit();
	return;
}

static int
ftp_init(vp)
	void	*vp;
{
	int debug;
	char *p;
	netbuf *ftp;
	struct fs_desc *fsp;
	struct wsync_target *wtp;

	fsp = VTOFS(vp);
	wtp = FSTOWS(fsp);
	p = NULL;
	DPRINT(2, ("ftp_init: remote root is %s\n", wtp->wt_dst->d_root));
	if (wsync_debug != 0)
		/*debug = FTPLIB_LOG_CLIENT;*/
		debug = FTPLIB_LOG_HARDCORE;
	else
		debug = FTPLIB_LOG_NORMAL;
	if (wtp->wt_dst->d_proxy[0] != '\0' &&
	    strcmp(wtp->wt_dst->d_proxy, "none") != 0)
		p = wtp->wt_dst->d_proxy;
	ftplib_set_debug(debug);
	ftplib_set_debug_handler(NULL, ftp_debug);
	DPRINT(2, ("ftp_init: host (%s), proxy (%s)\n",
	    wtp->wt_dst->d_host, wtp->wt_dst->d_proxy));
	if (ftplib_connect(wtp->wt_dst->d_host, p, &ftp) == 0) {
		warn("ftp_init: ftplib_connect failed!");
		return (-1);
	}
	ftplib_set_debug_handler(ftp, ftp_debug);
	if (ftplib_login(wtp->wt_dst->d_user, wtp->wt_dst->d_passwd, 
	    ftp) == 0) {
		warnx("ftp_init: ftplib_login: %s", ftplib_last_response(ftp));
		return (-1);
	}
	if (wtp->wt_dst->d_flags & WS_DEST_FTP_PASSIVE)
		(void)ftplib_options(FTPLIB_CONNMODE, FTPLIB_PASSIVE, ftp);
	if (wsync_debug != 0) {
		(void)ftplib_options(FTPLIB_CALLBACKBYTES, 1024, ftp);
		(void)ftplib_options(FTPLIB_XFERCALLBACK, (long)ftp_hash, ftp);
	}
	if (ftplib_chdir(wtp->wt_dst->d_root, ftp) == 0) {
		if (ftplib_mkdir(wtp->wt_dst->d_root, ftp) == 0) {
			warnx("ftp_init: ftplib_mkdir: %s", 
			    ftplib_last_response(ftp));
			ftplib_quit(ftp);
			return (-1);
		}
		if (ftplib_chdir(wtp->wt_dst->d_root, ftp) == 0) {
			warnx("ftp_init: ftplib_chdir: %s", 
			    ftplib_last_response(ftp));
			ftplib_quit(ftp);
			return (-1);
		}
	}
	fsp->fsd_data = ftp;
	return (0);
}

static void
ftp_deinit(vp)
	void	*vp;
{
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_deinit: \n"));
	fsp = VTOFS(vp);
	ftplib_quit(FSTONB(fsp));
	return;
}

static int 
ftp_chdir(dir, vp)
	const	char *dir;
	void	*vp;
{
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_chdir: %s\n", dir));
	fsp = VTOFS(vp);
	if (ftplib_chdir(dir, FSTONB(fsp)) == 0)
		return (-1);
	return (0);
}

static int 
ftp_mkdir(dir, vp)
	const	char *dir;
	void	*vp;
{
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_mkdir: %s\n", dir));
	fsp = VTOFS(vp);
	if (ftplib_mkdir(dir, FSTONB(fsp)) == 0)
		return (-1);
	return (0);
}

static int 
ftp_put(from, to, vp)
	const	char *from;	/* Full/relative path to input file */
	const	char *to;	/* path to remote location */
	void	*vp;
{
	int fmode;
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_put: %s -> %s\n", from, to));
	fsp = VTOFS(vp);
	fmode = filemode(from);
	if (ftplib_put(from, to, fmode, FSTONB(fsp)) == 0)
		return (-1);
	return (0);
}

static int 
ftp_dele(file, vp)
	const	char *file;
	void	*vp;
{
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_dele: %s\n", file));
	fsp = VTOFS(vp);
	if (ftplib_delete(file, FSTONB(fsp)) == 0)
		return (-1);
	return (0);
}

static time_t 
ftp_mtime(file, vp)
	const	char *file;
	void	*vp;
{
	int yy, mo, day, hour, min, sec;
	time_t rtime;
	char *timestr;
	struct fs_desc *fsp;
	struct tm timebuf;
	char resp[128];

	DPRINT(3, ("ftp_mtime: %s\n", file));
	fsp = VTOFS(vp);
	if (ftplib_moddate(file, resp, sizeof(resp), FSTONB(fsp)) == 0)
		return (-1);
	/*
	 * Next code taken from /usr/src/usr.bin/ftp/util.c
	 * BSD sources.
	 */
	rtime = -1;
	/*
	 * time-val = 14DIGIT [ "." 1*DIGIT ]
	 *		YYYYMMDDHHMMSS[.sss]
	 * mdtm-response = "213" SP time-val CRLF / error-response
	 */
	/* TODO: parse .sss as well, use timespecs. */

	timestr = resp;
	/* Repair `19%02d' bug on server side */
	if (strncmp(timestr, "191", 3) == 0) {
		fprintf(stderr,
    "Y2K warning! Fixed incorrect time-val received from server.\n");
		timestr[0] = ' ';
		timestr[1] = '2';
		timestr[2] = '0';
	}
	sscanf(resp, "%04d%02d%02d%02d%02d%02d", &yy, &mo,
		&day, &hour, &min, &sec);
	memset(&timebuf, 0, sizeof(timebuf));
	timebuf.tm_sec = sec;
	timebuf.tm_min = min;
	timebuf.tm_hour = hour;
	timebuf.tm_mday = day;
	timebuf.tm_mon = mo - 1;
	timebuf.tm_year = yy - TM_YEAR_BASE;
	timebuf.tm_isdst = -1;
	rtime = mktime(&timebuf);
	if (rtime == -1)
		fprintf(stderr, "Can't convert %s to a time.\n", resp);
#if defined (BSD)
	/*
	 * SunOS 5.8 doesn't have the tm_gmtoff. And I don't have any clue how
	 * to convert to GMT for now (Sun Apr 11 18:16:49 EET 2004).
	 */
	else
		rtime += timebuf.tm_gmtoff;	/* conv. local -> GMT */
#endif	/* BSD */
	return (rtime);
}

static off_t 
ftp_size(file, vp)
	const	char *file;
	void	*vp;
{
	int fmode;
	size_t fsiz;
	struct fs_desc *fsp;

	DPRINT(3, ("ftp_size: %s\n", file));
	fsp = VTOFS(vp);
	fmode = filemode(file);
	if (ftplib_size(file, &fsiz, fmode, FSTONB(fsp)) == 0)
		return (0);
	return ((off_t)fsiz);
}

static void 
ftp_debug(msg)
	const	char *msg;
{

	fprintf(stderr, "ftp ===> %s", msg);
	return;
}

/*
 * Try to suggest a file type (ascii/binary) for a given file, by
 * reading first 200 bytes and examining them for control/unprintable
 * characters.
 */
static char 
filemode(file)
	const	char *file;
{
	int fd;
	ssize_t siz;
	u_char *p;
	u_char buf[200];

	if ((fd = open(file, O_RDONLY, 0)) < 0) {
		return (FTPLIB_BINARY);
	}
	if ((siz = read(fd, buf, sizeof(buf))) < 0) {
		close(fd);
		return (FTPLIB_BINARY);
	}
	close(fd);
	for (p = buf; (u_long)p < (u_long)buf + siz; p++)
		if ((*p > 128 || *p < 32) && strcont(*p, "\r\n\t") == 0) {
			return (FTPLIB_BINARY);
		}
	return (FTPLIB_ASCII);
}

static void 
ftp_hash(netbuf *ftp, int bytes, void *arg)
{

	printf("#");
	fflush(stdout);
	return;
}

/*
 * Local filesystem operations.
 */

static int 
loc_init(vp)
	void	*vp;
{
	struct fs_desc *fsp;
	struct wsync_target *wtp;

	fsp = VTOFS(vp);
	wtp = FSTOWS(fsp);
	DPRINT(3, ("loc_init: root %s\n", wtp->wt_dst->d_root));
	if (mkdir(wtp->wt_dst->d_root, 0755) < 0 && errno != EEXIST) {
		warn("mkdir(%s)", wtp->wt_dst->d_root);
		return (-1);
	}
	return (0);
}

static void 
loc_deinit(vp)
	void	*vp;
{

	return;
}

static int 
loc_chdir(dir, vp)
	const	char *dir;
	void	*vp;
{

	DPRINT(3, ("loc_chdir: %s\n", dir));
	return (chdir(dir));
}

static int 
loc_mkdir(dir, vp)
	const	char *dir;
	void	*vp;
{

	DPRINT(3, ("loc_mkdir: %s\n", dir));
	return (mkdir(dir, 0755));
}

/*
 * cp(1) for mmap(2).
 */
static int 
loc_put(from, to, vp)
	const	char *from, *to;
	void	*vp;
{
	int err = 0;
	int from_fd, to_fd, rbcnt, wbcnt;
	char *p;
	struct stat from_st;
	char buffer[MAXBSIZE];

	DPRINT(3, ("loc_put: %s -> %s\n", from, to));
	if ((from_fd = open(from, O_RDONLY)) < 0) {
		fprintf(stderr, "loc_put: open(%s ->): %s\n", 
		    from, strerror(errno));
		return (-1);
	}
	if (fstat(from_fd, &from_st) < 0) {
		fprintf(stderr, "loc_put: stat(%s): %s\n",
		    from, strerror(errno));
		close(from_fd);
		return (-1);
	}
	if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 
	    from_st.st_mode)) < 0) {
		fprintf(stderr, "loc_put: open(-> %s): %s\n", 
		    to, strerror(errno));
		close(from_fd);
		return (-1);
	}
#if defined (USE_MMAP)
#if !defined (MAP_FILE)
# define MAP_FILE		0	/* SunOS */
#endif	/* MAP_FILE */
	if (from_st.st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)from_st.st_size, PROT_READ,
		    MAP_FILE | MAP_SHARED, from_fd, (off_t)0)) == MAP_FAILED) {
			fprintf(stderr, "loc_put: mmap(%s) failed; "
			    "continuing with normal copy: %s\n", from,
			    strerror(errno));
		} else {
			if (write(to_fd, p, from_st.st_size) != 
			    from_st.st_size) {
				fprintf(stderr, "loc_put: write(%s): %s\n",
				    to, strerror(errno));
				err = -1;
			}
			if (munmap(p, (size_t)from_st.st_size) < 0) {
				fprintf(stderr, "loc_put: munmap: %s\n",
				    strerror(errno));
				err = -1;
			}
			close(from_fd);
			close(to_fd);
			return (err);
		}
	}
#endif	/* USE_MMAP */
	for (;;) {
		rbcnt = read(from_fd, buffer, sizeof(buffer));
		if (rbcnt < 0) {
			fprintf(stderr, "loc_put: read(%s): %s\n",
			    from, strerror(errno));
			err = -1;
			break;
		}
		if (rbcnt == 0)
			break;
		wbcnt = write(to_fd, buffer, rbcnt);
		if (wbcnt != rbcnt || wbcnt < 0) {
			fprintf(stderr, "loc_put: write(%s): %s\n",
			    from, strerror(errno));
			err = -1;
			break;
		}
	}
	close(from_fd);
	close(to_fd);
	return (err);
}

static int 
loc_dele(file, vp)
	const	char *file;
	void	*vp;
{

	DPRINT(3, ("loc_dele: %s\n", file));
	return (unlink(file));
}

static time_t 
loc_mtime(file, vp)
	const	char *file;
	void	*vp;
{
	struct stat sb;

	DPRINT(3, ("loc_mtime: %s\n", file));
	if (stat(file, &sb) < 0) {
		fprintf(stderr, "loc_mtime: %s: %s\n", file, strerror(errno));
		return (-1);
	}
	return (sb.st_mtime);
}

static off_t 
loc_size(file, vp)
	const	char *file;
	void	*vp;
{
	struct stat sb;

	DPRINT(3, ("loc_size: %s\n", file));
	if (stat(file, &sb) < 0) {
		fprintf(stderr, "loc_size: %s: %s\n", file, strerror(errno));
		return (-1);
	}
	return (sb.st_size);
}

int
fs_reg()
{

	if (ftpops.fso_fsreg != NULL)
		(*ftpops.fso_fsreg)(NULL);
	if (localops.fso_fsreg != NULL)
		(*localops.fso_fsreg)(NULL);
	return (0);
}

void
fs_unreg()
{

	if (ftpops.fso_fsunreg != NULL)
		(*ftpops.fso_fsunreg)(NULL);
	if (localops.fso_fsunreg != NULL)
		(*localops.fso_fsunreg)(NULL);
	return;
}

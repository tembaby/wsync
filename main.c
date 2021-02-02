/* $Id: main.c,v 1.15 2004/04/21 22:39:23 te Exp $ */

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
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <dirent.h>
#if defined (__OpenBSD__)
# include <util.h>
#elif defined (sun)
# include <fparseln.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <wsync.h>
#include <strlib.h>
#include <ftplib.h>

#if !defined (__dead)
# define __dead
#endif

/*
 * Handy macros.
 */
#define curdir(d)	(d[0] == '.' && d[1] == '\0')
#define noproxy(p)	(p[0] == '\0' || strcmp(p, "none") == 0)

/*
 * List initialization for targets.
 */
#define WST_INIT(wtp) do { \
	TAILQ_INIT(&((wtp)->wt_fs_files)); \
	TAILQ_INIT(&((wtp)->wt_fs_dirs)); \
	TAILQ_INIT(&((wtp)->wt_conf_files)); \
	TAILQ_INIT(&((wtp)->wt_excs)); \
} while (0)

static	struct wsync_target *get_trgt(char *,const char *);
static	int build_disk_entries(struct wsync_target *);
static	int build_conf_entries(struct wsync_target *);
static	void trelease(struct wsync_target *);
static	int getdest(struct wsync_target *,const char *,int);
static	FILE *open_conf(struct wsync_target *,const char *);
static	void dumpwsttree(struct wsync_target *);
static	int write_conf(struct wsync_target *);
static	struct exclude_entry *newexc(char *);
static	int build_exclude_list(struct wsync_target *,int);
static	int excluded(struct wsync_target *,char *);
static	int gordir(struct wsync_target *);
static	int goroot(struct wsync_target *);
static	char *dirname(const char *);
static	int inherit(register struct wsync_target *,
		register struct wsync_target *);
static	int settpath(char *,int,char *,char *,char *);
static	__dead void usage(void);
static	int wsyncroot(void);

time_t	utc_time(time_t);
int	wsync(struct wsync_target *);

extern	char *__progname;
int	wsync_debug = 0;
char	*wsync_root;

#if defined (sun)
char	*__progname;

static char *
getprogname(char *argv0)
{

	return (argv0);
}
#endif	/* sun */

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	int o, usecwd, connected;
	int no_recurse, no_fsymlink, prune, no_run, ghtml;
	char *dir, *dfile, *cfile, *dbgenv;
	struct wsync_target *wtp;
	char cwd[PATH_MAX];

	dfile = NULL;
	cfile = NULL;
	wsync_root = NULL;
	connected = 0;
	no_recurse = no_fsymlink = prune = no_run = ghtml = 0;

#if defined (sun)
	__progname = getprogname(argv[0]);
#endif	/* sun */
	
	while ((o = getopt(argc, argv, "dvhD:NSPCgR:?")) != -1) {
		switch (o) {
		case 'd':
			++wsync_debug;
			break;
		case 'v':
			printf("wsync v%s (%s)\n", VERSION, AUTHER);
			printf("\n");
			printf("Contains ftplib:\n");
			printf("%s\n", ftplib_version(FTPLIB_VER_FULL));
			exit(EXIT_SUCCESS);
		case 'D':
			/* Destination section reference file. */
			dfile = optarg;
			break;
		case 'N':
			/* No recursive behavior. */
			no_recurse = 1;
			break;
		case 'S':
			/* Don't follow symbolic links. */
			no_fsymlink = 1;
			break;
		case 'P':
			/* 
			 * Try to remotely delete files that 
			 * are locally deleted.
			 */
			prune = 1;
			break;
		case 'C':
			no_run = 1;
			break;
		case 'g':
			ghtml = 1;
			break;
		case 'R':
			wsync_root = optarg;
			break;
		case 'h':
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	
	if ((dbgenv = getenv("WSYNC_DEBUG")) != NULL) {
		if (strcasecmp(dbgenv, "yes") == 0 || atoi(dbgenv) > 0)
			++wsync_debug;
	}
	
	usecwd = 0;
	if (*argv == NULL)
		usecwd = 1;
	cfile = WSYNC_CONF;
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "getcwd: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Check wsync root only if we are not generating HTML */
	if (ghtml == 0)
		wsyncroot();
	
	/* Specefic FS operations initialization */
	(void)fs_reg();
	
	/*
	 * Support command line syntax: wsync dir0 dir1 dir1 ...
	 */
	for (;;) { 
		dir = *argv;
		if (dir == NULL && usecwd != 0)
			dir = ".";
		if (dir == NULL)
			break;
		if (ghtml != 0) {
			(void)htmldir(dir, NULL);
			if (usecwd != 0)
				break;
			continue;
		}
		
		if ((wtp = get_trgt(dir, cfile)) == NULL) {
			fprintf(stderr, "ignoring ``%s''\n", dir);
			++argv;
			continue;
		}
		
		/*
		 * Set the inheritable flags.
		 */
		if (no_recurse != 0)
			wtp->wt_flags |= WS_WF_NORECURSE;
		if (no_fsymlink != 0)
			wtp->wt_flags |= WS_WF_NOSYMLINK;
		if (prune != 0)
			wtp->wt_flags |= WS_WF_PRUNE;
		if (no_run != 0)
			wtp->wt_flags |= WS_WF_NORUN;
		DPRINT(2, ("system flags 0x%x\n", wtp->wt_flags));
		
		/*
		 * Load the destination folder information from
		 * configuration file or let the user edit this
		 * section if it doesn't exist.
		 */
		if (getdest(wtp, dfile, 0) < 0)
			goto release;
		
		/*
		 * Build an image of the contents of the directory
		 * to be synchronized.  This contains subdirectories
		 * names (not contents).
		 */
		if (build_disk_entries(wtp) < 0)
			exit(EXIT_FAILURE);
		
		if (wsync_debug != 0)
			dumpwsttree(wtp);
		wtp->wt_fsws = wtp;
		
		/*
		 * On FTP wsync connect to remote server.
		 * Don't bother if we are on check-only mode.
		 */
		if (RUNNABLE(wtp) != 0) {
			if ((*wtp->wt_fops->fso_init)(wtp->wt_fs) < 0)
				goto release;
			connected = 1;
		}
		
		/*
		 * Do the actual synchronization.
		 */
		wsync(wtp);
		if (RUNNABLE(wtp) != 0)
			write_conf(wtp);
release:
		if (RUNNABLE(wtp) != 0 && connected != 0) {
			(*wtp->wt_fops->fso_deinit)(wtp->wt_fs);
		}

		if (usecwd != 0)
			break;
		
		if (chdir(cwd) < 0) {
			fprintf(stderr, "chdir(%s): %s\n", 
			    cwd, strerror(errno));
			fprintf(stderr, "can't continue.\n");
			exit(EXIT_FAILURE);
		}
		
		if (connected != 0)
			(*wtp->wt_fops->fso_deinit)(wtp->wt_fs);

		trelease(wtp);
		++argv;
	}
	fs_unreg();
	return (0);
}

/*
 * Allocate `head' target and make it CWD.
 * Load the exclude list here if the configuration file exist,
 * or let the user enter them manually (/usr/bin/vi).  This list
 * is inheritable among subdirectories.
 */
static struct wsync_target *
get_trgt(dirn, conf_file)
	char	*dirn;
	const	char *conf_file;
{
	int clen;
	char *p, *oldc;
	FILE *f;
	struct wsync_target *wtp;
	char cdir[PATH_MAX];
	char tempname[PATH_MAX];

	wtp = xmalloc(sizeof(struct wsync_target));
	memset(wtp, 0, sizeof(struct wsync_target));
	
	/*
	 * Change directory to the head target so we can start building
	 * our database and basic local directory information later.
	 */
	if (curdir(dirn) == 0) {
		if (chdir(dirn) < 0) {
			fprintf(stderr, "get_trgt: chdir(%s): %s\n", 
			    dirn, strerror(errno));
			free(wtp);
			return (NULL);
		}
	}
	if (getcwd(cdir, sizeof(cdir)) == NULL) {
		fprintf(stderr, "get_trgt(%s): can't determine CWD!: %s\n",
		    dirn, strerror(errno));
		free(wtp);
		return (NULL);
	}
	
	/* 
	 * Concentrate with the last path component since this name will
	 * be used on making remote hierarchy and in $WSYNC_ROOT database.
	 */
	p = rindex(cdir, '/');
	if (p == NULL)
		p = cdir;
	else
		++p;
	if (strlcpy(wtp->wt_name, p, sizeof(wtp->wt_name) - 1) >= 
	    sizeof(wtp->wt_name)) {
		fprintf(stderr, "get_target(%s): %s too long\n", dirn, p);
		free(wtp);
		return (NULL);
	}
	
	/*
	 * Our configuration directory path in the $WSYNC_ROOT
	 * database directory.
	 */
	clen = strlen(wsync_root) + strlen(wtp->wt_name) + 
	   strlen(conf_file) + 4; /* `NUL' + 2*`/' */
	if (clen >= PATH_MAX) {
		fprintf(stderr, "get_target(%s): wsync root path too long\n",
		    wtp->wt_name);
		free(wtp);
		return (NULL);
	}
	wtp->wt_conf_name = xmalloc(clen);
	/* First create the directory if it doens't exist */
	snprintf(wtp->wt_conf_name, clen, "%s/%s", wsync_root, wtp->wt_name);
	if (mkdir(wtp->wt_conf_name, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "get_target(%s): %s: %s\n", wtp->wt_name,
		    wtp->wt_conf_name, strerror(errno));
		free(wtp);
		return (NULL);
	}
	/* Then we construct the configuration file path name */
	snprintf(wtp->wt_conf_name, clen, "%s/%s/%s", wsync_root, 
	    wtp->wt_name, conf_file);

	if (wsync_debug != 0)
		printf("sync'ing directory %s\n", wtp->wt_name);

	WST_INIT(wtp);
	
	/* Load exclude list or edit it. */
	if ((f = open_conf(wtp, "exclude")) == NULL) {
		printf("\nNo configuration file or no (@)exclude\n"
		    "Enter manually\n\n");
		(void)sleep(1);
		if (installsection("exclude", tempname, 
		    sizeof(tempname) - 1) < 0) {
			fprintf(stderr, "canceled.  preserving defaults.\n");
		}
		/* Read the temporary file. */
		oldc = wtp->wt_conf_name;
		wtp->wt_conf_name = tempname;
		build_exclude_list(wtp, 1);
		wtp->wt_conf_name = oldc;
	}
	return (wtp);
}

/*
 * Create list of files to be examined later by wsync().
 * Create entries for subdiresctories.
 */
static int 
build_disk_entries(wtp)
	struct	wsync_target *wtp;
{
	int wtype, blen;
	char *n;
	DIR *dh;
	struct dirent *de;
	struct wsync_entry *wep;
	struct wsync_target *swtp;
	struct stat st;
	char symlinkname[PATH_MAX + 1];

	/*
	 * Callers must chdir to target directory before calling us.
	 */
	if ((dh = opendir(".")) == NULL) {
		fprintf(stderr, "buildwstdb: opendir(%s): %s\n", wtp->wt_name,
		    strerror(errno));
		return (-1);
	}
	for (;;) {
		if ((de = readdir(dh)) == NULL)
			break;
		if (de->d_name[0] == '.' && (de->d_name[1] == '\0' ||
		    (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;
		n = de->d_name;
		if (lstat(n, &st) < 0) {
			fprintf(stderr, "buildwstdb: lstat(%s): %s\n",
			    de->d_name, strerror(errno));
			free(wep);
			return (-1);
		}
		if (S_ISREG(st.st_mode) != 0)
			wtype = WS_ET_FILE;
		else if (S_ISDIR(st.st_mode) != 0)
			wtype = WS_ET_DIR;
		else if (S_ISLNK(st.st_mode) != 0)
			wtype = WS_ET_SYMLINK;
		else {
			fprintf(stderr, 
			    "buildwstdb: %s: %o type unsupported\n",
			    de->d_name, (unsigned int)st.st_mode);
			free(wep);
			continue;
		}
start_build:
		/*
		 * Resolve symbolic links, if required to, and deal
		 * with the actual linked target.
		 */
		if (wtype == WS_ET_SYMLINK) {
    			if (wtp->wt_flags & WS_WF_NOSYMLINK)
				continue;
			/* Follow symbolic links */
			DPRINT(3, ("following symlink: "));
			if ((blen = readlink(n, symlinkname,
			    sizeof(symlinkname) - 1)) < 0) {
				warn("readlink(%s): Ignoring symlink", n);
				continue;
			}
			symlinkname[blen] = 0;
			if (stat(symlinkname, &st) < 0) {
				warn("stat(%s): Ignoring symlink", symlinkname);
				continue;
			}
			DPRINT(3, ("%s -> %s\n", n, symlinkname));
			if (S_ISREG(st.st_mode) != 0)
				wtype = WS_ET_FILE;
			else if (S_ISDIR(st.st_mode) != 0)
				wtype = WS_ET_DIR;
			else {
				fprintf(stderr, 
				    "buildwstdb: %s: %o type unsupported "
				    "while following symlink\n", n, (unsigned int)st.st_mode);
				free(wep);
				continue;
			}
			n = symlinkname;
			goto start_build;
		} if (wtype == WS_ET_FILE) {
			wep = xmalloc(sizeof(struct wsync_entry));
			memset(wep, 0, sizeof(struct wsync_entry));
			wep->we_mtime = utc_time(st.st_mtime);
			wep->we_type = wtype;
			wep->we_stat = WS_ES_INIT;
			if (strlcpy(wep->we_name, n, 
			    sizeof(wep->we_name) - 1) >= sizeof(wep->we_name)) {
				fprintf(stderr, 
				    "buildwstdb: buffer overrun: %s\n", n);
				free(wep);
				return (-1);
			}
			if (TAILQ_FIRST(&wtp->wt_fs_files) == NULL)
				TAILQ_INSERT_HEAD(&wtp->wt_fs_files, 
				    wep, we_link);
			else
				TAILQ_INSERT_TAIL(&wtp->wt_fs_files, 
				    wep, we_link);
			++wtp->wt_ndents;
		} else if (wtype == WS_ET_DIR) {
			if (chdir(n) < 0) {
				fprintf(stderr, "buildwstdb: chdir(%s): %s\n",
				    n, strerror(errno));
				return (-1);
			}
			swtp = xmalloc(sizeof(struct wsync_target));
			memset(swtp, 0, sizeof(struct wsync_target));
			if (snprintf(swtp->wt_name, sizeof(swtp->wt_name),
			    "%s/%s", wtp->wt_name, n) >= 
			    sizeof(swtp->wt_name)) {
				fprintf(stderr, 
				    "buildwstdb: buffer overrun: %s/%s\n",
				    wtp->wt_name, n);
				free(swtp);
				return (-1);
			}
			
			/*
			 * Inheritable configuration values.
			 */
			WST_INIT(swtp);
			if (inherit(wtp, swtp) < 0) {
				fprintf(stderr, "%s: Ignoring subdirectory\n",
				    swtp->wt_name);
				free(swtp);
				continue;
			}
			
			build_disk_entries(swtp);
			(void)chdir("..");
			
			if (TAILQ_FIRST(&wtp->wt_fs_dirs) == NULL)
				TAILQ_INSERT_HEAD(&wtp->wt_fs_dirs, swtp,
				    wt_link);
			else
				TAILQ_INSERT_TAIL(&wtp->wt_fs_dirs, swtp,
				    wt_link);
		}
	}
	return (0);
}

static void
dumpwsttree(wtp)
	struct	wsync_target *wtp;
{
	register struct wsync_entry *wep;
	register struct wsync_target *swtp;

	DPRINT(2, ("Examining %s\n", wtp->wt_name));
	TAILQ_FOREACH(wep, &wtp->wt_fs_files, we_link) {
		DPRINT(2, ("entry=%s, type=%d, mtime=%s", 
		    wep->we_name, wep->we_type, 
		    ctime(&wep->we_mtime)));
	}
	DPRINT(2, ("%d total\n", wtp->wt_ndents));
	TAILQ_FOREACH(swtp, &wtp->wt_fs_dirs, wt_link)
		dumpwsttree(swtp);
	return;
}

/*
 * Read list of files from configuration file along with
 * their last known state, modification time.
 * This will be examined by wsync() to decide the action 
 * that should be taken on disk files.
 * This list can be empty (e.g., initial wsync'ing).
 */
static int 
build_conf_entries(wtp)
	struct	wsync_target *wtp;
{
	int errn;
	size_t clen;
	register char *line, *p, *e;
	register struct wsync_entry *wep;
	FILE *f;
	
	DPRINT(3, ("build_conf_entries: load (@)entries from %s\n",
	    wtp->wt_conf_name));
	if ((f = open_conf(wtp, "entries")) == NULL)
		return (-1);
	while ((line = fparseln(f, NULL, NULL, NULL, 0)) != NULL) {
		p = line;
		while (*p != '\0' && (*p == ' ' || *p == '\t'))
			++p;
		if (p != '\0') {
			if (strlen(p) > 3 &&
			    *(p + 0) == '(' && *(p + 1) == '@' &&
			    *(p + 2) == ')') {
				free(line);
				break;
			}
			wep = xmalloc(sizeof(struct wsync_entry));
			memset(wep, 0, sizeof(struct wsync_entry));
			/*
			 * Type.
			 */
			switch (*p) {
			case 'f':
				wep->we_type = WS_ET_FILE;
				break;
			case 'd':
				wep->we_type = WS_ET_DIR;
				break;
			case 's':
				wep->we_type = WS_ET_SYMLINK;
				break;
			default:
				goto bad;
			}
			DPRINT(3, ("type=%d, ", wep->we_type));
			++p;
			if (*p != '/')
				goto bad;
			/*
			 * Name.
			 */
			e = ++p;
			while (*e != '\0' && *e != '/')
				++e;
			if (*e == '\0')
				goto bad;
			if (e - p >= sizeof(wep->we_name))
				clen = sizeof(wep->we_name) - 1;
			else
				clen = e - p + 1;
			strlcpy(wep->we_name, p, clen);
			DPRINT(3, ("name=%s, ", wep->we_name));
			p = e;
			if (*p != '/')
				goto bad;
			++p;
			/*
			 * Timestamp.
			 */
			e = p;
			errn = 0;
			while (*e != '\0' && *e != '/') {
				if (isdigit((int)*e) == 0) {
					errn = -1;
					break;
				}
				++e;
			}
			if (*p == '\0' || errn < 0)
				goto bad;
			wep->we_mtime = atoi(p);
			/* 
			 * We don't change to UTC time since 
			 * it's saved UTC 
			 */
			wep->we_mtime = wep->we_mtime;
			DPRINT(3, ("mtime=%d, ", (int)wep->we_mtime));
			p = e;
			if (*p != '/')
				goto bad;
			++p;
			/*
			 * Status.
			 */
			switch (*p) {
			case 's':
				wep->we_stat = WS_ES_SYNC;
				break;
			case 'i':
				wep->we_stat = WS_ES_INIT;
				break;
			case 'c':
				wep->we_stat = WS_ES_CHNG;
				break;
			default:
				goto bad;
			}
			DPRINT(3, ("status=%d\n", wep->we_type));
			if (TAILQ_FIRST(&wtp->wt_conf_files)== NULL)
				TAILQ_INSERT_HEAD(&wtp->wt_conf_files, wep,
				    we_link);
			else
				TAILQ_INSERT_TAIL(&wtp->wt_conf_files, wep,
				    we_link);
			goto cont;
bad:
			free(wep);
			return (-1);
cont:
			free(line);
		}
	}
	return (0);
}

/*
 * XXX Do better free()ing.
 */
static void 
trelease(wtp)
	struct	wsync_target *wtp;
{

	if (wtp->wt_fs != NULL)
		free(wtp->wt_fs);
	free(wtp);
	return;
}

/*
 * Try to load destination information from:
 * - Reference file (-d <file> command line option)
 * - From configuration file (typically ./.wsync.conf, or
 *   -f <file> command line option).
 * - Let the user edit it manually (via editor).
 *
 * Will fail if the above three routes fails and the program 
 * should abandon running.
 * 
 * Layout of destination section:
 * 
 * (@)destination
 * service ftp|local
 * host ftp.somewhere.com
 * port 21
 * proxy 192.168.2.1:21
 * proxy-port 21
 * user user_name
 * password xxxx
 * root /
 * file /path/to/file_that_contain_destination_info
 *
 * NOTE: key/pair values must exist. "none" or "0" should be used
 * approperiately when key/value pair should be ignored.
 */
static int 
getdest(wtp, dfile, norecurse)
	struct	wsync_target *wtp;
	const	char *dfile;
	int	norecurse;
{
	int ret;
	char *line, *p, *e, *oldc;
	FILE *f;
	struct destination dest, *d;
	char keyword[64];
	char tempname[PATH_MAX];
	
	DPRINT(3, ("getdest: %s\n", wtp->wt_conf_name));
	d = &dest;
	memset(d, 0, sizeof(struct destination));
	if (dfile != NULL) {
		DPRINT(2, ("using destination reference file %s\n", dfile));
		oldc = wtp->wt_conf_name;
		wtp->wt_conf_name = (char *)dfile;
		if ((f = open_conf(wtp, "destination")) == NULL) {
			ret = -1;
		}
		wtp->wt_conf_name = oldc;
		if (ret < 0)
			return (ret);
	} else if ((f = open_conf(wtp, "destination")) == NULL) {
		printf("\nNo configuration file or no (@)destination\n"
		    "Enter manually\n\n");
		(void)sleep(1);
		if (installsection("destination", tempname, 
		    sizeof(tempname) - 1) < 0) {
			fprintf(stderr, "canceled.  aborting.\n");
			return (-1);
		}
		/* Read the temporary file. */
		ret = 0;
		oldc = wtp->wt_conf_name;
		wtp->wt_conf_name = tempname;
		if ((f = open_conf(wtp, "destination")) == NULL) {
			ret = -1;
		}
		wtp->wt_conf_name = oldc;
		if (ret < 0)
			return (ret);
	}

	/*
	 * Do the actual parsing.
	 */
	while ((line = fparseln(f, NULL, NULL, NULL, 0)) != NULL) {
		p = line;
		while (*p != '\0' && (*p == ' ' || *p == '\t'))
			++p;
		if (*p != '\0') {
			if (strlen(p) > 3 &&
			    *(p + 0) == '(' && *(p + 1) == '@' &&
			    *(p + 2) == ')') {
				free(line);
				break;
			}
			e = keyword;
			while (*p != '\0' && *p != ' ' && *p != '\t' &&
			    e - keyword < sizeof(keyword) - 1)
				*e++ = *p++;
			if (*p == '\0') {
				fprintf(stderr, "invalid key/value pair: %s\n",
				    line);
				free(line);
				return (-1);
			}
			if (e - keyword >= sizeof(keyword) - 1) {
				fprintf(stderr, "keyword too long: %s\n",
				    line);
				free(line);
				return (-1);
			}
			*e = '\0';
			DPRINT(3, ("keyword %s\n", keyword));
			while (*p != '\0' && (*p == ' ' || *p == '\t'))
				++p;
			if (*p == '\0') {
				fprintf(stderr, "no value: %s\n",
				    line);
				free(line);
				return (-1);
			}
			e = p + strlen(p) - 1;
			if (*e == ' ' || *e == '\t') {
				while (*e == ' ' || *e == '\t')
					--e;
				*++e = '\0';
			}
			DPRINT(3, ("value %s\n", p));
			if (strcmp(keyword, "service") == 0) {
				if (strcmp(p, "ftp") == 0) {
					d->d_type = WS_DEST_FTP;
				} else if (strcmp(p, "local") == 0) {
					d->d_type = WS_DEST_LOCAL;
				} else {
					fprintf(stderr, 
					    "service %s invalid\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "host") == 0) {
				if (strlcpy(d->d_host, p, 
				    sizeof(d->d_host) - 1) >= 
				    sizeof(d->d_host)) {
					fprintf(stderr, "host name too long %s"
					    "\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "port") == 0) {
				d->d_port = (u_short)atoi(p);
			} else if (strcmp(keyword, "proxy") == 0) {
				if (strlcpy(d->d_proxy, p, 
				    sizeof(d->d_proxy) - 1) >= 
				    sizeof(d->d_proxy)) {
					fprintf(stderr, "proxy name too long %s"
					    "\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "proxy-port") == 0) {
				d->d_proxyport = (u_short)atoi(p);
			} else if (strcmp(keyword, "user") == 0) {
				if (strlcpy(d->d_user, p, 
				    sizeof(d->d_user) - 1) >= 
				    sizeof(d->d_user)) {
					fprintf(stderr, "user name too long %s"
					    "\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "password") == 0) {
				if (strlcpy(d->d_passwd, p, 
				    sizeof(d->d_passwd) - 1) >= 
				    sizeof(d->d_passwd)) {
					fprintf(stderr, "password too long %s"
					    "\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "root") == 0) {
				if (strlcpy(d->d_root, p, 
				    sizeof(d->d_root) - 1) >= 
				    sizeof(d->d_root)) {
					fprintf(stderr, "root path too long %s"
					    "\n", p);
					free(line);
					return (-1);
				}
			} else if (strcmp(keyword, "file") == 0) {
				if (strlcpy(d->d_file, p, 
				    sizeof(d->d_file) - 1) >= 
				    sizeof(d->d_file)) {
					fprintf(stderr, "reference file name"
					    "too long %s\n", p);
					free(line);
					return (-1);
				}
				if (norecurse == 0) {
					DPRINT(2, 
					    ("loading reference file %s\n",
					    d->d_file));
					oldc = wtp->wt_conf_name;
					wtp->wt_conf_name = d->d_file;
					ret = getdest(wtp, NULL, 1);
					wtp->wt_conf_name = oldc;
					return (ret);
				}
			} else if (strcmp(keyword, "connection") == 0) {
				if (strcmp(p, "passive") == 0)
					d->d_flags |= WS_DEST_FTP_PASSIVE;
				else if (strcmp(p, "active") == 0)
					d->d_flags |= WS_DEST_FTP_ACTIVE;
				else if (d->d_type == WS_DEST_FTP)
					d->d_flags |= WS_DEST_FTP_ACTIVE;
				else
					d->d_flags &= 
					    ~(WS_DEST_FTP_ACTIVE|WS_DEST_FTP_PASSIVE);
			} else {
				fprintf(stderr, "%s: unknown keyword\n", 
				    keyword);
				free(line);
				return (-1);
			}
		}
		free(line);
	}
	d = xmalloc(sizeof(struct destination));
	bcopy(&dest, d, sizeof(struct destination));
	wtp->wt_dst = d;
	wtp->wt_fs = xmalloc(sizeof(struct fs_desc));
	wtp->wt_fs->fsd_data = NULL;
	wtp->wt_fs->fsd_wstp = NULL;
	switch (wtp->wt_dst->d_type) {
	case WS_DEST_FTP:
		wtp->wt_fs->fsd_fsops = &ftpops;
		break;
	case WS_DEST_LOCAL:
		wtp->wt_fs->fsd_fsops = &localops;
		break;
	}
	return (0);
}

/*
 * Open configuration file (in wsync_target structure) and seek
 * to the specified section `sec'.
 *
 * Return file pointer on successfule open/seek or NULL otherwise.
 */
static FILE *
open_conf(wtp, sec)
	struct	wsync_target *wtp;
	const	char *sec;
{
	char *line, *p;
	FILE *f;

	if ((f = fopen(wtp->wt_conf_name, "r")) != NULL) {
		if (sec == NULL)
			return (f);
		while ((line = fparseln(f, NULL, NULL, NULL, 0)) != NULL) {
			p = line;
			while (*p != '\0' && (*p == ' ' || *p == '\t'))
				++p;
			if (strlen(p) > 3 && 
			    *(p + 0) == '(' && *(p + 1) == '@' &&
			    *(p + 2) == ')') {
				p += 3;
				if (strcmp(p, sec) == 0) {
					free(line);
					return (f);
				}
			}
			free(line);
		}
		fclose(f);
		f = NULL;
	}
	return (f);
}

/*
 * Handy macros used by wsync().
 */

#define CFWE_FIND(w0,wl,wt) do { \
	TAILQ_FOREACH(wt, wl, we_link) { \
		if (strcmp(w0->we_name, wt->we_name) == 0) \
			break; \
	} \
} while (0)

#define WSSTAT(s)	(s == WS_ES_INIT ? "initial" : \
		s == WS_ES_CHNG ? "changed" : s == WS_ES_SYNC ? "sync" : "N/A")

int
wsync(wtp)
	struct	wsync_target *wtp;
{
	char *p;
	register char *n;
	register struct wsync_entry *fswep, *cfwep;
	register struct wsync_target *swtp;
	char fpath[PATH_MAX];

	DPRINT(2, ("==> sync'ing %s:\n", wtp->wt_name));
	if (wtp->wt_dst->d_type == WS_DEST_FTP) {
		char *r;

		r = wtp->wt_dst->d_root;
		printf("checking (ftp): %s --> ftp://%s%s%s/%s\n", 
		    wtp->wt_name, wtp->wt_dst->d_host,
		    r[0] == '/' && r[1] == '\0' ? "" : "/",
		    r[0] == '/' && r[1] == '\0' ? "" : r,
		    wtp->wt_name);
	} else {
		printf("checking (local): %s --> %s/%s\n", 
		    wtp->wt_name, wtp->wt_dst->d_root, wtp->wt_name);
	}
	(void)build_conf_entries(wtp);
	(void)build_exclude_list(wtp, 0); /* XXX necessary? makes dups. */
	if (gordir(wtp) < 0) {
		fprintf(stderr, "switching to %s failed\n", wtp->wt_name);
		return (-1);
	}
	TAILQ_FOREACH(fswep, &wtp->wt_fs_files, we_link) {
		DPRINT(3, ("entry: fs(%s)=%s, ", fswep->we_name, 
		    WSSTAT(fswep->we_stat)));
		CFWE_FIND(fswep, &wtp->wt_conf_files, cfwep);
		/*
		 * Mark entry as exist in filesystem and in configuration
		 * file.  Entries not marked as dirty are considered deleted
		 * from local filesystem and are subject to deletion (-P 
		 * option is used).
		 */
		if (cfwep != NULL)
			++cfwep->we_dirty;
		if (excluded(wtp, fswep->we_name) != 0) {
			continue;
		}
		if (cfwep == NULL) {
			DPRINT(3, ("cf(%s)=NULL\n", "none"));
			fswep->we_stat = WS_ES_INIT;
		} else if (cfwep->we_stat == WS_ES_CHNG) {
			DPRINT(3, ("cf(%s)=%s\n", cfwep->we_name, 
			    WSSTAT(cfwep->we_stat)));
			fswep->we_stat = WS_ES_CHNG;
		} else if (cfwep->we_stat == WS_ES_SYNC) {
			DPRINT(3, ("cf(%s)=%s, mtime ", cfwep->we_name,
			    WSSTAT(cfwep->we_stat)));
			if (cfwep->we_mtime != fswep->we_mtime) {
				DPRINT(3, ("not-sync\n"));
				fswep->we_stat = WS_ES_CHNG;
			} else {
				DPRINT(3, ("sync\n"));
				fswep->we_stat = WS_ES_SYNC;
			}
			DPRINT(3, ("cf=%d, fs=%d\n", (int)cfwep->we_mtime,
			    (int)fswep->we_mtime));
		} else {
			/*
			 * Will be in `initial' state and will leave it
			 * this way to get uploaded.
			 */
		}
		n = fswep->we_name;
		/*
		 * Now we do the upload/copy decision based on sync/mtime
		 * checks above.
		 */
		if (fswep->we_stat == WS_ES_INIT || 
		    fswep->we_stat == WS_ES_CHNG) {
			DPRINT(3, ("uploading %s:%s\n", wtp->wt_name, n));
			if (RUNNABLE(wtp) != 0) {
				printf("\t%s/%s <- ", wtp->wt_name, n);
				if (settpath(fpath, sizeof(fpath), 
				    wtp->wt_dst->d_root,
				    wtp->wt_name, n) < 0) {
					warnx("path too long!");
					return (-1);
				}
				if ((*wtp->wt_fops->fso_put)(n, fpath,
				    wtp->wt_fs) == 0) {
					fswep->we_stat = WS_ES_SYNC;
					if (cfwep != NULL)
						cfwep->we_stat = WS_ES_SYNC;
					printf("synchronized\n");
				} else {
					fswep->we_stat = WS_ES_CHNG;
					printf("failed!\n");
				}
			} else {
				DPRINT(3, ("(run_only): "));
				printf("\t%s/%s\n", wtp->wt_name, n);
			}
		}
	}

	/*
	 * Remotley delete files that was deleted from local system
	 * but still exist in configuration file.
	 */
	if (RUNNABLE(wtp) != 0 && ((wtp->wt_flags & WS_WF_PRUNE) != 0)) {
		DPRINT(2, ("checking files to prune\n"));
		TAILQ_FOREACH(cfwep, &wtp->wt_conf_files, we_link) {
			if (cfwep->we_dirty == 0) {
				DPRINT(3, ("prune: removing %s\n", 
				    cfwep->we_name));
				/* TODO */
			}
		}
	}

	/* Preform "cd $ROOT" remotely */
	(void)goroot(wtp);
	
	/* If we are not recursive, stop here. */
	if (wtp->wt_flags & WS_WF_NORECURSE) {
		DPRINT(2, ("Stop.\n"));
		return (0);
	}

	/*
	 * Now wsync subdirectories.  Callers of wsync must prepare
	 * local directories before calling it (i.e., chnage current
	 * directory.)
	 */
	TAILQ_FOREACH(swtp, &wtp->wt_fs_dirs, wt_link) {
		DPRINT(2, ("sync'ing subdir: %s\n", swtp->wt_name));
		p = dirname(swtp->wt_name);
		DPRINT(3, ("will change local directory to %s\n", p));
		if (chdir(p) < 0) {
			warn("wsync(): chdir(%s)", p);
			continue;
		}
		printf("\n");
		wsync(swtp);
		(void)chdir("..");
	}
	return (0);
}

/*
 * Write configuration files (./.wsync.conf) recursively
 * to current directories and all subdirectories.
 */
static int
write_conf(wtp)
	struct	wsync_target *wtp;
{
	int fd;
	char *p;
	FILE *f;
	register struct destination *d;
	register struct wsync_entry *wep;
	register struct wsync_target *swtp;
	register struct exclude_entry *ee;
	char buf[BUFSIZ << 2];

	if ((f = fopen(wtp->wt_conf_name, "w")) == NULL) {
		warn("write_conf: fopen(%s/%s)", 
		    wtp->wt_name, wtp->wt_conf_name);
		return (-1);
	}
	DPRINT(2, ("write_conf: config: %s:%s\n", 
	    wtp->wt_name, wtp->wt_conf_name));
	fd = fileno(f);
	(void)fchown(fd, getuid(), getuid());
	if (fchmod(fd, S_IRUSR|S_IWUSR) < 0) {
		warn("write_conf: fchmod(%s/%s)", wtp->wt_name,
		    wtp->wt_conf_name);
		fclose(f);
		return (-1);
	}
	
#define WRITE(f,buf,blen) do { \
	if (fwrite(buf, blen, 1, f) != 1)  { \
		warn("write_conf: fwrite(%s/%s)", wtp->wt_name, \
		    wtp->wt_conf_name); \
		fclose(f); \
		return (-1); \
	} \
} while (0)

	/*
	 * Version.
	 */
	snprintf(buf, sizeof(buf), "(@)version %s\n\n", VERSION);
	WRITE(f, buf, strlen(buf));

	/*
	 * Destination.
	 */
	d = wtp->wt_dst;
	snprintf(buf, sizeof(buf), "(@)destination\n");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "service %s\n", 
	    d->d_type == WS_DEST_FTP ? "ftp" : d->d_type == WS_DEST_LOCAL ?
	    "local" : "ERROR");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "connection %s\n", 
	    d->d_flags & WS_DEST_FTP_ACTIVE ? "active" :
	    d->d_flags & WS_DEST_FTP_PASSIVE ? "passive" : "none");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "host %s\n", 
	    d->d_host[0] != '\0' ? d->d_host : "none");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "port %hd\n", d->d_port);
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "proxy %s\n", 
	    noproxy(d->d_proxy) == 0 ? d->d_proxy : "none");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "proxy-port %hd\n", d->d_proxyport);
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "user %s\n",
	    d->d_user[0] != '\0' ? d->d_user : "none");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "password %s\n",
	    d->d_passwd[0] != '\0' ? d->d_passwd : "none");
	WRITE(f, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "root %s\n",
	    d->d_root[0] != '\0' ? d->d_root : "/");
	WRITE(f, buf, strlen(buf));
	if (d->d_file[0] == '\0')
		WRITE(f, "#", 1);
	snprintf(buf, sizeof(buf), "file %s\n", 
	    d->d_file[0] != '\0' ? d->d_file : "none");
	WRITE(f, buf, strlen(buf));

	/*
	 * Entries.
	 */
	snprintf(buf, sizeof(buf), "\n(@)entries\n");
	WRITE(f, buf, strlen(buf));
	TAILQ_FOREACH(wep, &wtp->wt_fs_files, we_link) {
		if (wep->we_type == WS_ET_DIR)
			continue;
		snprintf(buf, sizeof(buf), "%c/%s/%d/%c\n",
		    wep->we_type == WS_ET_FILE ? 'f' : 
		    wep->we_type == WS_ET_SYMLINK ? 's' : 'e',
		    wep->we_name, wep->we_mtime, 
		    wep->we_stat == WS_ES_SYNC ? 's' :
		    wep->we_stat == WS_ES_INIT ? 'i' :
		    wep->we_stat == WS_ES_CHNG ? 'c' : 'e');
		WRITE(f, buf, strlen(buf));
	}

	/*
	 * Exclude list
	 */
	snprintf(buf, sizeof(buf), "\n(@)exclude\n");
	WRITE(f, buf, strlen(buf));
	TAILQ_FOREACH(ee, &wtp->wt_excs, ee_link) {
		if (strcmp(ee->ee_name, WSYNC_CONF) == 0 ||
		    strcmp(ee->ee_name, wtp->wt_conf_name) == 0)
			continue;
		snprintf(buf, sizeof(buf), "%s\n", ee->ee_name);
		WRITE(f, buf, strlen(buf));
	}
	fclose(f);
	
	/*
	 * Write configuration to subdirectories.
	 */
	TAILQ_FOREACH(swtp, &wtp->wt_fs_dirs, wt_link) {
		p = dirname(swtp->wt_name);
		DPRINT(2, ("should write subdirectory %s\n", p));
		if (chdir(p) < 0) {
			warn("write_conf(): chdir(%s)", p);
			continue;
		}
		write_conf(swtp);
		(void)chdir("..");
	}
	return (0);
}

static int
build_exclude_list(wtp, skip)
	struct	wsync_target *wtp;
	int	skip;
{
	char *line, *p;
	FILE *f;
	struct exclude_entry *ee;

	/*
	 * Default excludes
	 */
	ee = newexc(WSYNC_CONF);
	TAILQ_INSERT_HEAD(&wtp->wt_excs, ee, ee_link);
	if (skip == 0 && strcmp(WSYNC_CONF, wtp->wt_conf_name) != 0) {
		ee = newexc(wtp->wt_conf_name);
		TAILQ_INSERT_TAIL(&wtp->wt_excs, ee, ee_link);
	}
	if ((f = open_conf(wtp, "exclude")) != NULL) {
		while ((line = fparseln(f, NULL, NULL, NULL, 0)) != NULL) {
			p = line;
			while ((*p == ' ' || *p == '\t') && *p != '\0')
				++p;
			if (*p == '\0') {
				free(line);
				continue;
			}
			ee = newexc(p);
			TAILQ_INSERT_TAIL(&wtp->wt_excs, ee, ee_link);
			free(line);
		}
	}
	return (0);
}

static int
excluded(wtp, name)
	struct	wsync_target *wtp;
	char	*name;
{
	register char *p0, *p1;
	register struct exclude_entry *ee;

	TAILQ_FOREACH(ee, &wtp->wt_excs, ee_link) {
		p0 = ee->ee_name;
		p1 = name;
		if (ee->ee_name[0] == '*') {
			++p0;
			p1 += strlen(p1) - strlen(p0);
		}
		if (*p0 == '\0' || *p1 == '\0')
			continue;
		if (strcmp(p0, p1) == 0) {
			DPRINT(3, ("- %s: excluded (rule: %s)\n", 
			    name, ee->ee_name));
			return (1);
		}
	}
	return (0);
}

static struct exclude_entry *
newexc(name)
	char	*name;
{
	struct exclude_entry *ee;

	ee = xmalloc(sizeof(struct exclude_entry));
	strlcpy(ee->ee_name, name, sizeof(ee->ee_name) - 1);
	return (ee);
}

static int
gordir(wtp)
	struct	wsync_target *wtp;
{
	register char *n, *r;
	char fpath[PATH_MAX];
	
	if (RUNNABLE(wtp) == 0)
		return (0);
	n = wtp->wt_name;
	if (wtp->wt_dst->d_type == WS_DEST_FTP) {
		DPRINT(3, ("gordir: remote directory to %s\n", n));
		if ((*wtp->wt_fops->fso_chdir)(n, wtp->wt_fs) < 0) {
			if ((*wtp->wt_fops->fso_mkdir)(n, wtp->wt_fs) < 0)
				return (-1);
			if ((*wtp->wt_fops->fso_chdir)(n, wtp->wt_fs) < 0)
				return (-1);
		}
	} else {
		/* Local */
		r = wtp->wt_dst->d_root;
		snprintf(fpath, sizeof(fpath), "%s%s%s",
		    r, r[0] == '/' && r[1] == '\0' ? "" : "/", n);
		/*if (*/(*wtp->wt_fops->fso_mkdir)(fpath, wtp->wt_fs);/* < 0) {
			warn("gordir: mkdir(%s)", fpath);
			return (-1);
		}*/
	}
	return (0);
}

static int
goroot(wtp)
	struct	wsync_target *wtp;
{

	if (RUNNABLE(wtp) == 0)
		return (0);
	if (wtp->wt_dst->d_type == WS_DEST_FTP) {
		return ((*wtp->wt_fops->fso_chdir)(wtp->wt_dst->d_root, 
		    wtp->wt_fs));
	}
	return (0);
}

static int
inherit(wtp, swtp)
	register	struct wsync_target *wtp;
	register	struct wsync_target *swtp;
{
	int clen;
	struct exclude_entry *ee0, *ee1;

	/*
	 * We already have our wt_name filled up by caller, and it's
	 * relative to parent directory.  For example if we started
	 * wsync'ing from /home/user/dir then:
	 *
	 * wsync_target of /home/user/dir will have its wt_name = "dir"
	 *
	 * A subdirectory of /home/user/dir called subdir will have its
	 * wsync_target's wt_name = "dir/subdir"
	 *
	 * So first we create our wsync root database subdirectory if it
	 * doesn't exist and then we construct name of configuration file 
	 * under that directory.
	 */
	
	clen = strlen(wsync_root) + strlen(swtp->wt_name) + 
	    strlen(WSYNC_CONF) + 4;
	if (clen >= PATH_MAX) {
		fprintf(stderr, "inherit: %s wsync root path too long\n",
		    swtp->wt_name);
		return (-1);
	}
	swtp->wt_conf_name = xmalloc(clen);
	/* Create subdirectory under $WSYNC_ROOT */
	snprintf(swtp->wt_conf_name, clen, "%s/%s", wsync_root, swtp->wt_name);
	if (mkdir(swtp->wt_conf_name, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "inherit: %s: %s\n", swtp->wt_conf_name,
		    strerror(errno));
		return (-1);
	}
	snprintf(swtp->wt_conf_name, clen, "%s/%s/%s", wsync_root, 
	    swtp->wt_name, WSYNC_CONF);
	
	swtp->wt_dst = wtp->wt_dst;
	swtp->wt_fs = wtp->wt_fs;
	swtp->wt_flags = wtp->wt_flags;
	swtp->wt_dst = wtp->wt_dst;
	TAILQ_FOREACH(ee0, &wtp->wt_excs, ee_link) {
		ee1 = xmalloc(sizeof(struct exclude_entry));
		bcopy(ee0, ee1, sizeof(struct exclude_entry));
		if (TAILQ_FIRST(&swtp->wt_excs) == NULL)
			TAILQ_INSERT_HEAD(&swtp->wt_excs, ee1, ee_link);
		else
			TAILQ_INSERT_TAIL(&swtp->wt_excs, ee1, ee_link);
	}
	return (0);
}

static char *
dirname(path)
	const	char *path;
{
	register const char *p;
	
	p = path + strlen(path);
	while (p > path && *p != '/')
		--p;
	if (*p == '/')
		++p;
	return ((char *)p);
}

static int
settpath(buf, buflen, root, path, name)
	char	*buf;
	int	buflen; 
	char	*root;
	char	*path;
	char	*name;
{
	char *r;

	r = root;
	if (snprintf(buf, buflen, "%s%s%s/%s", 
	    r, r[0] == '/' && r[1] == '\0' ? "" : "/", path, name) >= buflen)
		return (-1);
	return (0);
}
					
time_t
utc_time(ltime)
	time_t	ltime;
{
	time_t rtime;
	struct tm *tm;

	tm = gmtime(&ltime);
	rtime = mktime(tm);
	return (rtime);
}

static int 
wsyncroot()
{
	struct stat sb;

	if (wsync_root == NULL) {
		if ((wsync_root = getenv("WSYNC_ROOT")) == NULL) {
			fprintf(stderr, "No $WSYNC_ROOT defined and "
			    "-R /path/to/root is not used.  Quitting!\n");
			exit(EXIT_FAILURE);
		}
	}

	if (stat(wsync_root, &sb) < 0) {
		printf("Building wsync root database directory @ %s\n",
		    wsync_root);
		/*
		 * No one but me should have 'rwx' to root directory.
		 */
		if (mkdir(wsync_root, 0700) < 0) {
			fprintf(stderr, "wsyncroot: mkdir(%s): %s\n",
			    wsync_root, strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		/*
		 * Already exists, check permissions and type.
		 */
		if (S_ISDIR(sb.st_mode) == 0) {
			fprintf(stderr, "wsyncroot: %s: is not a directory\n",
			    wsync_root);
			exit(EXIT_FAILURE);
		}
		if (sb.st_uid != getuid()) {
			fprintf(stderr, "wsyncroot: %s is not owned by you.  "
			    "Go away!\n", wsync_root);
			exit(EXIT_FAILURE);
		}
		if ((sb.st_mode & ALLPERMS) != 0700) {
			fprintf(stderr, "wsyncroot: %s has wrong permissions "
			    "%04o, should be %04o\n", wsync_root, 
			    (unsigned int)(sb.st_mode & ALLPERMS), 0700);
			exit(EXIT_FAILURE);
		}
	}
	return (0);
}

static __dead void
usage()
{

	fprintf(stderr, "%s: usage: %s <OPTIONS> [directory]\n", 
	    __progname, __progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-D <f>\tuse destination reference file\n");
	fprintf(stderr, "\t-R <p>\tuse <p> a path to wsync root database\n");
	fprintf(stderr, "\t-N\tsuppress recursive behavior\n");
	fprintf(stderr, "\t-S\tdon't follow symbolic links\n");
	fprintf(stderr, "\t-P\tremotely prune locally deleted files\n");
	fprintf(stderr, "\t-C\tjust print what files are changed\n");
	fprintf(stderr, "\t-d\tdebug\n");
	fprintf(stderr, "\t-v\tversion\n");
	fprintf(stderr, "\t-h\thelp\n");
	exit(EXIT_SUCCESS);
}

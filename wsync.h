/* $Id: wsync.h,v 1.17 2004/04/21 22:39:24 te Exp $ */ 

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

#if !defined (_WSYNC_H)
# define _WSYNC_H

#if defined (_WINDOWS)
# include <bsd_list.h>
# include <utypes.h>
#elif defined (__OpenBSD__) || defined (__NetBSD__) || \
	defined (__FreeBSD__)
# include <sys/queue.h>
# include <sys/types.h>
# include <sys/param.h>		/* MAXHOSTNAMELEN, MAXLOGNAME */
#elif defined (sun)
# include <bsd_list.h>
# include <limits.h>		/* MAX_PATH */
# include <netdb.h>		/* MAXHOSTNAMELEN */
#endif

#define WSYNC_CONF	"wsync.conf"
#if defined (DEBUG)
# define DPRINT(l,m)	if ((l) <= wsync_debug) \
				printf m; fflush(stdout)
#else
# define DPRINT(l,m)
#endif

#if defined (sun)
# include <strings.h>
# define ALLPERMS	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)
# define NAME_MAX	_POSIX_NAME_MAX
# define MAXLOGNAME	LOGNAME_MAX
# define _PATH_VI	"/bin/vi"
# define _PATH_TMP	"/tmp/"
#endif	/* sun */

struct destination {
	u_char	d_type;
#define WS_DEST_FTP		1
#define WS_DEST_LOCAL		2
	u_int	d_flags;
#define WS_DEST_FTP_PASSIVE	0x0001
#define WS_DEST_FTP_ACTIVE	0x0002
	char	d_file[PATH_MAX];	/* File name to take `dst' info from */
	char	d_root[PATH_MAX];	/* Root to create sync'd tree under */
	/* Applies to FTP */
	char	d_host[MAXHOSTNAMELEN];
	u_short	d_port;
	char	d_proxy[MAXHOSTNAMELEN];
	u_short	d_proxyport;
	char	d_user[MAXLOGNAME];
	char	d_passwd[16];
};

/* f/BUGS/mod_time/s */
struct wsync_entry {
	TAILQ_ENTRY(wsync_entry)
		we_link;
	u_char	we_type;
#define WS_ET_FILE		1
#define WS_ET_DIR		2
#define WS_ET_SYMLINK		3
	char	we_name[NAME_MAX];
	time_t	we_mtime;
	u_char	we_stat;
#define WS_ES_INIT		1	/* File in initial state */
#define WS_ES_SYNC		2	/* Local copy and remote are in sync */
#define WS_ES_CHNG		3	/* Local copy changed but no sync was possible */
#define WS_ES_DELE		4	/* Deleted from system, but exist in conf */
	int	we_dirty;		/* Special for conf_list */
};
TAILQ_HEAD(wsync_entries_t, wsync_entry);

struct exclude_entry {
	TAILQ_ENTRY(exclude_entry)
		ee_link;
	char	ee_name[NAME_MAX];
};
TAILQ_HEAD(exclude_list_t, exclude_entry);

struct fsops {
	int	(*fso_fsreg)(void *);
	void	(*fso_fsunreg)(void *);
	int	(*fso_init)(void *);
	void	(*fso_deinit)(void *);
	int	(*fso_chdir)(const char *,void *);
	int	(*fso_mkdir)(const char *,void *);
	int	(*fso_put)(const char *,const char *,void *);
	int	(*fso_dele)(const char *,void *);
	time_t	(*fso_mtime)(const char *,void *);
	off_t	(*fso_size)(const char *,void *);
};

struct fs_desc {
	void	*fsd_data;		/* Filesystem specific data */
	void	*fsd_wstp;		/* wsync_target */
	struct	fsops *fsd_fsops;
};

struct wsync_target;
TAILQ_HEAD(wsync_target_list_t, wsync_target);

struct wsync_target {
	TAILQ_ENTRY(wsync_target)
		wt_link;
	char	wt_name[PATH_MAX];	/* Full/relative (to root) path name */
	char	*wt_conf_name;		/* Configuration file name */
	struct	destination *wt_dst;
	struct	wsync_entries_t wt_fs_files;
	struct	wsync_target_list_t wt_fs_dirs;
	struct	wsync_entries_t wt_conf_files;
	struct	exclude_list_t wt_excs;
	struct	fs_desc *wt_fs;		/* Remote filesystem operations */
#define wt_fops		wt_fs->fsd_fsops
#define wt_fsws		wt_fs->fsd_wstp
	u_char	wt_flags;
#define WS_WF_NOSYMLINK		0x01
#define WS_WF_NORECURSE		0x02
#define WS_WF_PRUNE		0x04
#define WS_WF_NORUN		0x08
	int	wt_ndents;
};

#define RUNNABLE(w)		(((w)->wt_flags & WS_WF_NORUN) == 0)

#define VERSION			"1.1.0-BETA"
#define AUTHER			"Tamer Embaby <tsemba@menanet.net>"

extern	int wsync_debug;
extern	struct fsops ftpops;
extern	struct fsops localops;

int	installsection(const char *,char *,size_t);
int	fs_reg(void);
int 	htmldir(char *,const char *);
void	fs_unreg(void);

#endif	/* !_WSYNC_H */

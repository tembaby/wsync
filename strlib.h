/* $Id: strlib.h,v 1.4 2002/11/19 03:39:06 te Exp $ */ 

/*
 * Copyright (c) 2002 Tamer Embaby <tsemba@menanet.net>
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

#if !defined (_STRLIB_H)
# define _STRLIB_H

struct kvp_t {
	char	*kvp_key;
	char	*kvp_value;
	char	**kvp_valv;
	char	**_kvp_splt;
};

int	strcont(register char,register char *);
char	**splitv(register char *,register int *,register char *);
char	*__strndup(const char *,int,const char *,int);
char	*__xstrdup(const char *,const char *,int);
void	*__emalloc(size_t,const char *,int);
void	*__xmalloc(size_t,const char *,int);
struct	kvp_t *kvparse(char *,char *,char *);
void	kvfree(struct kvp_t *);

#define strndup(s,n)	__strndup(s, n, __FILE__, __LINE__)
#define xstrdup(s)	__xstrdup(s, __FILE__, __LINE__)
#define emalloc(s)	__emalloc(s, __FILE__, __LINE__)
#define xmalloc(s)	__xmalloc(s, __FILE__, __LINE__)

#endif	/* _STRLIB_H */

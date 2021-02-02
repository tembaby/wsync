/* $Id: strlib.c,v 1.6 2002/11/19 03:53:58 te Exp $ */ 

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "strlib.h"

#if defined (_WIN32)
# define strdup		_strdup
#endif

/* 
 * TODO:
 * - netstring()
 */

#define STRLIB_DEBUG

/*
 * Return true if string `s' contains character `c'
 */
int
strcont(c, s)
	register	char c;
	register	char *s;
{

	while (*s) {
		if (*s == c)
			return (1);
		s++;
	}
	return (0);
}

/*
 * Split string `s' into array component based on: space, tab, or 
 * delimiter vector (if any).
 */
char **
splitv(s, nelm, delim)
	register	char *s;
	register	int *nelm;
	register	char *delim;	/* Delimiters list */
{
	int i, mem, slen, d;
	size_t msize;
	register char *p, *e;
	char **vec;

	d = mem = i = 0;
	p = s;
	vec = NULL;
	slen = strlen(s);
	if (nelm != NULL)
		*nelm = 0;

#ifdef STRLIB_DEBUG
	printf("splitv: parsing: %s\n", s);
#endif

	while (p < (s + slen)) {
		e = p;
		/* Igonre LWS */
		while ((*e == ' ' || *e == '\t') && *e != 0)
			e++;
		if (*e == 0)
			break;
		p = e;
		/* Tokenize */
		for (;;) {
			if (*e == '"') {
				++e;
				for (;;) {
					if (*e == '\0')
						break;
					if (*e == '\\') {
						++e;
						if (*e == '"')
							++e;
					}
					if (*e == '"')
						break;
					++e;
				}
			}
			/* EOS? */
			if (*e == '\0')
				break;
			/* LWS? */
			if (*e == ' ' || *e == '\t')
				break;
			/* Delimiter (if any?) */
			if (delim != NULL && (d = strcont(*e, delim)) != 0)
				break;
			++e;
		}
		if ((e - p) != 0) {
			msize = ((i + 1) + 1) * sizeof(char *);
			if (vec == NULL)
				vec = malloc(msize);
			else
				vec = realloc(vec, msize);
			if (vec == NULL) {
				fprintf(stderr, 
				    "splitv: out of dynamic memory\n");
				return (NULL);
			}
			mem = 1;
			vec[i] = strndup(p, e - p);
#ifdef STRLIB_DEBUG
			printf("splitv: vec[%d] = [%s]\n", i, vec[i]);
#endif
		}
		/* Did we stop due to delimiter? */
		if (d != 0) {
			/*
			 * Advance only if previous check
			 * did insert new element
			 */
			if ((e - p) != 0)
				i++;
			p = e++;
			d = 0;
			msize = ((i + 1) + 1) * sizeof(char *);
			/*
			 * And insert the delimiter in the returned vector.
			 */
			if (vec == NULL)
				vec = malloc(msize);
			else
				vec = realloc(vec, msize);
			if (vec == NULL) {
				fprintf(stderr, 
				    "splitv: out of dynamic memory\n");
				return (NULL);
			}
			vec[i] = strndup(p, e - p);
#ifdef STRLIB_DEBUG
			printf("splitv: vec[%d] = [%s] (delim)\n", i, vec[i]);
#endif
		}
		p = e;
		i++;
	}

	if (mem == 0)
		return (NULL);
	if (nelm != NULL)
		*nelm = i;
	vec[i] = NULL;
#ifdef STRLIB_DEBUG
	printf("splitv: vec[%d] = NULL\n", i);
#endif
	return (vec);
}

/*
 * Duplicate a string `s' of length `n'.
 */
char *
__strndup(s, n, fn, ln)
	const	char *s;
	int	n;
	const	char *fn;
	int	ln;
{
	char *d;

	if ((d = malloc(n + 1) ) == NULL) {
		fprintf(stderr, 
		    "strndup(%s:%d): out of dynamic memory\n", fn, ln);
		return (NULL);
	}
	strncpy(d, s, n);
	d[n] = 0;
	return (d);
}

char *
__xstrdup(s, fn, ln)
	const	char *s;
	const	char *fn;
	int	ln;
{
	char *p;

	if ((p = strdup(s)) == NULL) {
		fprintf(stderr, 
		    "xstrdup(%s:%d): out of dynamic memory\n", fn, ln);
		exit(EXIT_FAILURE);
	}
	return (p);
}

void *
__emalloc(siz, m, l)
	size_t	siz;
	const	char *m;
	int	l;
{
	void *p;

	if ((p = malloc(siz)) == NULL)
		fprintf(stderr, "%s:%d: out of dynamic memory\n", m, l);
	return (p);
}

void *
__xmalloc(siz, m, l)
	size_t	siz;
	const	char *m;
	int	l;
{
	void *p;
	
	if ((p = __emalloc(siz, m, l)) == NULL)
		exit(EXIT_FAILURE);
	return (p);
}

/*
 * Parses a line that should contain key/value pair as of:
 *
 *     debug    high
 *     log    = all
 * set host   = http://somewhere.com
 * let proxy    http://192.168.0.1
 *
 * The leading part is optional, kvparse will not complain if it doesn't
 * exist.  But the separator part, if specified is mandatory, kvparse will
 * fail if it failed to detect such part.
 *
 * kvparse will return kvp_t structure with the following members:
 *
 * kvp_key:    Key.
 * kvp_value:  Value.
 * kvp_valv:   Values vector (if value if more than one word like:
 *     set args = -g -o
 *   in which case kvp_valv will contain: "-g", "-o", NULL.)
 * _kvp_splt:  Vector originally returned from splitv() to be used by kvfree.
 *
 * Caller must use kvfree after finishing with kvp_t returned value to
 * free consumed memory.
 *
 * Common use:
 * kvparse("key value", NULL, NULL);
 * kvparse("key = value", NULL, "=");
 * kvparse("set key value", "set", NULL);
 */
struct kvp_t *
kvparse(line, leading, sep)
	char	*line;
	char	*leading;
	char	*sep;
{
	char **v;
	struct kvp_t *kvp;

	if ((v = splitv(line, NULL, NULL)) == NULL)
		return (NULL);
	if ((kvp = malloc(sizeof(struct kvp_t))) == NULL) {
		free(v);
		return (NULL);
	}
	kvp->_kvp_splt = v;
	if (leading != NULL && strcmp(leading, *v) == 0) {
		++v;
	}
	kvp->kvp_key = *v;
	if (sep != NULL) {
		++v;
		if (strcmp(sep, *v) != 0) {
			free(kvp->_kvp_splt);
			free(kvp);
			return (NULL);
		}
	}
	++v;
	kvp->kvp_value = *v;
	kvp->kvp_valv = v;
	return (kvp);
}

void
kvfree(kvp)
	struct	kvp_t *kvp;
{

	if (kvp != NULL && kvp->_kvp_splt != NULL)
		free(kvp->_kvp_splt);
	return;
}

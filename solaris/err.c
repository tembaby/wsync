/* $Id$ */ 

/*
 * Copyright (c) 2004 Tamer Embaby <tsemba@menanet.net>
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
#include <string.h>
#include <stdarg.h>

#include <err.h>

#define SUPPRESS_ERRNO		0x7fff

extern	int errno;
extern	char *__progname;

static void
verr(int errn, const char *tag, const char *fmt, va_list ap)
{

	fprintf(stderr, "%s: %s: ", __progname, tag);
	vfprintf(stderr, fmt, ap);
	if (errn != SUPPRESS_ERRNO)
		fprintf(stderr, ": %s.", strerror(errn));
	fprintf(stderr, "\n");
}

void
err(int errn, const char *fmt, ...)
{
	int sverrn;
	va_list ap;

	sverrn = errno;
	va_start(ap, fmt);
	verr(sverrn, "ERROR", fmt, ap);
	va_end(ap);
	exit(sverrn);
	return;
}

void
errx(int errn, const char *fmt, ...)
{
	int sverrn;
	va_list ap;

	sverrn = errno;
	va_start(ap, fmt);
	verr(SUPPRESS_ERRNO, "ERROR", fmt, ap);
	va_end(ap);
	exit(sverrn);
}

void
warn(const char *fmt, ...)
{
	int sverrn;
	va_list ap;

	sverrn = errno;
	va_start(ap, fmt);
	verr(sverrn, "WARNING", fmt, ap);
	va_end(ap);
	return;
}

void
warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(SUPPRESS_ERRNO, "WARNING", fmt, ap);
	va_end(ap);
	return;
}

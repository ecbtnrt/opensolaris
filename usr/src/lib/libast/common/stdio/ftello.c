/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2009 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#pragma prototyped

#ifndef _NO_LARGEFILE64_SOURCE
#define _NO_LARGEFILE64_SOURCE	1
#endif

#include "stdhdr.h"

off_t
ftello(Sfio_t* f)
{
	STDIO_INT(f, "ftello", off_t, (Sfio_t*), (f))

	return sfseek(f, (Sfoff_t)0, SEEK_CUR);
}

#ifdef _typ_int64_t

int64_t
ftello64(Sfio_t* f)
{
	STDIO_INT(f, "ftello64", int64_t, (Sfio_t*), (f))

	return sfseek(f, (Sfoff_t)0, SEEK_CUR) >= 0 ? 0 : -1;
}

#endif

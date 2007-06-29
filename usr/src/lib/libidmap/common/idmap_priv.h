/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Additional API for Identity Mapping Service
 */

#ifndef _IDMAP_PRIV_H
#define	_IDMAP_PRIV_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include "idmap.h"
#include "idmap_prot.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	IDMAP_MAX_NAME_LEN	512

#define	IDMAP_ERROR(rc)		rc != IDMAP_SUCCESS && rc != IDMAP_NEXT
#define	IDMAP_FATAL_ERROR(rc)	rc == IDMAP_ERR_MEMORY ||\
				rc == IDMAP_ERR_DB

/* Opaque handle to batch config add/remove operations */
typedef struct idmap_udt_handle idmap_udt_handle_t;

/* Opaque iterator */
typedef struct idmap_iter idmap_iter_t;


/*
 * Update API
 */

/* Create handle for updates */
extern idmap_stat idmap_udt_create(idmap_handle_t *,
	idmap_udt_handle_t **);

/* Commit */
extern idmap_stat idmap_udt_commit(idmap_udt_handle_t *);

/* Destroy the update handle */
extern void idmap_udt_destroy(idmap_udt_handle_t *);

/* Add name-based mapping rule */
extern idmap_stat idmap_udt_add_namerule(idmap_udt_handle_t *, const char *,
	boolean_t, const char *, const char *, boolean_t, int);

/* Remove name-based mapping rule */
extern idmap_stat idmap_udt_rm_namerule(idmap_udt_handle_t *, boolean_t,
	const char *, const char *, const char *, int);

/* Flush name-based mapping rules */
extern idmap_stat idmap_udt_flush_namerules(idmap_udt_handle_t *, boolean_t);


/*
 * Iterator API
 */

/* Create a iterator to get SID to UID/GID mappings */
extern idmap_stat idmap_iter_mappings(idmap_handle_t *, boolean_t,
	idmap_iter_t **);

/* Iterate through the SID to UID/GID mappings */
extern idmap_stat idmap_iter_next_mapping(idmap_iter_t *, char **,
	idmap_rid_t *, uid_t *, char **, char **, char **, int *);

/* Create a iterator to get name-based mapping rules */
extern idmap_stat idmap_iter_namerules(idmap_handle_t *, const char *,
	boolean_t, const char *, const char *, idmap_iter_t **);

/* Iterate through the name-based mapping rules */
extern idmap_stat idmap_iter_next_namerule(idmap_iter_t *, char **,
	char **, char **, boolean_t *, int *);

/* Set the number of entries requested per batch */
extern idmap_stat idmap_iter_set_limit(idmap_iter_t *, uint64_t);

/* Destroy the iterator */
extern void idmap_iter_destroy(idmap_iter_t *);


/*
 * Get mapping
 */
extern idmap_stat idmap_get_w2u_mapping(idmap_handle_t *, const char *,
	idmap_rid_t *, const char *, const char *, int, int *,
	uid_t *, char **, int *);

extern idmap_stat idmap_get_u2w_mapping(idmap_handle_t *, uid_t *,
	const char *, int, int, char **, idmap_rid_t *, char **,
	char **, int *);


/*
 * Miscellaneous
 */

/* utf8 to string */
extern idmap_stat idmap_utf82str(char **, size_t, idmap_utf8str *);

/* string to utf8 */
extern idmap_stat idmap_str2utf8(idmap_utf8str **, const char *, int);

/* string to status */
extern idmap_stat idmap_string2stat(const char *);

/* internal status to protocol status */
extern idmap_stat idmap_stat4prot(idmap_stat);

#ifdef __cplusplus
}
#endif

#endif /* _IDMAP_PRIV_H */
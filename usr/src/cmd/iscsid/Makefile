#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#
# cmd/iscsid/Makefile
#

PROG= 		iscsi-initiator
MANIFEST=	iscsi-initiator.xml
SVCMETHOD=	iscsi-initiator

PRODUCT=	$(PROG)
OBJS=		iscsid.o
SRCS=		$(OBJS:%.o=./%.c)
LLOBJS=		$(OBJS:%.o=%.ll)
POFILES= $(OBJS:%.o=%.po)
POFILE= iscsi-initiator.po

include ../Makefile.cmd

CCVERBOSE	=
LDLIBS += -lnsl

ROOTMANIFESTDIR=	$(ROOTSVCNETWORKISCSI)
$(ROOTSVCNETWORKISCSI)/iscsi-initiator.xml := FILEMODE = 0444

.KEEP_STATE:

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

LINTFLAGS += -erroff=E_FUNC_HAS_NO_RETURN_STMT
LINTFLAGS += -erroff=E_BAD_PTR_CAST_ALIGN

.PARALLEL: $(OBJS)

all:	$(PROG)

clean:
	$(RM) $(PROG) $(OBJS) $(LLOBJS)

lint:	lint_SRCS

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

install: all $(ROOTMANIFEST) $(ROOTSVCMETHOD)

check: $(CHKMANIFEST)

include ../Makefile.targ

#!/bin/ksh -p
# (note we use "/bin/ksh -p" for Linux/pdksh support in this script)

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
# buildksh93.sh - ast-ksh standalone build script for the 
# OpenSolaris ksh93-integration project
#

# ksh93t sources can be downloaded like this from the AT&T site:
# wget --http-user="I accept www.opensource.org/licenses/cpl" --http-passwd="." 'http://www.research.att.com/sw/download/beta/INIT.2009-10-14.tgz'
# wget --http-user="I accept www.opensource.org/licenses/cpl" --http-passwd="." 'http://www.research.att.com/sw/download/beta/ast-ksh.2009-10-14.tgz'

function fatal_error
{
    print -u2 "${progname}: $*"
    exit 1
}

set -o errexit
set -o xtrace

typeset progname="$(basename "${0}")"
typeset buildmode="$1"

if [[ "${buildmode}" == "" ]] ; then
    fatal_error "buildmode required."
fi

# Make sure we use the C locale during building to avoid any unintended
# side-effects
export LANG=C
export LC_ALL=$LANG LC_MONETARY=$LANG LC_NUMERIC=$LANG LC_MESSAGES=$LANG LC_COLLATE=$LANG LC_CTYPE=$LANG
# Make sure the POSIX/XPG6 tools are in front of /usr/bin (/bin is needed for Linux after /usr/bin)
export PATH=/usr/xpg6/bin:/usr/xpg4/bin:/usr/ccs/bin:/usr/bin:/bin:/opt/SUNWspro/bin

# Make sure the POSIX/XPG6 packages are installed (mandatory for building
# our version of ksh93 correctly).
if [[ "$(uname -s)" == "SunOS" ]] ; then
    if [[ ! -x "/usr/xpg6/bin/tr" ]] ; then
        fatal_error "XPG6/4 packages (SUNWxcu6,SUNWxcu4) not installed."
    fi
fi

function print_solaris_builtin_header
{
# Make sure to use \\ instead of \ for continuations
cat <<ENDOFTEXT
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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SOLARIS_KSH_CMDLIST_H
#define	_SOLARIS_KSH_CMDLIST_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * List builtins for Solaris.
 * The list here is partially autogenerated and partially hand-picked
 * based on compatibility with the native Solaris versions of these
 * tools
 */

/* POSIX compatible commands */
#ifdef _NOT_YET
#define	XPG6CMDLIST(f)	\\
	{ "/usr/xpg6/bin/" #f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
#define	XPG4CMDLIST(f)	\\
	{ "/usr/xpg4/bin/" #f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
#else
#define	XPG6CMDLIST(f)
#define	XPG4CMDLIST(f)
#endif /* NOT_YET */
/*
 * Commands which are 100% compatible with native Solaris versions (/bin is
 * a softlink to ./usr/bin, ksh93 takes care about the lookup)
 */
#define	BINCMDLIST(f)	\\
	{ "/bin/"	#f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
#define	USRBINCMDLIST(f)	\\
	{ "/usr/bin/"	#f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
#define	SBINCMDLIST(f)	\\
	{ "/sbin/"	#f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
#define	SUSRBINCMDLIST(f)	\\
	{ "/usr/sbin/"	#f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },
/*
 * Make all ksh93 builtins accessible when /usr/ast/bin was added to
 * /usr/xpg6/bin:/usr/xpg4/bin:/usr/ccs/bin:/usr/bin:/bin:/opt/SUNWspro/bin
 */
#define	ASTCMDLIST(f)	\\
	{ "/usr/ast/bin/" #f, NV_BLTIN|NV_BLTINOPT|NV_NOFREE, bltin(f) },

/* undo ast_map.h #defines to avoid collision */
#undef basename
#undef dirname
#undef mktemp

/* Generated data, do not edit. */
XPG4CMDLIST(basename)
ASTCMDLIST(basename)
BINCMDLIST(cat)
ASTCMDLIST(cat)
XPG4CMDLIST(chgrp)
ASTCMDLIST(chgrp)
ASTCMDLIST(chmod)
XPG4CMDLIST(chown)
BINCMDLIST(chown)
ASTCMDLIST(chown)
BINCMDLIST(cksum)
ASTCMDLIST(cksum)
BINCMDLIST(cmp)
ASTCMDLIST(cmp)
BINCMDLIST(comm)
ASTCMDLIST(comm)
XPG4CMDLIST(cp)
ASTCMDLIST(cp)
BINCMDLIST(cut)
ASTCMDLIST(cut)
XPG4CMDLIST(date)
ASTCMDLIST(date)
ASTCMDLIST(dirname)
XPG4CMDLIST(expr)
ASTCMDLIST(expr)
ASTCMDLIST(fds)
ASTCMDLIST(fmt)
BINCMDLIST(fold)
ASTCMDLIST(fold)
BINCMDLIST(head)
ASTCMDLIST(head)
XPG4CMDLIST(id)
ASTCMDLIST(id)
BINCMDLIST(join)
ASTCMDLIST(join)
XPG4CMDLIST(ln)
ASTCMDLIST(ln)
BINCMDLIST(logname)
ASTCMDLIST(logname)
BINCMDLIST(mkdir)
ASTCMDLIST(mkdir)
BINCMDLIST(mkfifo)
ASTCMDLIST(mkfifo)
BINCMDLIST(mktemp)
ASTCMDLIST(mktemp)
XPG4CMDLIST(mv)
ASTCMDLIST(mv)
BINCMDLIST(paste)
ASTCMDLIST(paste)
BINCMDLIST(pathchk)
ASTCMDLIST(pathchk)
BINCMDLIST(rev)
ASTCMDLIST(rev)
XPG4CMDLIST(rm)
ASTCMDLIST(rm)
BINCMDLIST(rmdir)
ASTCMDLIST(rmdir)
XPG4CMDLIST(stty)
ASTCMDLIST(stty)
BINCMDLIST(sum)
ASTCMDLIST(sum)
SUSRBINCMDLIST(sync)
SBINCMDLIST(sync)
BINCMDLIST(sync)
ASTCMDLIST(sync)
BINCMDLIST(tail)
XPG4CMDLIST(tail)
ASTCMDLIST(tail)
BINCMDLIST(tee)
ASTCMDLIST(tee)
BINCMDLIST(tty)
ASTCMDLIST(tty)
ASTCMDLIST(uname)
BINCMDLIST(uniq)
ASTCMDLIST(uniq)
BINCMDLIST(wc)
ASTCMDLIST(wc)

/* Mandatory for ksh93 test suite and AST scripts */
BINCMDLIST(getconf)

#ifdef	__cplusplus
}
#endif

#endif /* !_SOLARIS_KSH_CMDLIST_H */
ENDOFTEXT
}

function build_shell
{
    set -o errexit
    set -o xtrace

    # OS.cputype.XXbit.compiler
    case "${buildmode}" in
        *.linux.*)
            # ksh93+AST config flags
            bast_flags="-DSHOPT_CMDLIB_BLTIN=0 -DSH_CMDLIB_DIR=\\\"/usr/ast/bin\\\" -DSHOPT_SYSRC -D_map_libc=1"
            
            # gcc flags
            bgcc99="gcc -std=gnu99 "
            bgcc_ccflags="${bon_flags} ${bast_flags} -g"

            case "${buildmode}" in
                # Linux i386
                *.i386.32bit.gcc*)  HOSTTYPE="linux.i386" CC="${bgcc99} -fPIC" cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"
                    ;;
                *)
                    fatal_error "build_shell: Illegal Linux type/compiler build mode \"${buildmode}\"."
                    ;;
            esac
            ;;
        *.solaris.*)
            # Notes:
            # 1. Do not remove/modify these flags or their order before either
            # asking the project leads at
            # http://www.opensolaris.org/os/project/ksh93-integration/
            # These flags all have a purpose, even if they look
            # weird/redundant/etc. at the first look.
            #
            # 2. We use -KPIC here since -Kpic is too small on 64bit sparc and
            # on 32bit it's close to the barrier so we use it for both 32bit and
            # 64bit to avoid later suprises when people update libast in the
            # future
            #
            # 3. "-D_map_libc=1" is needed to force map.c to add a "_ast_" prefix to all
            # AST symbol names which may otherwise collide with Solaris/Linux libc
            #
            # 4. "-DSHOPT_SYSRC" enables /etc/ksh.kshrc support (AST default is currently
            # to enable it if /etc/ksh.kshrc or /etc/bash.bashrc are available on the
            # build machine).
            #
            # 5. -D_lib_socket=1 -lsocket -lnsl" was added to make sure ksh93 is compiled
            # with networking support enabled, the current AST build infratructure has
            # problems with detecting networking support in Solaris.
            #
            # 6. "-xc99=%all -D_XOPEN_SOURCE=600 -D__EXTENSIONS__=1" is used to force
            # the compiler into C99 mode. Otherwise ksh93 will be much slower and lacks
            # lots of arithmethic functions.
            #
            # 7. "-D_TS_ERRNO -D_REENTRANT" are flags taken from the default OS/Net
            # build system.
            #
            # 8. "-xpagesize_stack=64K is used on SPARC to enhance the performace
            #
            # 9. -DSHOPT_CMDLIB_BLTIN=0 -DSH_CMDLIB_DIR=\\\"/usr/ast/bin\\\" -DSHOPT_CMDLIB_HDR=\\\"/home/test001/ksh93/ast_ksh_20070322/solaris_cmdlist.h\\\"
            # is used to bind all ksh93 builtins to a "virtual" directory
            # called "/usr/ast/bin/" and to adjust the list of builtins
            # enabled by default to those defined by PSARC 2006/550
            
            solaris_builtin_header="${PWD}/tmp_solaris_builtin_header.h"
            print_solaris_builtin_header >"${solaris_builtin_header}"
            
            # OS/Net build flags
            bon_flags="-D_TS_ERRNO -D_REENTRANT"
            
            # ksh93+AST config flags
            bast_flags="-DSHOPT_CMDLIB_BLTIN=0 -DSH_CMDLIB_DIR=\\\"/usr/ast/bin\\\" -DSHOPT_CMDLIB_HDR=\\\"${solaris_builtin_header}\\\" -DSHOPT_SYSRC -D_map_libc=1"
            
            # Sun Studio flags
            bsunc99="/opt/SUNWspro/bin/cc -xc99=%all -D_XOPEN_SOURCE=600 -D__EXTENSIONS__=1"
            bsuncc_app_ccflags_sparc="-xpagesize_stack=64K" # use bsuncc_app_ccflags_sparc only for final executables
            bsuncc_ccflags="${bon_flags} -KPIC -g -xs -xspace -Xa -xstrconst -z combreloc -xildoff -xcsi -errtags=yes ${bast_flags} -D_lib_socket=1 -lsocket -lnsl"

            # gcc flags
            bgcc99="/usr/sfw/bin/gcc -std=gnu99 -D_XOPEN_SOURCE=600 -D__EXTENSIONS__=1"
            bgcc_warnflags="-Wall -Wextra -Wno-unknown-pragmas -Wno-missing-braces -Wno-sign-compare -Wno-parentheses -Wno-uninitialized -Wno-implicit-function-declaration -Wno-unused -Wno-trigraphs -Wno-char-subscripts -Wno-switch"
            bgcc_ccflags="${bon_flags} ${bgcc_warnflags} ${bast_flags} -D_lib_socket=1 -lsocket -lnsl"
 
            case "${buildmode}" in
	        # for -m32/-m64 flags see usr/src/Makefile.master, makefile symbols *_XARCH/co.
                *.i386.32bit.suncc*)  HOSTTYPE="sol11.i386" CC="${bsunc99} -m32"                  cc_sharedlib="-G" CCFLAGS="${bsuncc_ccflags}"  ;;
                *.i386.64bit.suncc*)  HOSTTYPE="sol11.i386" CC="${bsunc99} -m64 -KPIC"            cc_sharedlib="-G" CCFLAGS="${bsuncc_ccflags}"  ;;
                *.sparc.32bit.suncc*) HOSTTYPE="sol11.sun4" CC="${bsunc99} -m32"                  cc_sharedlib="-G" CCFLAGS="${bsuncc_ccflags}" bsuncc_app_ccflags="${bsuncc_app_ccflags_sparc}" ;;
                *.sparc.64bit.suncc*) HOSTTYPE="sol11.sun4" CC="${bsunc99} -m64 -dalign -KPIC"    cc_sharedlib="-G" CCFLAGS="${bsuncc_ccflags}" bsuncc_app_ccflags="${bsuncc_app_ccflags_sparc}" ;;

                *.i386.32bit.gcc*)  HOSTTYPE="sol11.i386" CC="${bgcc99} -fPIC"                                            cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;
                *.i386.64bit.gcc*)  HOSTTYPE="sol11.i386" CC="${bgcc99} -m64 -mtune=opteron -Ui386 -U__i386 -fPIC"        cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;
                *.sparc.32bit.gcc*) HOSTTYPE="sol11.sun4" CC="${bgcc99} -m32 -mcpu=v8 -fPIC"                              cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;
                *.sparc.64bit.gcc*) HOSTTYPE="sol11.sun4" CC="${bgcc99} -m64 -mcpu=v9 -fPIC"                              cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;
                *.s390.32bit.gcc*)  HOSTTYPE="sol11.s390" CC="${bgcc99} -m32          -fPIC"				  cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;
                *.s390.64bit.gcc*)  HOSTTYPE="sol11.s390" CC="${bgcc99} -m64          -fPIC"				  cc_sharedlib="-shared" CCFLAGS="${bgcc_ccflags}"  ;;

                *)
                    fatal_error "build_shell: Illegal Solaris type/compiler build mode \"${buildmode}\"."
                    ;;
            esac
            ;;
        *)
            fatal_error "Illegal OS build mode \"${buildmode}\"."
            ;;
    esac

    # some prechecks
    [[ -z "${CCFLAGS}"  ]] && fatal_error "build_shell: CCFLAGS is empty."
    [[ -z "${CC}"       ]] && fatal_error "build_shell: CC is empty."
    [[ -z "${HOSTTYPE}" ]] && fatal_error "build_shell: HOSTTYPE is empty."
    [[ ! -f "bin/package" ]] && fatal_error "build_shell: bin/package missing."
    [[ ! -x "bin/package" ]] && fatal_error "build_shell: bin/package not executable."

    export CCFLAGS CC HOSTTYPE

    # build ksh93
    bin/package make CCFLAGS="${CCFLAGS}" CC="${CC}" HOSTTYPE="${HOSTTYPE}"

    root="${PWD}/arch/${HOSTTYPE}"
    [[ -d "$root" ]] || fatal_error "build_shell: directory ${root} not found."
    log="${root}/lib/package/gen/make.out"

    [[ -s $log ]] || fatal_error "build_shell: no make.out log found."

    if [[ -f ${root}/lib/libast-g.a   ]] then link_libast="ast-g"     ; else link_libast="ast"     ; fi
    if [[ -f ${root}/lib/libdll-g.a   ]] then link_libdll="dll-g"     ; else link_libdll="dll"     ; fi
    if [[ -f ${root}/lib/libsum-g.a   ]] then link_libsum="sum-g"     ; else link_libsum="sum"     ; fi
    if [[ -f ${root}/lib/libcmd-g.a   ]] then link_libcmd="cmd-g"     ; else link_libcmd="cmd"     ; fi
    if [[ -f ${root}/lib/libshell-g.a ]] then link_libshell="shell-g" ; else link_libshell="shell" ; fi

    if [[ "${buildmode}" != *.staticshell* ]] ; then
        # libcmd causes some trouble since there is a squatter in solaris
        # This has been fixed in Solaris 11/B48 but may require adjustments
        # for older Solaris releases
        for lib in libast libdll libsum libcmd libshell ; do
            case "$lib" in
            libshell)
                base="src/cmd/ksh93/"
                vers=1
                link="-L${root}/lib/ -l${link_libcmd} -l${link_libsum} -l${link_libdll} -l${link_libast} -lm"
                ;;
            libdll)
                base="src/lib/${lib}"
                vers=1
                link="-ldl"
                ;;
            libast)
                base="src/lib/${lib}"
                vers=1
                link="-lm"
                ;;
            *)
                base="src/lib/${lib}"
                vers=1
                link="-L${root}/lib/ -l${link_libast} -lm"
                ;;
            esac

            (
            cd "${root}/${base}"
	    
	    if [[ -f ${lib}-g.a ]] ; then lib_a="${lib}-g.a" ; else lib_a="${lib}.a" ; fi
	    
            if [[ "${buildmode}" == *solaris* ]] ; then
                ${CC} ${cc_sharedlib} ${CCFLAGS} -Bdirect -Wl,-zallextract -Wl,-zmuldefs -o "${root}/lib/${lib}.so.${vers}" "${lib_a}"  $link
            else
                ${CC} ${cc_sharedlib} ${CCFLAGS} -Wl,--whole-archive -Wl,-zmuldefs "${lib_a}" -Wl,--no-whole-archive -o "${root}/lib/${lib}.so.${vers}" $link
            fi
           
            #rm ${lib}.a
            mv "${lib_a}" "disabled_${lib_a}_"

            cd "${root}/lib"
            ln -sf "${lib}.so.${vers}" "${lib}.so"
            )
        done

        (
          base=src/cmd/ksh93
          cd "${root}/${base}"
          rm -f \
	      "${root}/lib/libshell.a" "${root}/lib/libshell-g.a" \
              "${root}/lib/libsum.a" "${root}/lib/libsum-g.a" \
              "${root}/lib/libdll.a" "${root}/lib/libdll-g.a" \
              "${root}/lib/libast.a""${root}/lib/libast-g.a"

          if [[ "${buildmode}" == *solaris* ]] ; then
              ${CC} ${CCFLAGS} ${bsuncc_app_ccflags} -L${root}/lib/ -Bdirect -o ksh pmain.o -lshell -Bstatic -l${link_libcmd} -Bdynamic -lsum -ldll -last -lm -lmd -lsecdb
          else
              ${CC} ${CCFLAGS} ${bsuncc_app_ccflags} -L${root}/lib/ -o ksh pmain.o -lshell -lcmd -lsum -ldll -last -lm
          fi
          
	  file ksh
	  file shcomp
	  
	  export LD_LIBRARY_PATH="${root}/lib:${LD_LIBRARY_PATH}"
	  export LD_LIBRARY_PATH_32="${root}/lib:${LD_LIBRARY_PATH_32}"
	  export LD_LIBRARY_PATH_64="${root}/lib:${LD_LIBRARY_PATH_64}"
	  ldd ksh
        )
    fi
}

function test_builtin_getconf
{
(
    print "# testing getconf builtin..."
    set +o errexit
    export PATH=/bin:/usr/bin
    for lang in ${TEST_LANG} ; do
        (
	    printf "## testing LANG=%s\n" "${lang}"
            export LC_ALL="${lang}" LANG="${lang}"
            ${SHELL} -c '/usr/bin/getconf -a | 
                         while read i ; do 
                         t="${i%:*}" ; a="$(getconf "$t" 2>/dev/null)" ; 
                         b="$(/usr/bin/getconf "$t" 2>/dev/null)" ; [ "$a" != "$b" ] && print "# |$t|:|$a| != |$b|" ;
                         done'
        )
    done
    print "# testing getconf done."
)
}

function test_shell
{
    set -o errexit
    set -o xtrace
    
    ulimit -s 65536 # need larger stack on 64bit SPARC to pass all tests

    export SHELL="$(ls -1 $PWD/arch/*/src/cmd/ksh93/ksh)"
    export LD_LIBRARY_PATH="$(ls -1ad $PWD/arch/*/lib):${LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH_32="$(ls -1ad $PWD/arch/*/lib):${LD_LIBRARY_PATH_32}"
    export LD_LIBRARY_PATH_64="$(ls -1ad $PWD/arch/*/lib):${LD_LIBRARY_PATH_64}"
    printf "## SHELL is |%s|\n" "${SHELL}"
    printf "## LD_LIBRARY_PATH is |%s|\n" "${LD_LIBRARY_PATH}"
    
    [[ ! -f "${SHELL}" ]] && fatal_error "test_shell: |${SHELL}| is not a file."
    [[ ! -x "${SHELL}" ]] && fatal_error "test_shell: |${SHELL}| is not executable."
    
    [[ "${TEST_LANG}" == "" ]] && TEST_LANG="C ja_JP.UTF-8"

    case "${buildmode}" in
            testshell.bcheck*)
                for lang in ${TEST_LANG} ; do
                    (
                        export LC_ALL="${lang}" LANG="${lang}"
                        for i in ./src/cmd/ksh93/tests/*.sh ; do 
                            bc_logfile="$(basename "$i").$$.bcheck"
                            rm -f "${bc_logfile}"
                            /opt/SUNWspro/bin/bcheck -q -access -o "${bc_logfile}" ${SHELL} ./src/cmd/ksh93/tests/shtests \
                                LD_LIBRARY_PATH_64="$LD_LIBRARY_PATH_64" \
                                LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
                                LD_LIBRARY_PATH_32="$LD_LIBRARY_PATH_32"\
                                LC_ALL="${lang}" LANG="${lang}" \
                                VMDEBUG=a \
                                "$i"
                            cat "${bc_logfile}"
                        done
                    )
                done
                ;;
            testshell.builtin.getconf)
                test_builtin_getconf
                ;;
            testshell)
                for lang in ${TEST_LANG} ; do
                    (
                        export LC_ALL="${lang}" LANG="${lang}"
                        for i in ./src/cmd/ksh93/tests/*.sh ; do 
                            ${SHELL} ./src/cmd/ksh93/tests/shtests -a \
                                LD_LIBRARY_PATH_64="$LD_LIBRARY_PATH_64" \
                                LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
                                LD_LIBRARY_PATH_32="$LD_LIBRARY_PATH_32" \
                                LC_ALL="${lang}" LANG="${lang}" \
                                VMDEBUG=a \
				SHCOMP=$PWD/arch/*/bin/shcomp \
                                "$i"
                        done
                    )
                done
                test_builtin_getconf
                ;;
    esac
}

# main
case "${buildmode}" in
        build.*) build_shell ;;
        testshell*)  test_shell  ;;
        *) fatal_error "Illegal build mode \"${buildmode}\"." ;;
esac
# EOF.

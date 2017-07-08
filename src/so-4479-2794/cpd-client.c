/*
@(#)File:           $RCSfile$
@(#)Version:        $Revision$
@(#)Last changed:   $Date$
@(#)Purpose:        CPD Client for SO 4479-2794
@(#)Author:         J Leffler
@(#)Copyright:      (C) JLSS 2017
@(#)Product:        :PRODUCT:
*/

/*TABSTOP=4*/

/*
** NB: This code uses some of the code from Stevens et al "Unix Network
** Programming, Volume 1, 3rd Edition" (aka UNP, or UNPv13e).
** Specifically, this code uses (a much cut down variant of) the unp.h
** header and four functions:
**  -  tcp_connect()
**  -  tcp_listen()
**  -  daemon_init() - renamed to daemonize() when imported to JLSS
**  -  Accept() - renamed to tcp_accept() when imported to JLSS, and
**     modified to handle SIGCHLD signals, unlike the book's version.
*/

#include "posixver.h"
#include "cpd.h"
#include "stderr.h"
#include "unpv13e.h"
#include <assert.h>
#include <ftw.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

static const char optstr[] = "hvVl:s:p:S:T:";
static const char usestr[] = "[-hvV][-l log][-s host][-p port][-S source][-T target]";
static const char hlpstr[] =
    "  -h         Print this help message and exit\n"
    "  -l log     Record errors in log file\n"
    "  -p port    Connect to cpd-server on this port (default 30991)\n"
    "  -s host    Connect to cpd-server on this host (default localhost)\n"
    "  -v         Set verbose mode\n"
    "  -S source  Source directory (default .)\n"
    "  -T target  Target directory (default - realpath for .)\n"
    "  -V         Print version information and exit\n"
    ;

static char default_source[] = ".";
static char default_target[] = ".";
static char default_server[] = "localhost";
static char default_logger[] = "/dev/null";
static char default_portno[] = STRINGIZE(CPD_DEFAULT_PORT);

static char *source = default_source;
static char *target = default_target;
static char *server = default_server;
static char *logger = default_logger;
static char *portno = default_portno;

static int cpd_fd = -1;
static int verbose = 0;

static void cpd_client(void);

#ifndef lint
/* Prevent over-aggressive optimizers from eliminating ID string */
extern const char jlss_id_cpd_client_c[];
const char jlss_id_cpd_client_c[] = "@(#)$Id$";
#endif /* lint */

int main(int argc, char **argv)
{
    err_setarg0(argv[0]);

    int opt;
    while ((opt = getopt(argc, argv, optstr)) != -1)
    {
        switch (opt)
        {
        case 'h':
            err_help(usestr, hlpstr);
            /*NOTREACHED*/
        case 'l':
            logger = optarg;
            break;
        case 'p':
            portno = optarg;
            break;
        case 's':
            server = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'S':
            source = optarg;
            break;
        case 'T':
            target = optarg;
            break;
        case 'V':
            err_version("CPD-CLIENT", &"@(#)$Revision$ ($Date$)"[4]);
            /*NOTREACHED*/
        default:
            err_usage(usestr);
            /*NOTREACHED*/
        }
    }

    if (optind != argc)
    {
        err_remark("Extraneous arguments, starting with '%s'\n", argv[optind]);
        err_usage(usestr);
    }

    FILE *log_fp = stderr;
    if (logger != default_logger)
    {
        if ((log_fp = fopen(logger, "a")) == 0)
            err_syserr("failed to open log file '%s': ", logger);
        err_stderr(log_fp);
    }

    cpd_client();

    if (log_fp != stderr)
        fclose(log_fp);

    return 0;
}

static int ftw_callback(const char *file, const struct stat *ptr, int flag)
{
    assert(file != 0);
    assert(ptr != 0);
    assert(flag == flag);   /* tautology */
    printf("FTW-CB: Name [%s]\n", file);
    return 0;
}

static void cpd_send_target(int fd, char *target)
{
    err_remark("Sending target [%s]\n", target);
    assert(target != 0);
    size_t len0 = 1;
    size_t len1 = 2;
    size_t len2 = strlen(target) + 1;
    Byte opcode[1] = { CPD_TARGETDIR };
    Byte tgtlen[2];
    st_int2(tgtlen, len2);
    assert(len2 <= UINT16_MAX);
    ssize_t explen = len0 + len1 + len2;
    ssize_t actlen;
    struct iovec iov[3] =
    {
        { .iov_len = len0, .iov_base = (char *)opcode },
        { .iov_len = len1, .iov_base = (char *)tgtlen },
        { .iov_len = len2, .iov_base = (char *)target },
    };
    actlen = writev(fd, iov, 3);
    if (actlen != explen)
        err_syserr("write error to server (wanted: %zu bytes, actual: %zd): ",
                   len0 + len1 + len2, actlen);
    err_remark("Target [%s] sent\n", target);
}

static void cpd_send_finished(int fd)
{
    printf("Sending finished\n");
    assert(target != 0);
    Byte opcode[1] = { CPD_FINISHED };
    if (write(fd, &opcode, sizeof(opcode)) != sizeof(opcode))
        err_syserr("write error to server (%zu bytes): ", sizeof(opcode));
}

static void cpd_recv_status(int fd, int *errnum, char **msgtxt)
{
    assert(fd >= 0);
    assert(errnum != 0);
    assert(msgtxt != 0);
    Byte err[2];
    if (read(fd, err, sizeof(err)) != sizeof(err))
        err_syserr("failed to read %zu bytes\n", sizeof(err));
    *errnum = ld_int2(err);
    Byte len[2];
    if (read(fd, len, sizeof(len)) != sizeof(len))
        err_syserr("failed to read %zu bytes\n", sizeof(err));
    uint16_t msglen = ld_int2(len);
    if (msglen == 0)
        *msgtxt = 0;
    else
    {
        *msgtxt = malloc(msglen);
        if (*msgtxt == 0)
            err_syserr("failed to allocate %d bytes\n", msglen);
        if (read(fd, *msgtxt, msglen) != msglen)
            err_syserr("failed to read %d bytes\n", msglen);
        assert((*msgtxt)[msglen - 1] == '\0');
    }
    printf("%s: status %d L = %d [%s]\n", __func__, *errnum, msglen, *msgtxt ? *msgtxt : "");
}

static void cpd_recv_message(int fd)
{
    assert(fd >= 0);
    Byte opcode;
    if (read(fd, &opcode, sizeof(opcode)) != sizeof(opcode))
        err_syserr("failed to read any response: ");
    switch (opcode)
    {
    case CPD_STATUS:
        {
        int errnum;
        char *msgtxt;
        cpd_recv_status(fd, &errnum, &msgtxt);
        free(msgtxt);
        }
        break;
    default:
        err_internal(__func__, "Unexpected opcode %d (0x%.2X)\n", opcode, opcode);
        /*NOTREACHED*/
    }
}

static void cpd_client(void)
{
    /* tcp_connect() does not return if it fails to connect */
    cpd_fd = tcp_connect(server, portno);
    assert(cpd_fd >= 0);
    cpd_send_target(cpd_fd, target);
    cpd_recv_message(cpd_fd);
    err_remark("Sending request\n");
    if (verbose)
        err_remark("The directory being copied is: %s\n", source);
    if (ftw(source, ftw_callback, 10) != 0)
        err_error("failed to traverse directory tree\n");
    cpd_send_finished(cpd_fd);
    cpd_recv_message(cpd_fd);
    if (close(cpd_fd) != 0)
        err_syserr("failed to close socket: ");
    if (verbose)
        err_remark("Directory %s has been copied\n", source);
}


/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded support for traverse and load/unload
 * PMNS operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pthread.h>

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

#define ITER 10

static int nmetric;

void
dometric(const char *name)
{
    nmetric++;
}

static void *
func1(void *arg)
{
    int		sts;
    int		i;
    char	msgbuf[PM_MAXERRMSGLEN];

    for (i = 0; i < ITER; i++) {
	nmetric = 0;
	sts = pmTraversePMNS("", dometric);
	if (sts >= 0)
	    printf("traverse: found %d metrics, sts %d\n", nmetric, sts);
	else {
	    /*
	     * expect 0 metrics and PM_ERR_NOPMNS if the pmTraversePMNS
	     * gets in between the pmUnloadPMNS and the pmLoadPMNS in the
	     * other thread
	     */
	    if (nmetric > 0 || sts != PM_ERR_NOPMNS)
		printf("traverse: found %d metrics, sts %s\n", nmetric, pmErrStr_r(sts, msgbuf, PM_MAXERRMSGLEN));
	    else {
		/* 
		 * nmetric == 0 && sts == PM_ERR_NOPMNS, so try again ...
		 * won't loop forever because eventually func2() will
		 * finish
		 */
		i--;
	    }
	}
    }

    return(NULL);	/* pthread done */
}

static void *
func2(void *arg)
{
    int		sts;
    char	*fn = "func2";
    int		i;
    char	msgbuf[PM_MAXERRMSGLEN];

    for (i = 0; i < ITER; i++) {
	pmUnloadNameSpace();
	if ((sts = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	    printf("%s: pmLoadNameSpace[%d]: %s\n", fn, i, pmErrStr_r(sts, msgbuf, PM_MAXERRMSGLEN));
	    exit(1);
	}
    }

    return(NULL);	/* pthread done */
}

static void
wait_for_thread(char *name, pthread_t tid)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid, (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %s: pthread_join: cancelled?\n", name);
	else if (msg != NULL)
	    printf("thread %s: pthread_join: %s\n", name, msg);
    }
    else
	printf("thread %s: pthread_join: error: %s\n", name, strerror(sts));
}

int
main(int argc, char **argv)
{
    pthread_t		tid1;
    pthread_t		tid2;
    int			sts;
    unsigned int	in[PDU_MAX+1];
    unsigned int	out[PDU_MAX+1];
    int			i;
    int			c;
    int			errflag = 0;
    char		msgbuf[PM_MAXERRMSGLEN];

    pmSetProgname(argv[0]);

    setvbuf(stdout, NULL, _IONBF, 0);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (optind != argc) {
	printf("Usage: multithread4\n");
	exit(1);
    }

    for (i = 0; i <= PDU_MAX; i++) {
	in[i] = out[i] = 0;
    }
    __pmSetPDUCntBuf(in, out);

    if ((sts = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	printf("%s: pmLoadNameSpace: %s\n", argv[0], pmErrStr_r(sts, msgbuf, PM_MAXERRMSGLEN));
	exit(1);
    }

    sts = pthread_create(&tid1, NULL, func1, NULL);
    if (sts != 0) {
	printf("thread_create: tid1: sts=%d\n", sts);
	exit(1);
    }
    sts = pthread_create(&tid2, NULL, func2, NULL);
    if (sts != 0) {
	printf("thread_create: tid2: sts=%d\n", sts);
	exit(1);
    }

    wait_for_thread("tid1", tid1);
    wait_for_thread("tid2", tid2);

    printf("Total PDU counts\n");
    printf("in:");
    for (i = 0; i <= PDU_MAX; i++)
	printf(" %d", in[i]);
    putchar('\n');
    printf("out:");
    for (i = 0; i <= PDU_MAX; i++)
	printf(" %d", out[i]);
    putchar('\n');

    exit(0);
}

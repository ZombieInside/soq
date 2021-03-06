/* SO 0153-8644 - Determine if a number is prime */
#include "posixver.h"
#include "stderr.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/*
** NB: setitimer() is marked obsolescent in POSIX 2008.  However, the
** replacement (timer_create(), timer_delete(), timer_settime()) is not
** available in Mac OS X 10.11.5 so using the obsolescent is more
** portable than using the replacement.
*/
#ifndef NO_PROGRESS_REPORTING
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#define PROGRESS_REPORT(x)  (x)
#else
#define PROGRESS_REPORT(x)  ((void)0)
#endif

#define WRAPPED_HEADER "timer.h"
#include "wraphead.h"

/* Original code - extremely slow */
static int IsPrime0(unsigned number)
{
    for (unsigned i = 2; i < number; i++)
    {
        if (number % i == 0)
        //if (number % i == 0 && i != number)
            return 0;
    }
    return 1;
}

/* First step up - radically better than IsPrime0() */
static int IsPrime1(unsigned number)
{
    if (number <= 1)
        return 0;
    unsigned i;
    for (i = 2; i * i <= number; i++)
    {
        if (number % i == 0)
            return 0;
    }
    return 1;
}

/* Second step up - noticeably better than IsPrime1() */
static int IsPrime2(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    for (unsigned i = 5; i * i <= number; i += 2)
    {
        if (number % i == 0)
            return 0;
    }
    return 1;
}

/* Slight step back - marginally slower than IsPrime2() */
static int IsPrime3(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    unsigned max = sqrt(number);
    for (unsigned i = 5; i <= max; i += 2)
    {
        if (number % i == 0)
            return 0;
    }
    return 1;
}

/* Third step up - noticeably better than IsPrime2() */
static int isprime1(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    unsigned max = sqrt(number) + 1;
    for (unsigned i = 6; i <= max; i += 6)
    {
        if (number % (i - 1) == 0 || number % (i + 1) == 0)
            return 0;
    }
    return 1;
}

/* Fourth step up - marginally worse than isprime1() */
static int isprime2(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    for (unsigned i = 6; (i - 1) * (i - 1) <= number; i += 6)
    {
        if (number % (i - 1) == 0 || number % (i + 1) == 0)
            return 0;
    }
    return 1;
}

/* Fifth step up - usually marginally but measurably better than isprime1() */
static int isprime3(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    unsigned int small_primes[] =
    {
         5,  7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
        53, 59, 61, 67, 71, 73, 79, 83, 87, 89, 91, 97
    };
    enum { NUM_SMALL_PRIMES = sizeof(small_primes) / sizeof(small_primes[0]) };
    for (unsigned i = 0; i < NUM_SMALL_PRIMES; i++)
    {
        if (number == small_primes[i])
            return 1;
        if (number % small_primes[i] == 0)
            return 0;
    }
    for (unsigned i = 102; (i - 1) * (i - 1) <= number; i += 6)
    {
        if (number % (i - 1) == 0 || number % (i + 1) == 0)
            return 0;
    }
    return 1;
}

/*
** Late-comer.  One test showed it slightly slower than isprime3() - but
** the same test showed isprime2() faster than both isprime3() and
** isprime4()
*/
static int isprime4(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    for (unsigned x = 5; x * x <= number; x += 6)
    {
        if (number % x == 0 || number % (x + 2) == 0)
            return 0;
    }
    return 1;
}

/* Another trial - this seems to be a little faster than either isprime3() or isprime4() */
static int isprime5(unsigned number)
{
    if (number <= 1)
        return 0;
    if (number == 2 || number == 3)
        return 1;
    if (number % 2 == 0 || number % 3 == 0)
        return 0;
    static const unsigned int small_primes[] =
    {
         5,  7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
        53, 59, 61, 67, 71, 73, 79, 83, 87, 89, 91, 97
    };
    enum { NUM_SMALL_PRIMES = sizeof(small_primes) / sizeof(small_primes[0]) };
    for (unsigned i = 0; i < NUM_SMALL_PRIMES; i++)
    {
        if (number == small_primes[i])
            return 1;
        if (number % small_primes[i] == 0)
            return 0;
    }
    for (unsigned i = 101; i * i <= number; i += 6)
    {
        if (number % i == 0 || number % (i + 2) == 0)
            return 0;
    }
    return 1;
}

static void test_primality_tester(const char *tag, int seed, int (*prime)(unsigned), int count)
{
    srand(seed);
    Clock clk;
    int nprimes = 0;
    clk_init(&clk);

    clk_start(&clk);
    for (int i = 0; i < count; i++)
    {
        if (prime(rand()))
            nprimes++;
    }
    clk_stop(&clk);

    char buffer[32];
    printf("%9s: %d primes found (out of %d) in %s s\n", tag, nprimes,
           count, clk_elapsed_us(&clk, buffer, sizeof(buffer)));
}

static int check_number(unsigned v)
{
    int p1 = IsPrime1(v);
    int p2 = IsPrime2(v);
    int p3 = IsPrime3(v);
    int p4 = isprime1(v);
    int p5 = isprime2(v);
    int p6 = isprime3(v);
    int p7 = isprime4(v);
    int p8 = isprime5(v);
    if (p1 != p2 || p1 != p3 || p1 != p4 || p1 != p5 || p1 != p6 || p1 != p7 || p1 != p8)
    {
        PROGRESS_REPORT(putchar('\n'));
        printf("!! FAIL !! %10u: IsPrime1() %d; isPrime2() %d;"
                " IsPrime3() %d; isprime1() %d; isprime2() %d;"
                " isprime3() %d; isprime4() %d; isprime5() %d\n",
                v, p1, p2, p3, p4, p5, p6, p7, p8);
        return 1;
    }
    return 0;
}

#ifndef NO_PROGRESS_REPORTING
static volatile sig_atomic_t icounter = 0;
static void alarm_handler(int signum)
{
    assert(signum == SIGALRM);
    if (write(STDOUT_FILENO, ".", 1) != 1)
        exit(1);
    if (++icounter % 60 == 0)
    {
        if (write(STDOUT_FILENO, "\n", 1) != 1)
            exit(1);
    }
}

static void set_interval_timer(int interval)
{
    struct itimerval iv = { { .tv_sec = interval, .tv_usec = 0 },
                            { .tv_sec = interval, .tv_usec = 0 } };
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, 0);
    setitimer(ITIMER_REAL, &iv, 0);
}
#endif

static void bake_off(int seed, int count)
{
    srand(seed);
    Clock clk;
    clk_init(&clk);
    printf("Seed: %d\n", seed);
    printf("Bake-off...warning this often takes more than two minutes.\n");
    PROGRESS_REPORT(set_interval_timer(1));

    clk_start(&clk);

    int failures = 0;

    /* Check numbers to 1000 */
    for (unsigned v = 1; v < 1000; v++)
    {
        if (check_number(v))
            failures++;
    }

    /* Check random numbers */
    for (int i = 0; i < count; i++)
    {
        unsigned v = rand();
        if (check_number(v))
            failures++;
    }

    clk_stop(&clk);
    PROGRESS_REPORT(set_interval_timer(0));

    PROGRESS_REPORT(putchar('\n'));
    char buffer[32];
    (void)clk_elapsed_us(&clk, buffer, sizeof(buffer));
    if (failures == 0)
        printf("== PASS == %s s\n", buffer);
    else
        printf("!! FAIL !! %d failures in %s s\n", failures, buffer);
}

enum { COUNT = 10000000 };

static void one_test(int seed, bool do_IsPrimeX)
{
    printf("Seed: %d\n", seed);
    assert(COUNT > 100000);
    if (do_IsPrimeX)
    {
        test_primality_tester("IsPrime0", seed, IsPrime0, COUNT / 100000);
        test_primality_tester("IsPrime1", seed, IsPrime1, COUNT);
        test_primality_tester("IsPrime2", seed, IsPrime2, COUNT);
        test_primality_tester("IsPrime3", seed, IsPrime3, COUNT);
    }
    test_primality_tester("isprime1", seed, isprime1, COUNT);
    test_primality_tester("isprime2", seed, isprime2, COUNT);
    test_primality_tester("isprime3", seed, isprime3, COUNT);
    test_primality_tester("isprime4", seed, isprime4, COUNT);
    test_primality_tester("isprime5", seed, isprime5, COUNT);
}

static const char optstr[] = "bhz";
static const char usestr[] = "[-bhz] [seed ...]";
static const char hlpstr[] =
    "  -b  Suppress the bake-off check\n"
    "  -h  Print this help message and exit\n"
    "  -z  Test speed of IsPrime0..IsPrime3 too\n"
    ;

int main(int argc, char **argv)
{
    int opt;
    bool do_bake_off = true;
    bool do_IsPrimeX = false;

    err_setarg0(argv[0]);
    while ((opt = getopt(argc, argv, optstr)) != -1)
    {
        switch (opt)
        {
            case 'b':
                do_bake_off = false;
                break;
            case 'h':
                err_help(usestr, hlpstr);
                /*NOTREACHED*/
            case 'z':
                do_IsPrimeX = true;
                break;
            default:
                err_usage(usestr);
                /*NOTREACHED*/
        }
    }

    int seed = time(0);
    if (do_bake_off)
    {
        bake_off(seed, COUNT);
    }

    if (optind != argc)
    {
        for (int i = optind; i < argc; i++)
            one_test(atoi(argv[i]), do_IsPrimeX);
    }
    else
        one_test(seed, do_IsPrimeX);

    return(0);
}

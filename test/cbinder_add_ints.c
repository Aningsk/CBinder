/**
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "cbinder_add_ints.h"
#include "middle.h"

#define OPTSTRING   "s:c:n:d:p:h" /* Used by getopt() */

void run_client(struct test_options options)
{
    client(options);
}

void run_server(struct test_options options)
{
    server(options);
}

void print_help_info(char *app_name)
{
    printf("Usage: %s [options]\n", app_name);
    printf("\toptions:\n");
    printf("\t-s cpu - server CPU number\n");
    printf("\t-c cpu - client CPU number\n");
    printf("\t-n num - iterations\n");
    printf("\t-d time - delay after operation in seconds\n");
    printf("\t-p payload - payload size (0 for correctness test)\n");

}

void print_options(struct test_options *options)
{
    if (UNBOUND_CPU == options->server_cpu)
        printf("Server CPU: unbound\n");
    else
        printf("Server CPU: %d\n", options->server_cpu);

    if (UNBOUND_CPU == options->client_cpu)
        printf("Client CPU: unbound\n");
    else
        printf("Client CPU: %d\n", options->client_cpu);

    printf("Iterations: %d\n", options->iterations);

    if (0 == options->payload_size)
        printf("Mode: correctness test\n");
    else
        printf("Mode: performance test (payload size = %d)\n",
                options->payload_size);
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int opt = 0;
    cpu_set_t avail_cpus;

    pid_t pid;
    int status;

    struct test_options options = {
        UNBOUND_CPU, /* server CPU */
        UNBOUND_CPU, /* client CPU */
        1000, /* iterations */
        0, /* payload size */
        1e-3, /* end of iteration delay */
    };

    /**
     * Determine CPUs available for use.
     * This testcase limits its self to using CPUs that were
     * available at the start of the benchmark.
     */
    ret = sched_getaffinity(0, sizeof(avail_cpus), &avail_cpus);
    if (0 != ret) {
        fprintf(stderr,
                "sched_getaffinity() fail. ret = %d, errno = %d\n",
                ret, errno);
        exit(1);
    }

    /**
     * Parse command line arguments 
     * getopt() will return -1,
     * when all cmd line options have been parsed.
     * If the first time getopt() return -1,
     * it will be error in this application,
     * as the application need one option at least.
     */
    opt = getopt(argc, argv, OPTSTRING);
    if (-1 == opt) {
        print_help_info(argv[0]);
        exit(1);
    }

    do {
        char *chptr; /* character pointer for cmd line parding */
        int cpu = 0;

        switch (opt) {
        case 'c': /* clinet CPU */
        case 's': /* server CPU */
            /* Parse the CPU number */
            cpu = strtoul(optarg, &chptr, 10);
            if ('\0' != *chptr) {
                fprintf(stderr,
                        "Invalid CPU specified for %d option of %s\n",
                        opt, optarg);
                exit(1);
            }

            /* Check the CPU is available */
            if (!CPU_ISSET(cpu, &avail_cpus)) {
                fprintf(stderr,
                        "CPU %s not currently available, "
                        "Available CPUs: %d\n",
                        optarg, CPU_COUNT(&avail_cpus));
            }

            if ('c' == opt)
                options.client_cpu = cpu;
            else if ('s' == opt)
                options.server_cpu = cpu;
            else
                exit(2); /* Never here */

            break;

        case 'n': /* iterations */
            options.iterations = strtoul(optarg, &chptr, 10);
            if ('\0' != *chptr) {
                fprintf(stderr,
                        "Invalid iterations specified of %s\n",
                        optarg);
                exit(1);
            }
            if (1 > options.iterations) {
                fprintf(stderr,
                        "Less than 1 iteration specified by: %s\n",
                        optarg);
                exit(1);
            }
            break;

        case 'd': /* delay between each iteration */
            options.iter_delay = strtod(optarg, &chptr);
            if ('\0' != *chptr || 0.0 > options.iter_delay) {
                fprintf(stderr,
                        "Invalid delay specified of: %s\n",
                        optarg);
                exit(1);
            }
            break;

        case 'p': /* payload size */
            options.payload_size = strtoul(optarg, &chptr, 10);
            if ('\0' != *chptr) {
                fprintf(stderr,
                        "Invalid payload size specified of: %s\n",
                        optarg);
                exit(1);
            }
            break;

        case 'h': /* help information */
            print_help_info(argv[0]);
            exit(0);
        case '?': /* others option is not supported */
            print_help_info(argv[0]);
            exit(1);
        }
    } while (-1 != (opt = getopt(argc, argv, OPTSTRING)));

    /* Display selected options */
    print_options(&options);

    /**
     * This process run as server.
     * Fork a new process run as client.
     */
    pid = fork();
    if (-1 == pid) {
        fprintf(stderr, "Fork error.\n");
        exit(1);
    }

    if (0 == pid) {
        run_client(options);
        return 0;
    } else {
        run_server(options);
        /* Wait children process end */
        ret = wait(&status);
        if ((-1 == ret) && (ECHILD == errno)) {
            printf("There is not children process.\n");
            return 0;
        }
        if (-1 == ret) {
            fprintf(stderr,
                    "wait fail. ret = %d, errno = %d\n",
                    ret, errno);
            exit(1);
        }
    }

    return 0;
}

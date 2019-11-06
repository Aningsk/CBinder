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
#include <cerrno>
#include <grp.h>
#include <iostream>
#include <libgen.h>
#include <time.h>
#include <unistd.h>
#include <float.h>

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include "testUtil.h"

#include "cbinder_add_ints.h"
#include "middle.h"

using namespace android;
using namespace std;

String16 serviceName("test.binderAddInts");

struct test_options server_options;
struct test_options client_options;

static ostream &operator<<(ostream &stream, const String16& str);
static ostream &operator<<(ostream &stream, const cpu_set_t& set);

static void bindCPU(unsigned int cpu)
{
    int rv;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    rv = sched_setaffinity(0, sizeof(cpuset), &cpuset);

    if (rv != 0) {
        cerr << "bindCPU failed, rv: " << rv << " errno: " << errno << endl;
        perror(NULL);
        exit(30);
    }
}

class AddIntsService : public BBinder
{
  public:
    AddIntsService(int cpu = UNBOUND_CPU);
    virtual ~AddIntsService() {}

    enum command {
        ADD_INTS = 0x120,
    };

    virtual status_t onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags = 0);

  private:
    int cpu_;
};

AddIntsService::AddIntsService(int cpu): cpu_(cpu) {
    if (cpu != UNBOUND_CPU) { bindCPU(cpu); }
}

// Server function that handles parcels received from the client
status_t AddIntsService::onTransact(uint32_t code, const Parcel &data,
                                    Parcel* reply, uint32_t flags) {
    int val1, val2;
    status_t rv(0);
    int cpu;

    struct test_options options;
    options = server_options;

    // If server bound to a particular CPU, check that
    // were executing on that CPU.
    if (cpu_ != UNBOUND_CPU) {
        cpu = sched_getcpu();
        if (cpu != cpu_) {
            cerr << "server onTransact on CPU " << cpu << " expected CPU "
                  << cpu_ << endl;
            exit(20);
        }
    }

    // Perform the requested operation
    switch (code) {
    case ADD_INTS:
        if (options.payload_size == 0) {
            val1 = data.readInt32();
            val2 = data.readInt32();
            reply->writeInt32(val1 + val2);
        } else {
            val1 = data.readInt32();
            reply->writeInt32(val1);
        }
        break;

    default:
      cerr << "server onTransact unknown code, code: " << code << endl;
      exit(21);
    }

    return rv;
}


void server(struct test_options options)
{
    int rv;

    server_options = options;

    // Add the service
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    if ((rv = sm->addService(serviceName,
        new AddIntsService(options.server_cpu))) != 0) {
        cerr << "addService " << serviceName << " failed, rv: " << rv
            << " errno: " << errno << endl;
    }

    // Start threads to handle server work
    proc->startThreadPool();
}

void client(struct test_options options)
{
    int rv;
    client_options = options;
    sp<IServiceManager> sm = defaultServiceManager();
    double min = FLT_MAX, max = 0.0, total = 0.0; // Time in seconds for all
                                                  // the IPC calls.

    // If needed bind to client CPU
    if (options.client_cpu != UNBOUND_CPU) { bindCPU(options.client_cpu); }

    // Attach to service
    sp<IBinder> binder;
    do {
        binder = sm->getService(serviceName);
        if (binder != 0) break;
        cout << serviceName << " not published, waiting..." << endl;
        usleep(500000); // 0.5 s
    } while(true);

    // Perform the IPC operations
    for (unsigned int iter = 0; iter < options.iterations; iter++) {
        Parcel send, reply;
        int expected;

        if (options.payload_size == 0) {
            // Create parcel to be sent.  Will use the iteration cound
            // and the iteration count + 3 as the two integer values
            // to be sent.
            int val1 = iter;
            int val2 = iter + 3;
            expected = val1 + val2;  // Expect to get the sum back
            send.writeInt32(val1);
            send.writeInt32(val2);
        } else {
            expected = options.payload_size;
            char *buf = new char[expected + 1];
            fill(buf, buf + expected, 'a');
            buf[expected] = 0;
            send.writeInt32(strlen(buf));
            send.writeCString(buf);
        }

        // Send the parcel, while timing how long it takes for
        // the answer to return.
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        if ((rv = binder->transact(AddIntsService::ADD_INTS,
            send, &reply)) != 0) {
            cerr << "binder->transact failed, rv: " << rv
                << " errno: " << errno << endl;
            exit(10);
        }
        struct timespec current;
        clock_gettime(CLOCK_MONOTONIC, &current);

        // Calculate how long this operation took and update the stats
        struct timespec deltaTimespec = tsDelta(&start, &current);
        double delta = ts2double(&deltaTimespec);
        min = (delta < min) ? delta : min;
        max = (delta > max) ? delta : max;
        total += delta;
        int result = reply.readInt32();
        if (result != expected) {
            cerr << "Unexpected result for iteration " << iter << endl;
            cerr << "  result: " << result << endl;
            cerr << "expected: " << expected << endl;
        } else if (options.payload_size == 0) {
            cout << "#" << iter << " pass, result: " << result << " expected: " << expected << endl;
        }

        if (options.iter_delay > 0.0) { testDelaySpin(options.iter_delay); }
    }

    // Display the results
    cout << "Time per iteration min: " << min
        << " avg: " << (total / options.iterations)
        << " max: " << max
        << endl;
}

static ostream &operator<<(ostream &stream, const String16& str)
{
    for (unsigned int n1 = 0; n1 < str.size(); n1++) {
        if ((str[n1] > 0x20) && (str[n1] < 0x80)) {
            stream << (char) str[n1];
        } else {
            stream << '~';
        }
    }

    return stream;
}

static ostream &operator<<(ostream &stream, const cpu_set_t& set)
{
    for (unsigned int n1 = 0; n1 < CPU_SETSIZE; n1++) {
        if (CPU_ISSET(n1, &set)) {
            if (n1 != 0) { stream << ' '; }
            stream << n1;
        }
    }

    return stream;
}

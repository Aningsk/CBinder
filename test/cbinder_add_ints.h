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

/* Indicator for a thread not bound to a specific CPU */
#ifndef _CBINDER_ADD_INTS_H
#define _CBINDER_ADD_INTS_H

#define UNBOUND_CPU     -1 

struct test_options {
    int server_cpu; /* server CPU */
    int client_cpu; /* client CPU */
    unsigned int iterations; /* iterations */
    unsigned int payload_size; /* payload size */
    float iter_delay; /* end of iteration delay in seconds */
};

#endif

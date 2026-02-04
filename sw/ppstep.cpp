/* Test program for qapybara communication.
    Sends packed messages to a pulse programmer over ZeroMQ.
    Uses msgpack for serialization.
    
    Compile with:
      g++ -std=c++17 -o ppstep ppstep.cpp -lzmq -lmsgpack-c -lpthread -lrt
    
    Usage:
      ./ppstep -u tcp://<ip_address>:8710 [-n iterations] [-v] [-m mode] [-h]
    
    Options:
      -u url          ZeroMQ URL of the pulse programmer (e.g. tcp://<ip_address>:8710)
        -n iterations   Number of iterations to run (default: 100)
        -v              Verbose output
        -m mode         Test mode (default: 0). 0 = sequencer, 1 = dds
        -h              Show help message

   Author: Till Rosenband, QuEra
   Date:   January 2026
*/
#include <zmq.h>
#include <msgpack.h>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

using namespace std;

// for channel names, see embedded_sw/ppoly/ppoly_interpreter.cpp
const string digital_channel_name = "pulser";

const vector<string> dds_channel_names = {
    "raw_move_aodx", "raw_move_aody", "rydberg_420", "raman_laser"
};

const bool send_receive = true; // false for debugging packing only

void send_packed_request(void* requester, msgpack_sbuffer* sbuf, bool print_debug = false) {
    // Print message bytes (request)
    if (print_debug) {
        printf("Request bytes: ");
        for (size_t i = 0; i < sbuf->size; ++i) {
            unsigned char byte = static_cast<unsigned char>(sbuf->data[i]);
            printf("%02X ", byte);
        }
        printf("\n");
        printf("Request ASCII:  ");
        for (size_t i = 0; i < sbuf->size; ++i) {
            unsigned char byte = static_cast<unsigned char>(sbuf->data[i]);
            if (byte >= 32 && byte <= 126)
                printf("%c", byte);
            else
                printf("\\x%02X", byte);
        }
        printf("\n");
    }



    // Send serialized buffer
    if(send_receive)
        zmq_send(requester, sbuf->data, sbuf->size, 0);

    if(print_debug) {
        printf("Sent msgpack request (%zu bytes)\n", sbuf->size);

        /* deserializes it. */
        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);
        msgpack_unpack_next(&msg, sbuf->data, sbuf->size, NULL);

        /* prints the deserialized object. */
        msgpack_object obj = msg.data;
        msgpack_object_print(stdout, obj);
        printf("\n");
        msgpack_unpacked_destroy(&msg);
    }

    // Receive reply
    if(!send_receive)
        return;

    char buffer[1024];
    int recv_size = zmq_recv(requester, buffer, sizeof(buffer), 0);
    if(print_debug)
        printf("Received reply (%d bytes)\n", recv_size); 

    /* prints the deserialized object. */
    if(print_debug) {
        /* deserializes it. */
        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);
        msgpack_unpack_next(&msg, buffer, recv_size, NULL);
        msgpack_object obj = msg.data;

        msgpack_object_print(stdout, obj);
        printf("\n");

        msgpack_unpacked_destroy(&msg);
    }
}

void pack_str(msgpack_packer* pk, const char* str) {
    size_t len = strlen(str);
    msgpack_pack_str(pk, len);
    msgpack_pack_str_body(pk, str, len);
}

void pack_floats(msgpack_packer* pk, const std::vector<float>& values, bool as_array = true ) {
    if (as_array) {
        msgpack_pack_array(pk, values.size());
    }
    for (float v : values) {
        msgpack_pack_float(pk, v);
    }
}

void pack_ints(msgpack_packer* pk, const std::vector<int>& values, bool as_array = true) {
    if (as_array) {
        msgpack_pack_array(pk, values.size());
    }
    for (int v : values) {
        msgpack_pack_int32(pk, v);
    }
}

void call_method(void* requester, const string& method_name, bool print_debug = true) {
    if(print_debug)
        printf("Serializing and sending %s request...\n", method_name.c_str());

    // Prepare msgpack buffer and packer
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 2);
    pack_str(&pk, "call");
    pack_str(&pk, method_name.c_str());

    pack_str(&pk, "data");
    msgpack_pack_map(&pk, 0); // empty data

    // Send serialized buffer
    send_packed_request(requester, &sbuf, print_debug);

    msgpack_sbuffer_destroy(&sbuf);
}

void ppoly(msgpack_packer& pk, const string& channel, int n, 
           const vector<vector<float>>& values, bool print_debug = true) {
    if(print_debug) {
        printf("Adding ppoly request for channel %s[%d]...\n", channel.c_str(), n);
        printf("Values: [\n");
        for (const auto& vec : values) {
            printf(" [");
            for (float v : vec) {
                printf("%f ", v);
            }
            printf("]\n");
        }
        printf("]\n");
    }

    msgpack_pack_array(&pk, 2);
    pack_str(&pk, "ppoly");
    msgpack_pack_array(&pk, 2);

    msgpack_pack_array(&pk, 3);
    pack_str(&pk, channel.c_str());
    pack_ints(&pk, {0, n}, false);

    msgpack_pack_array(&pk, values.size());
    for (const auto& vec : values) {
        pack_floats(&pk, vec);
    }
}

void sequence(void* requester, bool print_debug = true) {
    if(print_debug)
        printf("Serializing and sending sequence request...\n");

    // Prepare msgpack buffer and packer
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 2);
    pack_str(&pk, "call");
    pack_str(&pk, "set_instructions");
    pack_str(&pk, "data");
    msgpack_pack_map(&pk, 1);
    pack_str(&pk, "instructions");

    unsigned int num_instructions = 4;
    msgpack_pack_array(&pk, num_instructions);

    double t[] = {0, 100, 200};
    vector<vector<float>> vals1 = {
        {t[0], 0.0, 0.0, 0, 0},
        {t[1], 15.0, 0.0, 0, 0},
        {t[2], 0.0, 0.0, 0, 0}
    };

    ppoly(pk, digital_channel_name.c_str(), 0, vals1, print_debug);
    
    vector<vector<float>> vals2 = {
        {t[0], 0.0, 0.0, 0, 0},
        {t[1], 1.0, 0.0, 0, 0},
        {t[2], 0.0, 0.0, 0, 0}
    };
    ppoly(pk, dds_channel_names[0].c_str(), 0, vals2, print_debug);

    vector<vector<float>> vals3 = {
        {t[0], 0.0, 0.0, 0, 0},
        {t[1], 80.0, 0.0, 0, 0},
        {t[2], 0.0, 0.0, 0, 0}
    };
    ppoly(pk, dds_channel_names[0].c_str(), 1, vals3, print_debug);

    // End of instructions array
    msgpack_pack_array(&pk, 2);
    pack_str(&pk, "wait_on_complete");
    msgpack_pack_array(&pk, 0);

    send_packed_request(requester, &sbuf, print_debug);
    
    msgpack_sbuffer_destroy(&sbuf);
}

void set_dds(void* requester,
    unsigned digital,
    const vector<float>& values,
    unsigned adj_type,
    bool print_debug = true) 
{
    if(print_debug)
        printf("Serializing and sending dds update request...\n");

    // Prepare msgpack buffer and packer
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, 2);
    pack_str(&pk, "call");
    pack_str(&pk, "set_instructions");
    pack_str(&pk, "data");
    msgpack_pack_map(&pk, 1);
    pack_str(&pk, "instructions");

    unsigned int num_instructions = 2 + dds_channel_names.size();

    msgpack_pack_array(&pk, num_instructions);

    double t[] = {0, 100, 200};
    vector<vector<float>> vals1 = {{t[0], digital, 0.0, 0, 0}};

    ppoly(pk, digital_channel_name.c_str(), 0, vals1, print_debug);

    for(unsigned i = 0; i < dds_channel_names.size(); i++) {
        ppoly(pk, dds_channel_names[i].c_str(), adj_type, {{t[0], values.at(i), 0.0, 0, 0}}, print_debug);
    }

    // End of instructions array
    msgpack_pack_array(&pk, 2);
    pack_str(&pk, "wait_on_complete");
    msgpack_pack_array(&pk, 0);

    send_packed_request(requester, &sbuf, print_debug);
    
    msgpack_sbuffer_destroy(&sbuf);
}

void print_usage(const char* prog_name) {
    printf("Usage: %s -u url [-n iterations] [-v] [-m mode]\n", prog_name);
    printf("  -u url          ZeroMQ URL of the pulse programmer (e.g. tcp://10.0.0.111:8710)\n");
    printf("  -n iterations   Number of iterations to run (default: 100)\n");
    printf("  -v              Verbose output\n");
    printf("  -m mode         Test mode (default: 0). 0 = sequencer, 1 = dds\n");
    printf("  -h              Show this help message\n");
}

int main(int argc, char* argv[])
{
    string url;
    int iterations = 100;
    bool verbose = false;
    int mode = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "u:n:hm:v")) != -1) {
        switch (opt) {
            case 'u':
                url = optarg;
                break;
            case 'n':
                iterations = atoi(optarg);
                break;
            case 'v':
                verbose = true;
                break;
            case 'm':
                mode = atoi(optarg);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if(url.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    printf("Connecting to %s\n", url.c_str());
    printf("Iterations: %d\n", iterations);

    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, url.c_str());

    call_method(requester, "list_methods", verbose);
    call_method(requester, "list_parameters", verbose);
    call_method(requester, "clear_commands", verbose);

    vector<float> f0 = {100.0, 100.0, 100.0, 100.0};
    vector<float> f1 = {2.0, 3.0, 3.0, 3.0};
    vector<float> p0 = {0.0, 90.0, 180.0, 270.0};
    vector<float> a0 = {1.0, 0.0, 0.0, 0.0};
    vector<float> a1 = {0.0, 0.0, 0.0, 0.0};

    set_dds(requester, 1, a0, 0, verbose);
    call_method(requester, "play_immediate", verbose);

    printf("\n\nStarting main loop...\n");
    struct timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    for(int i = 0; i < iterations; i++) {
        call_method(requester, "clear_commands", verbose);

        if(mode == 0) {
            sequence(requester, verbose);
        } else if(mode == 1) {
            if(i % 2 == 0)
                set_dds(requester, 15, f0, 1, verbose);
            else
                set_dds(requester, 0, f1, 1, verbose);
        }
        call_method(requester, "play_immediate", verbose);
        usleep(100);
    }
    gettimeofday(&t_end, NULL);
    long elapsed_us = (t_end.tv_sec - t_start.tv_sec) * 1000000L + (t_end.tv_usec - t_start.tv_usec);
    printf("Loop of %d iterations took %ld us (%.1f us per iteration)\n\n", iterations, elapsed_us, 
           (double)elapsed_us/iterations);
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}

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
    zmq_send(requester, sbuf->data, sbuf->size, 0);
    if(print_debug)
        printf("Sent msgpack request (%zu bytes)\n", sbuf->size);

    // Receive reply
    char buffer[1024];
    int recv_size = zmq_recv(requester, buffer, sizeof(buffer), 0);
    if(print_debug)
        printf("Received reply (%d bytes)\n", recv_size);

    // Unpack reply as map
    //unpack_reply(buffer, recv_size);
    /* deserializes it. */
    msgpack_unpacked msg;
    msgpack_unpacked_init(&msg);
    msgpack_unpack_return ret = msgpack_unpack_next(&msg, buffer, recv_size, NULL);

    /* prints the deserialized object. */
    if(print_debug) {
        msgpack_object obj = msg.data;
        msgpack_object_print(stdout, obj);
        printf("\n");
    }

    msgpack_unpacked_destroy(&msg);
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

void call_method(void* requester, const string& method_name, bool empty_data = false, bool print_debug = true) {
    if(print_debug)
        printf("Serializing and sending %s request...\n", method_name.c_str());

    // Prepare msgpack buffer and packer
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer pk;
    msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    msgpack_pack_map(&pk, empty_data ? 1 : 2);
    pack_str(&pk, "call");
    pack_str(&pk, method_name.c_str());

    if (!empty_data) {
        pack_str(&pk, "data");
        msgpack_pack_map(&pk, 0); // empty data
    }

    // Send serialized buffer
    send_packed_request(requester, &sbuf, print_debug);
    msgpack_sbuffer_destroy(&sbuf);
}

void ppoly(msgpack_packer& pk, const string& channel, int n, 
           const vector<vector<float>>& values, bool print_debug = true) {
    if(print_debug)
        printf("Adding ppoly request...\n");

    msgpack_pack_array(&pk, 2);
    pack_str(&pk, "ppoly");
    msgpack_pack_array(&pk, 2);

    msgpack_pack_array(&pk, 3);
    pack_str(&pk, channel.c_str());
    pack_ints(&pk, {0, n}, false);

    msgpack_pack_array(&pk, 3);
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

    vector<vector<float>> vals1 = {
        {0.0, 0.0, 0.0, 0, 0},
        {1.0, 15.0, 0.0, 0, 0},
        {2.0, 0.0, 0.0, 0, 0}
    };

    ppoly(pk, "pulser", 0, vals1, print_debug);
    
    vector<vector<float>> vals2 = {
        {0.0, 0.0, 0.0, 0, 0},
        {1.0, 1.0, 0.0, 0, 0},
        {2.0, 0.0, 0.0, 0, 0}
    };
    ppoly(pk, "raw_move_aodx", 0, vals2, print_debug);

    vector<vector<float>> vals3 = {
        {0.0, 0.0, 0.0, 0, 0},
        {1.0, 80.0, 0.0, 0, 0},
        {2.0, 0.0, 0.0, 0, 0}
    };
    ppoly(pk, "raw_move_aodx", 1, vals3, print_debug);

    // End of instructions array
    msgpack_pack_array(&pk, 2);
    pack_str(&pk, "wait_on_complete");
    msgpack_pack_array(&pk, 0);

    send_packed_request(requester, &sbuf, print_debug);
    if(print_debug) {
        /* deserializes it. */
        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);
        msgpack_unpack_return ret = msgpack_unpack_next(&msg, sbuf.data, sbuf.size, NULL);

        /* prints the deserialized object. */
        msgpack_object obj = msg.data;
        msgpack_object_print(stdout, obj);
        printf("\n");
        msgpack_unpacked_destroy(&msg);
    }
    msgpack_sbuffer_destroy(&sbuf);
}

void print_usage(const char* prog_name) {
    printf("Usage: %s -u url [-n iterations]\n", prog_name);
    printf("  -u url          ZeroMQ URL of the pulse programmer (e.g. tcp://10.0.0.111:8710)\n");
    printf("  -n iterations   Number of iterations to run (default: 100)\n");
}

int main(int argc, char* argv[])
{
    string url;

    int iterations = 100;
    int opt;
    while ((opt = getopt(argc, argv, "u:n:h")) != -1) {
        switch (opt) {
            case 'u':
                url = optarg;
                break;
            case 'n':
                iterations = atoi(optarg);
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

    call_method(requester, "list_methods");
    call_method(requester, "list_parameters");
    call_method(requester, "clear_commands", true);

    struct timeval t_start, t_end;
    gettimeofday(&t_start, NULL);
    for(int i = 0; i < iterations; i++) {
        sequence(requester, false);
        call_method(requester, "play_immediate", false, false);
        usleep(100);
    }
    gettimeofday(&t_end, NULL);
    long elapsed_us = (t_end.tv_sec - t_start.tv_sec) * 1000000L + (t_end.tv_usec - t_start.tv_usec);
    printf("Loop of %d iterations took %ld us (%.1f us per iteration)\n", iterations, elapsed_us, 
           (double)elapsed_us/iterations);
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}

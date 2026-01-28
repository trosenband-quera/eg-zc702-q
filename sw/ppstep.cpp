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

using namespace std;
string url = "tcp://10.0.0.111:8710";

void pack_str(msgpack_packer* pk, const char* str) {
    size_t len = strlen(str);
    msgpack_pack_str(pk, len);
    msgpack_pack_str_body(pk, str, len);
}

void unpack_map(const msgpack_object_map& map) {
    for (uint32_t i = 0; i < map.size; i++) {
        msgpack_object_kv* kv = &map.ptr[i];
        if (kv->key.type == MSGPACK_OBJECT_STR) {
            string key(kv->key.via.str.ptr, kv->key.via.str.size);
            cout << "Key: " << key << " - ";
            if (kv->val.type == MSGPACK_OBJECT_STR) {
                string val(kv->val.via.str.ptr, kv->val.via.str.size);
                cout << "Value: " << val << endl;
            } else if (kv->val.type == MSGPACK_OBJECT_ARRAY) {
                cout << "Value is an array of size " << kv->val.via.array.size << endl;
                for (uint32_t j = 0; j < kv->val.via.array.size; j++) {
                    msgpack_object elem = kv->val.via.array.ptr[j];
                    if (elem.type == MSGPACK_OBJECT_STR) {
                        string elem_str(elem.via.str.ptr, elem.via.str.size);
                        cout << "  Element " << j << ": " << elem_str << endl;
                    }
                }
            } else if (kv->val.type == MSGPACK_OBJECT_MAP) {
                cout << "Value is a map of size " << kv->val.via.map.size << endl;
                unpack_map(kv->val.via.map);
            } else {
                cout << "Value type: " << kv->val.type << endl;
            }
        }
    }
}

void unpack_reply(const char* buffer, size_t size) {
    msgpack_unpacked result;
    msgpack_unpacked_init(&result);
    size_t off = 0;
    msgpack_unpack_return ret = msgpack_unpack_next(&result, buffer, size, &off);
    if (ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_object obj = result.data;
        if (obj.type == MSGPACK_OBJECT_MAP) {
            cout << "Reply is a map with " << obj.via.map.size << " entries:" << endl;
            unpack_map(obj.via.map);
        } else {
            cout << "Reply is not a map, type: " << obj.type << endl;
        }
    } else {
        cout << "Failed to unpack reply: " << ret << endl;
    }
    msgpack_unpacked_destroy(&result);
}

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
    printf("Sent msgpack request (%zu bytes)\n", sbuf->size);

    // Receive reply
    char buffer[1024];
    int recv_size = zmq_recv(requester, buffer, sizeof(buffer), 0);
    printf("Received reply (%d bytes)\n", recv_size);

    // Unpack reply as map
    //unpack_reply(buffer, recv_size);
    /* deserializes it. */
    msgpack_unpacked msg;
    msgpack_unpacked_init(&msg);
    msgpack_unpack_return ret = msgpack_unpack_next(&msg, buffer, recv_size, NULL);

    /* prints the deserialized object. */
    msgpack_object obj = msg.data;
    msgpack_object_print(stdout, obj);
    printf("\n");
    msgpack_unpacked_destroy(&msg);
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
    send_packed_request(requester, &sbuf, false);
    msgpack_sbuffer_destroy(&sbuf);
}

void ppoly(msgpack_packer& pk, const string& channel, int n, 
           const vector<vector<float>>& values, bool print_debug = true) {
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

    ppoly(pk, "pulser", 0, vals1, true);
    
    vector<vector<float>> vals2 = {
        {0.0, 0.0, 0.0, 0, 0},
        {1.0, 5.0, 0.0, 0, 0},
        {2.0, 0.0, 0.0, 0, 0}
    };
    ppoly(pk, "raw_move_aodx", 0, vals2, true);

    vector<vector<float>> vals3 = {
        {0.0, 0.0, 0.0, 0, 0},
        {1.0, 80.0, 0.0, 0, 0},
        {2.0, 0.0, 0.0, 0, 0}
    };
    ppoly(pk, "raw_move_aodx", 1, vals3, true);

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
int main (void)
{
    printf ("Connecting to %s\n", url.c_str());
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, url.c_str());

    for(int i = 0; i < 100; i++) {
        call_method(requester, "list_methods");
        call_method(requester, "list_parameters");
        call_method(requester, "clear_commands", true);
        sequence(requester);
        call_method(requester, "play_immediate", true);
        usleep(1000);
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}

#include "client.h"
#include "coap_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>

static void test_path_builder(void) {
    CoapMessage msg; coap_message_init(&msg);
    int r = coap_add_path_segments(&msg, "/a/b");
    assert(r == 0);
    char path[64];
    int len = coap_message_get_uri_path(&msg, path, sizeof(path));
    assert(len > 0);
    assert(strcmp(path, "a/b") == 0);
    assert(msg.option_count >= 2);
    printf("✓ test_path_builder\n");
}

static void test_build_get_post(void) {
    CoapMessage get; assert(coap_build_get(&get, "/hello", false) == 0);
    assert(get.code == COAP_METHOD_GET);
    assert(get.type == COAP_TYPE_CONFIRMABLE);

    const char *payload = "abc";
    CoapMessage post; assert(coap_build_post(&post, "/echo", (const uint8_t *)payload, 3, true) == 0);
    assert(post.code == COAP_METHOD_POST);
    assert(post.type == COAP_TYPE_NON_CONFIRMABLE);
    assert(post.payload && post.payload_length == 3);
    printf("✓ test_build_get_post\n");
}

static void child_server_loop(int sock) {
    uint8_t in[COAP_MAX_MESSAGE_SIZE];
    struct sockaddr_storage src; socklen_t slen = (socklen_t)sizeof(src);
    ssize_t r = recvfrom(sock, in, sizeof(in), 0, (struct sockaddr *)&src, &slen);
    if (r <= 0) _exit(0);

    CoapMessage req; coap_message_init(&req);
    if (coap_decode(&req, in, (size_t)r) != 0) _exit(0);

    // Construir respuesta 2.05 Content 'hello'
    CoapMessage resp; coap_message_init(&resp);
    resp.version = COAP_VERSION;
    resp.message_id = req.message_id;
    resp.token_length = req.token_length;
    if (req.token_length > 0) memcpy(resp.token, req.token, req.token_length);
    resp.type = (req.type == COAP_TYPE_CONFIRMABLE) ? COAP_TYPE_ACKNOWLEDGMENT : COAP_TYPE_NON_CONFIRMABLE;
    resp.code = COAP_RESPONSE_CONTENT;
    const char *msg = "hello";
    memcpy(resp.payload_buffer, msg, 5);
    resp.payload = resp.payload_buffer;
    resp.payload_length = 5;

    uint8_t out[COAP_MAX_MESSAGE_SIZE];
    int out_n = coap_encode(&resp, out, sizeof(out));
    if (out_n <= 0) _exit(0);
    sendto(sock, out, (size_t)out_n, 0, (struct sockaddr *)&src, slen);
    _exit(0);
}

static void test_client_send_with_fork(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(sock >= 0);
    int opt = 1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons(0);
    assert(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);

    socklen_t alen = sizeof(addr);
    assert(getsockname(sock, (struct sockaddr *)&addr, &alen) == 0);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        // Child: simple one-shot server
        child_server_loop(sock);
    }

    // Parent: build client request to child's port
    CoapMessage req; assert(coap_build_get(&req, "/hello", false) == 0);
    req.message_id = 0x55AA; req.token_length = 1; req.token[0] = 0x77;

    CoapClientConfig cfg = {
        .host = "127.0.0.1",
        .port = port,
        .timeout_ms = 1000,
        .retries = 0,
        .non = false,
        .verbose = true,
    };

    CoapMessage resp; coap_message_init(&resp);
    int rc = coap_client_send(&cfg, &req, &resp);
    assert(rc == 0);
    assert(resp.code == COAP_RESPONSE_CONTENT);
    assert(resp.payload && resp.payload_length == 5);
    assert(memcmp(resp.payload, "hello", 5) == 0);

    close(sock);
    printf("✓ test_client_send_with_fork\n");
}

int main(void) {
    printf("=== TeleClient tests ===\n");
    test_path_builder();
    test_build_get_post();
    test_client_send_with_fork();
    printf("✓ All TeleClient tests passed\n");
    return 0;
}

#include "coap.h"
#include "coap_codec.h"
#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --host HOST --port N --method GET|POST --path /p [--payload STR] [--timeout MS] [--retries N] [--non] [--verbose]\n",
        prog);
}

int main(int argc, char *argv[]) {
    const char *host = NULL;
    int port = -1;
    const char *method_str = NULL;
    const char *path = NULL;
    const char *payload = NULL;
    int timeout_ms = 1000;
    int retries = 2;
    bool non = false;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) host = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) method_str = argv[++i];
        else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) path = argv[++i];
        else if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) payload = argv[++i];
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) timeout_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--retries") == 0 && i + 1 < argc) retries = atoi(argv[++i]);
        else if (strcmp(argv[i], "--non") == 0) non = true;
        else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        else { usage(argv[0]); return 1; }
    }

    if (!host || port <= 0 || !method_str || !path) { usage(argv[0]); return 1; }

    CoapCode method;
    if (strcmp(method_str, "GET") == 0) method = COAP_METHOD_GET;
    else if (strcmp(method_str, "POST") == 0) method = COAP_METHOD_POST;
    else { fprintf(stderr, "Unsupported method: %s\n", method_str); return 1; }

    // Construir mensaje
    srand((unsigned)time(NULL));
    CoapMessage req; coap_message_init(&req);
    CoapMessage resp; coap_message_init(&resp);

    if (method == COAP_METHOD_GET) {
        if (coap_build_get(&req, path, non) != 0) { fprintf(stderr, "build GET failed\n"); return 1; }
    } else {
        size_t n = payload ? strlen(payload) : 0;
        if (coap_build_post(&req, path, (const uint8_t *)payload, n, non) != 0) {
            fprintf(stderr, "build POST failed\n"); return 1;
        }
    }

    // Generar ID y token
    req.message_id = (uint16_t)(rand() & 0xFFFF);
    req.token_length = 1; req.token[0] = (uint8_t)(rand() & 0xFF);

    CoapClientConfig cfg = {
        .host = host,
        .port = (uint16_t)port,
        .timeout_ms = timeout_ms,
        .retries = retries,
        .non = non,
        .verbose = verbose,
    };

    int rc = coap_client_send(&cfg, &req, &resp);
    if (rc != 0) return 1;

    const char *code_str = coap_code_to_string(resp.code);
    printf("%s\n", code_str ? code_str : "");
    if (resp.payload && resp.payload_length > 0) {
        fwrite(resp.payload, 1, resp.payload_length, stdout);
        printf("\n");
    }
    return 0;
}

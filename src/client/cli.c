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

static bool is_json_content(const CoapMessage *msg) {
    const CoapOptionDef *opt = coap_message_find_option(msg, COAP_OPTION_CONTENT_FORMAT);
    if (!opt || opt->length == 0) return false;
    // application/json = 50
    if (opt->length == 1 && opt->value[0] == 50) return true;
    return false;
}

static const char *find_key_after(const char *s, const char *end, const char *key) {
    // Busca "key" : y devuelve puntero al primer char después de ':'
    while (s && s < end) {
        const char *p = strstr(s, key);
        if (!p || p >= end) return NULL;
        const char *q = strchr(p, ':');
        if (!q || q >= end) return NULL;
        return q + 1;
    }
    return NULL;
}

static bool parse_double_at(const char *s, const char *end, double *out, const char **newpos) {
    if (!s || s >= end) return false;
    while (s < end && (*s == ' ' || *s == '\t' || *s == '"')) s++;
    char *ep = NULL;
    double v = strtod(s, &ep);
    if (ep == s || !ep || ep > end) return false;
    *out = v; *newpos = ep; return true;
}

static bool parse_ull_at(const char *s, const char *end, unsigned long long *out, const char **newpos) {
    if (!s || s >= end) return false;
    while (s < end && (*s == ' ' || *s == '\t' || *s == '"')) s++;
    char *ep = NULL;
    unsigned long long v = strtoull(s, &ep, 10);
    if (ep == s || !ep || ep > end) return false;
    *out = v; *newpos = ep; return true;
}

static void print_table_header(void) {
    printf("%-16s | %12s | %10s | %8s | %16s\n",
           "TIMESTAMP(ms)", "TEMPERATURA", "HUMEDAD", "VOLTAJE", "CANTIDAD_PROD");
    printf("-----------------+--------------+------------+----------+----------------\n");
}

static void print_table_row(unsigned long long ts,
                            double temp, double hum, double volt,
                            double cant) {
    printf("%16llu | %12.2f | %10.2f | %8.2f | %16.2f\n",
           ts, temp, hum, volt, cant);
}

static void render_telemetry_table(const uint8_t *payload, size_t n) {
    // Copiamos en buffer NUL-terminado para búsquedas seguras
    char *buf = (char *)malloc(n + 1);
    if (!buf) { fwrite(payload, 1, n, stdout); printf("\n"); return; }
    memcpy(buf, payload, n); buf[n] = '\0';
    const char *s = buf; const char *end = buf + n;

    print_table_header();

    while (s < end) {
        const char *data_key = strstr(s, "\"data\"");
        if (!data_key) break;
        const char *data_obj = strchr(data_key, '{');
        if (!data_obj) break;
        // Encontrar cierre del objeto data (contar llaves)
        int depth = 0; const char *p = data_obj; const char *data_end = NULL;
        for (; p < end; p++) {
            if (*p == '{') depth++;
            else if (*p == '}') { depth--; if (depth == 0) { data_end = p + 1; break; } }
        }
        if (!data_end) break;

        double temp = 0.0, hum = 0.0, volt = 0.0, cant = 0.0;
        bool ok_temp=false, ok_hum=false, ok_volt=false, ok_cant=false;

        const char *pos;
        pos = find_key_after(data_obj, data_end, "\"temperatura\"");
        if (pos) ok_temp = parse_double_at(pos, data_end, &temp, &pos);
        pos = find_key_after(data_obj, data_end, "\"humedad\"");
        if (pos) ok_hum = parse_double_at(pos, data_end, &hum, &pos);
        pos = find_key_after(data_obj, data_end, "\"voltaje\"");
        if (pos) ok_volt = parse_double_at(pos, data_end, &volt, &pos);
        pos = find_key_after(data_obj, data_end, "\"cantidad_producida\"");
        if (pos) ok_cant = parse_double_at(pos, data_end, &cant, &pos);

        // Buscar timestamp en el objeto externo posterior a data
        const char *ts_key = strstr(data_end, "\"timestamp\"");
        unsigned long long ts = 0ULL; const char *npos = NULL; bool ok_ts=false;
        if (ts_key) {
            const char *ts_val = strchr(ts_key, ':');
            if (ts_val) ok_ts = parse_ull_at(ts_val + 1, end, &ts, &npos);
        }

        // Imprimir fila (si faltan valores, se verán como 0.00)
        print_table_row(ok_ts ? ts : 0ULL,
                        ok_temp ? temp : 0.0,
                        ok_hum ? hum : 0.0,
                        ok_volt ? volt : 0.0,
                        ok_cant ? cant : 0.0);

        // Continuar búsqueda después del timestamp si existió, si no después de data_end
        s = (npos && npos < end) ? npos : data_end;
    }

    free(buf);
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
        // Si estamos consultando /api/v1/telemetry y la respuesta es JSON, renderizar tabla
        if (method == COAP_METHOD_GET && path &&
            (strcmp(path, "/api/v1/telemetry") == 0 || strcmp(path, "api/v1/telemetry") == 0) &&
            is_json_content(&resp)) {
            render_telemetry_table(resp.payload, resp.payload_length);
        } else {
            fwrite(resp.payload, 1, resp.payload_length, stdout);
            printf("\n");
        }
    }
    return 0;
}

#include "client.h"
#include "coap_codec.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

static int resolve_udp(const char *host, const char *port_str, struct addrinfo **out) {
	struct addrinfo hints; memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	return getaddrinfo(host, port_str, &hints, out);
}

static void vlogf(bool verbose, const char *fmt, ...) {
	if (!verbose) return;
	va_list ap; va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

int coap_add_path_segments(CoapMessage *msg, const char *path) {
	if (!msg || !path) return -1;
	const char *p = path;
	while (*p == '/') p++;
	const char *start = p;
	for (;;) {
		const char *slash = strchr(start, '/');
		size_t len = slash ? (size_t)(slash - start) : strlen(start);
		if (len > 0) {
			if (coap_message_add_option(msg, COAP_OPTION_URI_PATH,
									 (const uint8_t *)start, len) != 0) {
				return -1;
			}
		}
		if (!slash) break;
		start = slash + 1;
	}
	return 0;
}

int coap_build_get(CoapMessage *req, const char *path, bool non) {
	if (!req || !path) return -1;
	coap_message_init(req);
	req->type = non ? COAP_TYPE_NON_CONFIRMABLE : COAP_TYPE_CONFIRMABLE;
	req->code = COAP_METHOD_GET;
	if (coap_add_path_segments(req, path) != 0) return -1;
	return 0;
}

int coap_build_post(CoapMessage *req, const char *path,
					  const uint8_t *payload, size_t n, bool non) {
	if (!req || !path) return -1;
	coap_message_init(req);
	req->type = non ? COAP_TYPE_NON_CONFIRMABLE : COAP_TYPE_CONFIRMABLE;
	req->code = COAP_METHOD_POST;
	if (coap_add_path_segments(req, path) != 0) return -1;
	if (payload && n > 0) {
		if (n > sizeof(req->payload_buffer)) return -1;
		memcpy(req->payload_buffer, payload, n);
		req->payload = req->payload_buffer;
		req->payload_length = n;
		// Content-Format text/plain (0) => longitud 0
		(void)coap_message_add_option(req, COAP_OPTION_CONTENT_FORMAT, NULL, 0);
	}
	return 0;
}

int coap_client_send(const CoapClientConfig *cfg,
					 const CoapMessage *req,
					 CoapMessage *resp) {
	if (!cfg || !req || !resp) return -1;
	char port_str[16]; snprintf(port_str, sizeof(port_str), "%u", (unsigned)cfg->port);

	struct addrinfo *ai = NULL;
	int gai = resolve_udp(cfg->host, port_str, &ai);
	if (gai != 0 || !ai) {
		vlogf(cfg->verbose, "resolve failed: %s\n", gai_strerror(gai));
		return -1;
	}

	uint8_t out[COAP_MAX_MESSAGE_SIZE];
	int out_n = coap_encode(req, out, sizeof(out));
	if (out_n <= 0) { freeaddrinfo(ai); return -1; }

	int rc = -1;
	for (int attempt = 0; attempt <= cfg->retries; attempt++) {
		vlogf(cfg->verbose, "Attempt %d/%d...\n", attempt + 1, cfg->retries + 1);

		int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) { rc = -1; break; }

		struct timeval tv; tv.tv_sec = cfg->timeout_ms / 1000; tv.tv_usec = (cfg->timeout_ms % 1000) * 1000;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		ssize_t sent = sendto(s, out, (size_t)out_n, 0, ai->ai_addr, ai->ai_addrlen);
		if (sent < 0 || (size_t)sent != (size_t)out_n) { close(s); continue; }

		uint8_t in[COAP_MAX_MESSAGE_SIZE];
		struct sockaddr_storage src; socklen_t slen = (socklen_t)sizeof(src);
		ssize_t r = recvfrom(s, in, sizeof(in), 0, (struct sockaddr *)&src, &slen);
		close(s);
		if (r <= 0) { vlogf(cfg->verbose, "timeout or recv error\n"); continue; }

		coap_message_init(resp);
		if (coap_decode(resp, in, (size_t)r) != 0) { vlogf(cfg->verbose, "decode failed\n"); continue; }
		if (resp->message_id != req->message_id || resp->token_length != req->token_length ||
			(resp->token_length > 0 && memcmp(resp->token, req->token, resp->token_length) != 0)) {
			vlogf(cfg->verbose, "mismatched token/id\n");
			continue;
		}
		rc = 0; break;
	}

	freeaddrinfo(ai);
	return rc;
}

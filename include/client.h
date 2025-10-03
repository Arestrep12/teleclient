#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "coap.h"

typedef struct {
	const char *host;
	uint16_t port;
	int timeout_ms; // por intento
	int retries;    // reintentos adicionales (total = retries+1)
	bool non;       // enviar como NON en lugar de CON
	bool verbose;   // logs a stderr
} CoapClientConfig;

// Envia una request CoAP y obtiene una respuesta (0 si OK, <0 si error/timeout)
int coap_client_send(const CoapClientConfig *cfg,
					 const CoapMessage *req,
					 CoapMessage *resp);

// Helpers para construir requests
int coap_build_get(CoapMessage *req, const char *path, bool non);
int coap_build_post(CoapMessage *req, const char *path,
				   const uint8_t *payload, size_t n, bool non);

// Helper para añadir opciones Uri-Path desde una ruta tipo "/a/b"
int coap_add_path_segments(CoapMessage *msg, const char *path);

#endif // CLIENT_H

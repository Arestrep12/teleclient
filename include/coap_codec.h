#ifndef COAP_CODEC_H
#define COAP_CODEC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "coap.h"

// Resultados/errores del codec
//  0  => OK
// <0  => error
// Nota: preferimos enteros negativos como códigos de error para coherencia
// con otras funciones platform_ que retornan PLATFORM_ERROR, etc.
typedef enum {
	COAP_CODEC_OK = 0,
	COAP_CODEC_EINVAL = -1,     // Parámetro inválido / versión/tkl inválidos
	COAP_CODEC_E2SMALL = -2,    // Buffer de salida insuficiente
	COAP_CODEC_EMALFORMED = -3, // Mensaje entrante mal formado
	COAP_CODEC_EOPTIONS = -4    // Error relacionado a opciones (orden/longitud/cantidad)
} CoapCodecResult;

// Decodifica un datagrama CoAP en "msg".
// - buffer: bytes del mensaje (datagrama UDP)
// - length: tamaño del buffer
// Retorna 0 en éxito o < 0 en error.
int coap_decode(CoapMessage *msg, const uint8_t *buffer, size_t length);

// Codifica un CoapMessage en el buffer de salida.
// - msg: mensaje a serializar
// - out: buffer destino
// - out_size: capacidad del buffer
// Retorna cantidad de bytes escritos (>0) o <0 en error (p.ej., COAP_CODEC_E2SMALL).
int coap_encode(const CoapMessage *msg, uint8_t *out, size_t out_size);

// Validación ligera previa al encode
bool coap_message_can_encode(const CoapMessage *msg);

#endif // COAP_CODEC_H

#ifndef COAP_H
#define COAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Constantes del protocolo CoAP (RFC 7252)
#define COAP_VERSION 1
#define COAP_DEFAULT_PORT 5683
#define COAP_MAX_TOKEN_LENGTH 8
#define COAP_MAX_OPTION_VALUE_LENGTH 270
#define COAP_PAYLOAD_MARKER 0xFF
#define COAP_MAX_MESSAGE_SIZE 1472

// Tipos de mensaje
typedef enum {
    COAP_TYPE_CONFIRMABLE = 0,      // CON
    COAP_TYPE_NON_CONFIRMABLE = 1,  // NON
    COAP_TYPE_ACKNOWLEDGMENT = 2,   // ACK
    COAP_TYPE_RESET = 3             // RST
} CoapType;

// Códigos de método/respuesta (class.detail)
typedef enum {
    // Métodos (0.xx)
    COAP_METHOD_GET = 1,        // 0.01
    COAP_METHOD_POST = 2,       // 0.02
    COAP_METHOD_PUT = 3,        // 0.03
    COAP_METHOD_DELETE = 4,     // 0.04

    // Respuestas exitosas (2.xx)
    COAP_RESPONSE_CREATED = 65,      // 2.01
    COAP_RESPONSE_DELETED = 66,      // 2.02
    COAP_RESPONSE_VALID = 67,        // 2.03
    COAP_RESPONSE_CHANGED = 68,      // 2.04
    COAP_RESPONSE_CONTENT = 69,      // 2.05

    // Errores del cliente (4.xx)
    COAP_ERROR_BAD_REQUEST = 128,          // 4.00
    COAP_ERROR_UNAUTHORIZED = 129,         // 4.01
    COAP_ERROR_BAD_OPTION = 130,           // 4.02
    COAP_ERROR_FORBIDDEN = 131,            // 4.03
    COAP_ERROR_NOT_FOUND = 132,            // 4.04
    COAP_ERROR_METHOD_NOT_ALLOWED = 133,   // 4.05
    COAP_ERROR_NOT_ACCEPTABLE = 134,       // 4.06

    // Errores del servidor (5.xx)
    COAP_ERROR_INTERNAL = 160,             // 5.00
    COAP_ERROR_NOT_IMPLEMENTED = 161,      // 5.01
    COAP_ERROR_BAD_GATEWAY = 162,          // 5.02
    COAP_ERROR_SERVICE_UNAVAILABLE = 163,  // 5.03
    COAP_ERROR_GATEWAY_TIMEOUT = 164,      // 5.04
    COAP_ERROR_PROXYING_NOT_SUPPORTED = 165 // 5.05
} CoapCode;

// Números de opción
typedef enum {
    COAP_OPTION_IF_MATCH = 1,
    COAP_OPTION_URI_HOST = 3,
    COAP_OPTION_ETAG = 4,
    COAP_OPTION_IF_NONE_MATCH = 5,
    COAP_OPTION_URI_PORT = 7,
    COAP_OPTION_LOCATION_PATH = 8,
    COAP_OPTION_URI_PATH = 11,
    COAP_OPTION_CONTENT_FORMAT = 12,
    COAP_OPTION_MAX_AGE = 14,
    COAP_OPTION_URI_QUERY = 15,
    COAP_OPTION_ACCEPT = 17,
    COAP_OPTION_LOCATION_QUERY = 20,
    COAP_OPTION_PROXY_URI = 35,
    COAP_OPTION_PROXY_SCHEME = 39,
    COAP_OPTION_SIZE1 = 60
} CoapOption;

// Formatos de contenido
typedef enum {
    COAP_FORMAT_TEXT = 0,     // text/plain; charset=utf-8
    COAP_FORMAT_LINK = 40,    // application/link-format
    COAP_FORMAT_XML = 41,     // application/xml
    COAP_FORMAT_OCTET = 42,   // application/octet-stream
    COAP_FORMAT_EXI = 47,     // application/exi
    COAP_FORMAT_JSON = 50,    // application/json
    COAP_FORMAT_CBOR = 60     // application/cbor
} CoapContentFormat;

// Estructura de una opción CoAP
typedef struct {
    uint16_t number;
    uint16_t length;
    uint8_t value[COAP_MAX_OPTION_VALUE_LENGTH];
} CoapOptionDef;

// Mensaje CoAP (perfil mínimo)
typedef struct {
    // Header fijo
    uint8_t version;
    CoapType type;
    uint8_t token_length;
    CoapCode code;
    uint16_t message_id;

    // Token (0-8 bytes)
    uint8_t token[COAP_MAX_TOKEN_LENGTH];

    // Opciones (ordenadas por número ascendente)
    CoapOptionDef options[16];
    size_t option_count;

    // Payload
    uint8_t *payload;
    size_t payload_length;

    // Buffer interno para payload (opcional)
    uint8_t payload_buffer[COAP_MAX_MESSAGE_SIZE];
} CoapMessage;

// Utilidades
const char *coap_type_to_string(CoapType type);
const char *coap_code_to_string(CoapCode code);
const char *coap_option_to_string(uint16_t option_number);
uint8_t coap_code_class(CoapCode code);
uint8_t coap_code_detail(CoapCode code);
CoapCode coap_make_code(uint8_t class_code, uint8_t detail);

// Inicialización y limpieza
void coap_message_init(CoapMessage *msg);
void coap_message_clear(CoapMessage *msg);

// Opciones
int coap_message_add_option(CoapMessage *msg, uint16_t number,
                            const uint8_t *value, size_t length);
const CoapOptionDef *coap_message_find_option(const CoapMessage *msg, uint16_t number);
int coap_message_get_uri_path(const CoapMessage *msg, char *buffer, size_t buffer_size);

// Validación básica
bool coap_message_is_valid(const CoapMessage *msg);
bool coap_message_is_request(const CoapMessage *msg);
bool coap_message_is_response(const CoapMessage *msg);

#endif // COAP_H

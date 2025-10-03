#include "coap.h"
#include <string.h>

const char *coap_type_to_string(CoapType type) {
    switch (type) {
        case COAP_TYPE_CONFIRMABLE: return "CON";
        case COAP_TYPE_NON_CONFIRMABLE: return "NON";
        case COAP_TYPE_ACKNOWLEDGMENT: return "ACK";
        case COAP_TYPE_RESET: return "RST";
        default: return "UNKNOWN";
    }
}

const char *coap_code_to_string(CoapCode code) {
    switch (code) {
        case COAP_METHOD_GET: return "GET";
        case COAP_METHOD_POST: return "POST";
        case COAP_METHOD_PUT: return "PUT";
        case COAP_METHOD_DELETE: return "DELETE";
        case COAP_RESPONSE_CREATED: return "2.01 Created";
        case COAP_RESPONSE_DELETED: return "2.02 Deleted";
        case COAP_RESPONSE_VALID: return "2.03 Valid";
        case COAP_RESPONSE_CHANGED: return "2.04 Changed";
        case COAP_RESPONSE_CONTENT: return "2.05 Content";
        case COAP_ERROR_BAD_REQUEST: return "4.00 Bad Request";
        case COAP_ERROR_UNAUTHORIZED: return "4.01 Unauthorized";
        case COAP_ERROR_NOT_FOUND: return "4.04 Not Found";
        case COAP_ERROR_METHOD_NOT_ALLOWED: return "4.05 Method Not Allowed";
        case COAP_ERROR_INTERNAL: return "5.00 Internal Server Error";
        case COAP_ERROR_NOT_IMPLEMENTED: return "5.01 Not Implemented";
        default: return "Unknown";
    }
}

const char *coap_option_to_string(uint16_t option_number) {
    switch (option_number) {
        case COAP_OPTION_URI_PATH: return "Uri-Path";
        case COAP_OPTION_CONTENT_FORMAT: return "Content-Format";
        case COAP_OPTION_URI_QUERY: return "Uri-Query";
        case COAP_OPTION_ACCEPT: return "Accept";
        default: return "Unknown";
    }
}

uint8_t coap_code_class(CoapCode code) {
    return (uint8_t)((code >> 5) & 0x07);
}

uint8_t coap_code_detail(CoapCode code) {
    return (uint8_t)(code & 0x1F);
}

CoapCode coap_make_code(uint8_t class_code, uint8_t detail) {
    return (CoapCode)((class_code << 5) | (detail & 0x1F));
}

void coap_message_init(CoapMessage *msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(CoapMessage));
    msg->version = COAP_VERSION;
    msg->type = COAP_TYPE_CONFIRMABLE;
}

void coap_message_clear(CoapMessage *msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(CoapMessage));
}

int coap_message_add_option(CoapMessage *msg, uint16_t number,
                            const uint8_t *value, size_t length) {
    if (!msg || msg->option_count >= (sizeof msg->options / sizeof msg->options[0])) {
        return -1;
    }
    if (length > COAP_MAX_OPTION_VALUE_LENGTH) {
        return -1;
    }

    // Insertar en orden
    size_t insert_pos = msg->option_count;
    for (size_t i = 0; i < msg->option_count; i++) {
        if (msg->options[i].number > number) {
            insert_pos = i;
            break;
        }
    }
    if (insert_pos < msg->option_count) {
        memmove(&msg->options[insert_pos + 1], &msg->options[insert_pos],
                (msg->option_count - insert_pos) * sizeof(CoapOptionDef));
    }

    msg->options[insert_pos].number = number;
    msg->options[insert_pos].length = (uint16_t)length;
    if (value && length > 0) {
        memcpy(msg->options[insert_pos].value, value, length);
    }
    msg->option_count++;
    return 0;
}

const CoapOptionDef *coap_message_find_option(const CoapMessage *msg, uint16_t number) {
    if (!msg) return NULL;
    for (size_t i = 0; i < msg->option_count; i++) {
        if (msg->options[i].number == number) {
            return &msg->options[i];
        }
    }
    return NULL;
}

int coap_message_get_uri_path(const CoapMessage *msg, char *buffer, size_t buffer_size) {
    if (!msg || !buffer || buffer_size == 0) {
        return -1;
    }

    buffer[0] = '\0';
    size_t offset = 0;
    for (size_t i = 0; i < msg->option_count; i++) {
        if (msg->options[i].number == COAP_OPTION_URI_PATH) {
            if (offset > 0) {
                if (offset + 1 < buffer_size) {
                    buffer[offset++] = '/';
                }
            }
            size_t copy_len = msg->options[i].length;
            if (offset + copy_len >= buffer_size) {
                copy_len = buffer_size - offset - 1;
            }
            memcpy(buffer + offset, msg->options[i].value, copy_len);
            offset += copy_len;
        }
    }
    if (offset >= buffer_size) {
        buffer[buffer_size - 1] = '\0';
        return (int)(buffer_size - 1);
    }
    buffer[offset] = '\0';
    return (int)offset;
}

bool coap_message_is_valid(const CoapMessage *msg) {
    if (!msg) return false;
    if (msg->version != COAP_VERSION) return false;
    if (msg->type > COAP_TYPE_RESET) return false;
    if (msg->token_length > COAP_MAX_TOKEN_LENGTH) return false;

    for (size_t i = 1; i < msg->option_count; i++) {
        if (msg->options[i].number < msg->options[i - 1].number) {
            return false;
        }
    }
    return true;
}

bool coap_message_is_request(const CoapMessage *msg) {
    if (!msg) return false;
    uint8_t cls = coap_code_class(msg->code);
    return cls == 0 && msg->code != 0;
}

bool coap_message_is_response(const CoapMessage *msg) {
    if (!msg) return false;
    uint8_t cls = coap_code_class(msg->code);
    return cls >= 2 && cls <= 5;
}

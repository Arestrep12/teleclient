#include "coap_codec.h"
#include <string.h>

static bool options_are_ordered(const CoapMessage *msg) {
	if (!msg) return false;
	for (size_t i = 1; i < msg->option_count; i++) {
		if (msg->options[i].number < msg->options[i - 1].number) return false;
	}
	return true;
}

bool coap_message_can_encode(const CoapMessage *msg) {
	if (!msg) return false;
	if (msg->version != COAP_VERSION) return false;
	if (msg->type > COAP_TYPE_RESET) return false;
	if (msg->token_length > COAP_MAX_TOKEN_LENGTH) return false;
	if (msg->option_count > (sizeof msg->options / sizeof msg->options[0])) return false;
	if (!options_are_ordered(msg)) return false;
	for (size_t i = 0; i < msg->option_count; i++) {
		if (msg->options[i].length > COAP_MAX_OPTION_VALUE_LENGTH) return false;
	}
	return true;
}

static int write_extended(uint32_t value,
                          uint8_t **p, uint8_t *end, uint8_t *out_nibble) {
	// value ya calculado (delta o length).
	// Decide nibble y escribe bytes extra si aplica.
	if (!p || !out_nibble) return COAP_CODEC_EINVAL;
	uint8_t *ptr = *p;
	if (value <= 12) {
		*out_nibble = (uint8_t)value;
		return COAP_CODEC_OK;
	} else if (value <= 13u + 255u) {
		*out_nibble = 13;
		if (!ptr || (ptr + 1) > end) return COAP_CODEC_E2SMALL;
		uint8_t ext = (uint8_t)(value - 13u);
		*ptr++ = ext;
		*p = ptr;
		return COAP_CODEC_OK;
	} else if (value <= 269u + 65535u) {
		*out_nibble = 14;
		if (!ptr || (ptr + 2) > end) return COAP_CODEC_E2SMALL;
		uint16_t ext = (uint16_t)(value - 269u);
		*ptr++ = (uint8_t)((ext >> 8) & 0xFF);
		*ptr++ = (uint8_t)(ext & 0xFF);
		*p = ptr;
		return COAP_CODEC_OK;
	}
	return COAP_CODEC_EOPTIONS; // Demasiado grande para representarse
}

int coap_encode(const CoapMessage *msg, uint8_t *out, size_t out_size) {
	if (!msg || !out) return COAP_CODEC_EINVAL;
	if (!coap_message_can_encode(msg)) return COAP_CODEC_EINVAL;

	uint8_t *p = out;
	uint8_t *end = out + out_size;
	if ((size_t)(end - p) < 4) return COAP_CODEC_E2SMALL;

	// Header
	uint8_t b0 = (uint8_t)((COAP_VERSION & 0x03) << 6);
	b0 |= (uint8_t)(((uint8_t)msg->type & 0x03) << 4);
	b0 |= (uint8_t)(msg->token_length & 0x0F);
	*p++ = b0;
	*p++ = (uint8_t)msg->code;
	*p++ = (uint8_t)((msg->message_id >> 8) & 0xFF);
	*p++ = (uint8_t)(msg->message_id & 0xFF);

	// Token
	if (msg->token_length > 0) {
		if ((size_t)(end - p) < msg->token_length) return COAP_CODEC_E2SMALL;
		memcpy(p, msg->token, msg->token_length);
		p += msg->token_length;
	}

	// Opciones
	uint32_t last = 0;
	for (size_t i = 0; i < msg->option_count; i++) {
		uint16_t number = msg->options[i].number;
		uint16_t length = msg->options[i].length;
		const uint8_t *value = msg->options[i].value;

		uint32_t delta = (uint32_t)number - last;
		uint8_t delta_nibble = 0, len_nibble = 0;

		if ((size_t)(end - p) < 1) return COAP_CODEC_E2SMALL;
		uint8_t *opt_start = p; // reservamos el byte inicial
		p++;

		int rc;
		rc = write_extended(delta, &p, end, &delta_nibble);
		if (rc != COAP_CODEC_OK) return rc;
		rc = write_extended(length, &p, end, &len_nibble);
		if (rc != COAP_CODEC_OK) return rc;

		opt_start[0] = (uint8_t)((delta_nibble << 4) | (len_nibble & 0x0F));

		if ((size_t)(end - p) < length) return COAP_CODEC_E2SMALL;
		if (length > 0) {
			memcpy(p, value, length);
			p += length;
		}
		last = number;
	}

	// Payload
	if (msg->payload && msg->payload_length > 0) {
		if ((size_t)(end - p) < (1 + msg->payload_length)) return COAP_CODEC_E2SMALL;
		*p++ = COAP_PAYLOAD_MARKER;
		memcpy(p, msg->payload, msg->payload_length);
		p += msg->payload_length;
	}

	return (int)(p - out);
}

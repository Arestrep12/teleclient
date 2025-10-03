#include "coap_codec.h"
#include <string.h>

static int read_extended(const uint8_t *buf, size_t len, size_t *offset,
                         uint8_t nibble, uint32_t *out_value) {
	if (!buf || !offset || !out_value) return COAP_CODEC_EINVAL;
	if (*offset >= len) return COAP_CODEC_EMALFORMED;

	if (nibble <= 12) {
		*out_value = nibble;
		return COAP_CODEC_OK;
	} else if (nibble == 13) {
		// 13 => 13 + 1 byte
		if ((*offset + 1) > len) return COAP_CODEC_EMALFORMED;
		uint8_t ext = buf[*offset];
		(*offset) += 1;
		*out_value = 13u + (uint32_t)ext;
		return COAP_CODEC_OK;
	} else if (nibble == 14) {
		// 14 => 269 + 2 bytes
		if ((*offset + 2) > len) return COAP_CODEC_EMALFORMED;
		uint16_t ext = (uint16_t)((buf[*offset] << 8) | buf[*offset + 1]);
		(*offset) += 2;
		*out_value = 269u + (uint32_t)ext;
		return COAP_CODEC_OK;
	} else {
		// 15 reservado
		return COAP_CODEC_EMALFORMED;
	}
}

int coap_decode(CoapMessage *msg, const uint8_t *buffer, size_t length) {
	if (!msg || !buffer) return COAP_CODEC_EINVAL;
	if (length < 4) return COAP_CODEC_EMALFORMED;

	coap_message_clear(msg);
	msg->payload = NULL;
	msg->payload_length = 0;

	// Header fijo
	uint8_t b0 = buffer[0];
	uint8_t ver = (uint8_t)((b0 >> 6) & 0x03);
	uint8_t type = (uint8_t)((b0 >> 4) & 0x03);
	uint8_t tkl = (uint8_t)(b0 & 0x0F);
	uint8_t code = buffer[1];
	uint16_t msg_id = (uint16_t)((buffer[2] << 8) | buffer[3]);

	if (ver != COAP_VERSION) return COAP_CODEC_EINVAL;
	if (tkl > COAP_MAX_TOKEN_LENGTH) return COAP_CODEC_EINVAL;
	if (type > COAP_TYPE_RESET) return COAP_CODEC_EINVAL;

	msg->version = ver;
	msg->type = (CoapType)type;
	msg->token_length = tkl;
	msg->code = (CoapCode)code;
	msg->message_id = msg_id;
	msg->option_count = 0;

	size_t off = 4;
	// Token
	if ((off + tkl) > length) return COAP_CODEC_EMALFORMED;
	if (tkl > 0) memcpy(msg->token, buffer + off, tkl);
	off += tkl;

	// Opciones y payload
	uint32_t last_option_number = 0;
	while (off < length) {
		uint8_t byte = buffer[off];
		if (byte == COAP_PAYLOAD_MARKER) {
			// payload
			off += 1;
			size_t remaining = length - off;
			if (remaining > 0) {
				if (remaining > COAP_MAX_MESSAGE_SIZE) return COAP_CODEC_EMALFORMED;
				if (remaining > sizeof(msg->payload_buffer)) return COAP_CODEC_EMALFORMED;
				memcpy(msg->payload_buffer, buffer + off, remaining);
				msg->payload = msg->payload_buffer;
				msg->payload_length = remaining;
			}
			return COAP_CODEC_OK;
		}

		uint8_t delta_nibble = (uint8_t)((byte >> 4) & 0x0F);
		uint8_t len_nibble = (uint8_t)(byte & 0x0F);
		off += 1;

		if (delta_nibble == 15 || len_nibble == 15) {
			return COAP_CODEC_EMALFORMED;
		}

		uint32_t delta = 0;
		uint32_t opt_len = 0;
		// Extensiones
		int rc;
		rc = read_extended(buffer, length, &off, delta_nibble, &delta);
		if (rc != COAP_CODEC_OK) return rc;
		rc = read_extended(buffer, length, &off, len_nibble, &opt_len);
		if (rc != COAP_CODEC_OK) return rc;

		// Validaciones de límites
		if (opt_len > COAP_MAX_OPTION_VALUE_LENGTH) return COAP_CODEC_EOPTIONS;

		uint32_t option_number32 = last_option_number + delta;
		if (option_number32 > 0xFFFFu) return COAP_CODEC_EOPTIONS;
		uint16_t option_number = (uint16_t)option_number32;

		// Asegurar orden ascendente
		if (option_number < last_option_number) return COAP_CODEC_EOPTIONS;

		if (msg->option_count >= (sizeof msg->options / sizeof msg->options[0])) {
			return COAP_CODEC_EOPTIONS;
		}

		// Leer valor de la opción
		if ((off + opt_len) > length) return COAP_CODEC_EMALFORMED;
		// Insertar usando la API (mantiene orden y valida longitud)
		if (opt_len > 0) {
			int r = coap_message_add_option(msg, option_number,
										   buffer + off, opt_len);
			if (r != 0) return COAP_CODEC_EOPTIONS;
		} else {
			// longitud 0
			int r = coap_message_add_option(msg, option_number, NULL, 0);
			if (r != 0) return COAP_CODEC_EOPTIONS;
		}
		off += opt_len;
		last_option_number = option_number;
	}

	// No hay payload marker; mensaje termina aquí
	return COAP_CODEC_OK;
}

#include <postgres.h>

#include <bech32.h>

#pragma GCC visibility push(hidden)

struct bech32_params {
	const char *encoding_name;
	size_t checksum_size,
		min_size, max_size,
		hrp_min_size, hrp_max_size,
		program_min_size, program_max_size,
		program_pkh_size, program_sh_size, program_tr_size,
		address_min_size;
	size_t (*encoded_size)(size_t, size_t, size_t);
	ssize_t (*address_encode)(char *restrict, size_t, const unsigned char *restrict, size_t, const char *restrict, size_t, unsigned);
	ssize_t (*address_decode)(unsigned char *restrict, size_t, const char *restrict, size_t, size_t *restrict, unsigned *restrict);
};

extern const struct bech32_params bech32_params, blech32_params;

void bech32_check_encode_error(enum bech32_error error, const struct bech32_params *params)
	__attribute__ ((__nothrow__));

void bech32_check_decode_error(ssize_t ret, const char in[], size_t n_in)
	__attribute__ ((__access__ (read_only, 2), __nonnull__, __nothrow__));

#pragma GCC visibility pop

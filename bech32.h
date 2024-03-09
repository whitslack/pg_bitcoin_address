#include <postgres.h>

#include <bech32.h>


void bech32_check_encode_error(enum bech32_error error)
	__attribute__ ((__nothrow__, __visibility__ ("hidden")));

void bech32_check_decode_error(ssize_t ret, const char in[], size_t n_in)
	__attribute__ ((__access__ (read_only, 2), __nonnull__, __nothrow__, __visibility__ ("hidden")));

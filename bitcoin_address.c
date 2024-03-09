#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#if HAVE_VARATT_H
# include <varatt.h>
#endif
#include <utils/builtins.h>

#include <base58check.h>

#include "bech32.h"

#define _likely(...) __builtin_expect(!!(__VA_ARGS__), 1)
#define _unlikely(...) __builtin_expect(!!(__VA_ARGS__), 0)


typedef struct varlena bitcoin_address;

/*
 * In memory and on disk, a bitcoin_address consists of an initial HRP length byte, followed by that many bytes of HRP, followed by
 * the 1-byte witness version, followed by the witness program. If the initial byte is 0xFF, it signifies a legacy address, in
 * which case the bytes that follow are exactly the bytes to be encoded in Base58Check. As an optimization, the initial byte may
 * have another value that is invalid as an HRP length, and this is interpreted as a well-known HRP from the lookup table below.
 */
static const char *const well_known_hrp[] = { // DO NOT RE-ORDER!
	// see SLIP-0173: Registered human-readable parts for BIP-0173
	/*0x80*/ "bc", // Bitcoin Mainnet
	/*0x81*/ "tb", // Bitcoin Testnet
	/*0x82*/ "bcrt", // Bitcoin Regtest
};

static ssize_t find_well_known_hrp(const char *hrp, size_t n_hrp) {
	for (size_t i = 0; i < sizeof well_known_hrp / sizeof *well_known_hrp; ++i)
		if (strncasecmp(hrp, well_known_hrp[i], n_hrp) == 0 && well_known_hrp[i][n_hrp] == '\0')
			return (ssize_t) i;
	return -1;
}

static const char * get_well_known_hrp(size_t idx) {
	if (_unlikely(idx >= sizeof well_known_hrp / sizeof *well_known_hrp))
		ereport(ERROR, errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				errmsg("stored bitcoin_address uses unknown human-readable prefix"));
	return well_known_hrp[idx];
}


PG_FUNCTION_INFO_V1(pg_bitcoin_address);
Datum
pg_bitcoin_address(PG_FUNCTION_ARGS)
{
	if (_unlikely(PG_ARGISNULL(1) || PG_ARGISNULL(2)))
		PG_RETURN_NULL();

	uint32 version = PG_GETARG_UINT32(1);
	const unsigned char *program;
	size_t n_program;
	{
		const bytea *arg = PG_GETARG_BYTEA_PP(2);
		program = (const unsigned char *) VARDATA_ANY(arg), n_program = VARSIZE_ANY_EXHDR(arg);
	}
	const char *hrp;
	size_t n_hrp;
	if (PG_ARGISNULL(0))
		hrp = NULL, n_hrp = 0;
	else {
		const text *arg = PG_GETARG_TEXT_PP(0);
		hrp = VARDATA_ANY(arg), n_hrp = VARSIZE_ANY_EXHDR(arg);

		size_t n = bech32_encoded_size(n_hrp, 5/*version*/ + n_program * BITS_PER_BYTE, 0);
		if (_unlikely(n > BECH32_MAX_SIZE))
			bech32_check_encode_error(BECH32_TOO_LONG);
	}

	ssize_t well_known_hrp_idx = -1;
	if (hrp) { // SegWit address
		if (_unlikely(n_hrp < BECH32_HRP_MIN_SIZE))
			bech32_check_encode_error(BECH32_HRP_TOO_SHORT);
		if (_unlikely(n_hrp > BECH32_HRP_MAX_SIZE))
			bech32_check_encode_error(BECH32_HRP_TOO_LONG);
		if (_unlikely(version > WITNESS_MAX_VERSION))
			bech32_check_encode_error(SEGWIT_VERSION_ILLEGAL);
		if (_unlikely(n_program < WITNESS_PROGRAM_MIN_SIZE))
			bech32_check_encode_error(SEGWIT_PROGRAM_TOO_SHORT);
		if (_unlikely(n_program > WITNESS_PROGRAM_MAX_SIZE))
			bech32_check_encode_error(SEGWIT_PROGRAM_TOO_LONG);
		if (_unlikely(version == 0 && n_program != WITNESS_PROGRAM_PKH_SIZE && n_program != WITNESS_PROGRAM_SH_SIZE))
			bech32_check_encode_error(SEGWIT_PROGRAM_ILLEGAL_SIZE);

		if ((well_known_hrp_idx = find_well_known_hrp(hrp, n_hrp)) >= 0)
			hrp = NULL, n_hrp = 0;
	}
	else { // legacy address
		if (_unlikely(version > UINT8_MAX))
			ereport(ERROR, errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("version is illegal"),
					errhint("legacy address version must be between 0 and 255"));
		if (_unlikely(n_program != 20))
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("program is of an illegal size"),
					errhint("legacy address program size must be 20 bytes"));
	}

	size_t n_out = VARHDRSZ + 1 + n_hrp + 1/*version*/ + n_program;
	bitcoin_address *out = palloc(n_out);
	unsigned char *p = (unsigned char *) VARDATA(out);
	*p++ = (unsigned char) (well_known_hrp_idx >= 0 ? (size_t) well_known_hrp_idx + 0x80 : hrp ? n_hrp : 0xFF);
	if (hrp)
		memcpy(p, hrp, n_hrp), p += n_hrp;
	*p++ = (unsigned char) version;
	memcpy(p, program, n_program);

	SET_VARSIZE(out, n_out);
	PG_RETURN_POINTER(out);
}


PG_FUNCTION_INFO_V1(pg_bitcoin_address_input);
Datum
pg_bitcoin_address_input(PG_FUNCTION_ARGS)
{
	const char *in = PG_GETARG_CSTRING(0);
	size_t n_in = strlen(in), n_out = 0;
	bitcoin_address *out = NULL;

	if (n_in >= SEGWIT_ADDRESS_MIN_SIZE && n_in <= BECH32_MAX_SIZE) {
		const char *sep = memrchr(in, '1', n_in);
		if (!sep)
			goto not_segwit;

		size_t n_hrp = sep - in;
		if (n_hrp < BECH32_HRP_MIN_SIZE || n_hrp > BECH32_HRP_MAX_SIZE ||
				n_in < SEGWIT_ADDRESS_MIN_SIZE - BECH32_HRP_MIN_SIZE + n_hrp)
			goto not_segwit;

		size_t n_program = (n_in - n_hrp - 1/*separator*/ - 1/*version*/ - 6/*checksum*/) * 5 / BITS_PER_BYTE;
		if (n_program < WITNESS_PROGRAM_MIN_SIZE || n_program > WITNESS_PROGRAM_MAX_SIZE)
			goto not_segwit;

		ssize_t well_known_hrp_idx = find_well_known_hrp(in, n_hrp);
		size_t program_offset = 1/*hrp length*/ + (well_known_hrp_idx >= 0 ? 0 : n_hrp) + 1/*version*/;
		out = palloc(n_out = VARHDRSZ + program_offset + n_program);

		size_t n_hrp_actual;
		unsigned version;
		ssize_t n_program_actual = segwit_address_decode(
				(unsigned char *) VARDATA(out) + program_offset, n_program,
				in, n_in, &n_hrp_actual, &version);
		if (n_program_actual < 0)
			switch ((enum bech32_error) n_program_actual) {
				case BECH32_MIXED_CASE:
				case BECH32_ILLEGAL_CHAR:
				case BECH32_HRP_ILLEGAL_CHAR:
				case SEGWIT_VERSION_ILLEGAL:
					// not SegWit, but try to reuse already allocated buffer
					if (n_out >= base58check_decode_buffer_size(in, n_in, VARHDRSZ + 1) &&
						base58check_decode((unsigned char **) &out, &n_out, in, n_in, VARHDRSZ + 1) == 0)
					{
						VARDATA(out)[0] = (uint8_t) 0xFF;
						goto success;
					}
					pfree(out), out = NULL, n_out = 0;
					goto not_segwit;
				case BECH32_PADDING_ERROR:
				case BECH32_CHECKSUM_FAILURE:
				case SEGWIT_PROGRAM_ILLEGAL_SIZE:
					bech32_check_decode_error(n_program_actual, in, n_in);
					// fall through
				case BECH32_TOO_SHORT:
				case BECH32_TOO_LONG:
				case BECH32_NO_SEPARATOR:
				case BECH32_BUFFER_INADEQUATE:
				case BECH32_HRP_TOO_SHORT:
				case BECH32_HRP_TOO_LONG:
				case SEGWIT_PROGRAM_TOO_SHORT:
				case SEGWIT_PROGRAM_TOO_LONG:
					__builtin_unreachable();
			}
		else if (_likely((size_t) n_program_actual == n_program && n_hrp_actual == n_hrp)) {
			if (well_known_hrp_idx >= 0) {
				VARDATA(out)[0] = (uint8) (well_known_hrp_idx + 0x80);
				VARDATA(out)[1] = (uint8) version;
			}
			else {
				VARDATA(out)[0] = (uint8) n_hrp;
				for (size_t i = 0; i < n_hrp; ++i)
					VARDATA(out)[1 + i] = (uint8) (in[i] | (in[i] >= 'A' && in[i] <= 'Z' ? 0x20 : 0));
				VARDATA(out)[1 + n_hrp] = (uint8) version;
			}
			goto success;
		}
		ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("internal error %d", (int) n_program_actual),
				errdetail_internal("%s", in));
	}

not_segwit:
	if (_unlikely(base58check_decode((unsigned char **) &out, &n_out, in, n_in, VARHDRSZ + 1) < 0 ||
			n_out != VARHDRSZ + 1 + 1/*version*/ + 20/*pubkey/script hash*/))
		ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("not a valid Bitcoin address"),
				errdetail_internal("%s", in));
	VARDATA(out)[0] = (uint8_t) 0xFF;

success:
	SET_VARSIZE(out, n_out);
	PG_RETURN_POINTER(out);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_output);
Datum
pg_bitcoin_address_output(PG_FUNCTION_ARGS)
{
	const bitcoin_address *arg = (const bitcoin_address *) PG_GETARG_POINTER(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	size_t n_in = VARSIZE_ANY_EXHDR(arg);
	const char *hrp;
	size_t n_hrp = *in++; --n_in;

	char *out = NULL;
	size_t n_out;
	if (n_hrp == 0xFF) { // legacy address
		n_out = 1/*null terminator*/;
		if (_unlikely(base58check_encode(&out, &n_out, in, n_in, 0) < 0))
			ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("internal error"));
	}
	else { // SegWit address
		if (n_hrp < BECH32_HRP_MIN_SIZE || n_hrp > BECH32_HRP_MAX_SIZE) // well-known HRP
			n_hrp = strlen(hrp = get_well_known_hrp(n_hrp - 0x80));
		else // explicit HRP
			hrp = (const char *) in, in += n_hrp, n_in -= n_hrp;

		unsigned version = *in++; --n_in;
		if (_unlikely(version > WITNESS_MAX_VERSION))
			bech32_check_encode_error(SEGWIT_VERSION_ILLEGAL);

		n_out = bech32_encoded_size(n_hrp, 5 + n_in * BITS_PER_BYTE, VARHDRSZ);
		out = palloc(n_out + 1/*null terminator*/);
		bech32_check_encode_error(segwit_address_encode(out, n_out, in, n_in, hrp, n_hrp, version));
	}
	out[n_out] = '\0';
	PG_RETURN_CSTRING(out);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_is_segwit);
Datum __attribute__ ((__pure__))
pg_bitcoin_address_is_segwit(PG_FUNCTION_ARGS)
{
	const bitcoin_address *arg = (const bitcoin_address *) PG_GETARG_POINTER(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);

	PG_RETURN_BOOL(*in != 0xFF);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_hrp);
Datum
pg_bitcoin_address_hrp(PG_FUNCTION_ARGS)
{
	const bitcoin_address *arg = (const bitcoin_address *) PG_GETARG_POINTER(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	const char *hrp;
	size_t n_hrp = *in++;

	if (n_hrp == 0xFF) // legacy address
		PG_RETURN_NULL();

	if (n_hrp < BECH32_HRP_MIN_SIZE || n_hrp > BECH32_HRP_MAX_SIZE) // well-known HRP
		n_hrp = strlen(hrp = get_well_known_hrp(n_hrp - 0x80));
	else // explicit HRP
		hrp = (const char *) in;

	PG_RETURN_TEXT_P(cstring_to_text_with_len(hrp, (int) n_hrp));
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_version);
Datum __attribute__ ((__pure__))
pg_bitcoin_address_version(PG_FUNCTION_ARGS)
{
	const bitcoin_address *arg = (const bitcoin_address *) PG_GETARG_POINTER(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	size_t n_hrp = *in++;

	if (n_hrp >= BECH32_HRP_MIN_SIZE && n_hrp <= BECH32_HRP_MAX_SIZE) // explicit HRP
		in += n_hrp;

	PG_RETURN_UINT32(*in);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_program);
Datum
pg_bitcoin_address_program(PG_FUNCTION_ARGS)
{
	const bitcoin_address *arg = (const bitcoin_address *) PG_GETARG_POINTER(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	size_t n_in = VARSIZE_ANY_EXHDR(arg);
	size_t n_hrp = *in++; --n_in;

	if (n_hrp >= BECH32_HRP_MIN_SIZE && n_hrp <= BECH32_HRP_MAX_SIZE) // explicit HRP
		in += n_hrp, n_in -= n_hrp;

	++in, --n_in; // skip version byte

	PG_RETURN_BYTEA_P(cstring_to_text_with_len((const char *) in, (int) n_in));
}

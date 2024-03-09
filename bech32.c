#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#if HAVE_VARATT_H
# include <varatt.h>
#endif
#include <utils/builtins.h>
#include <utils/varbit.h>

#include "bech32.h"

#define _likely(...) __builtin_expect(!!(__VA_ARGS__), 1)
#define _unlikely(...) __builtin_expect(!!(__VA_ARGS__), 0)


void
bech32_check_encode_error(enum bech32_error error)
{
	if (_likely(error >= 0)) return;
	switch (error) {
		case BECH32_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
					errmsg("Bech32 encoding is too long"),
					errhint("encoding must be no more than 90 characters in length"));
		case BECH32_HRP_TOO_SHORT:
			ereport(ERROR, errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING),
					errmsg("Bech32 human-readable prefix is empty"),
					errhint("HRP must be between 1 and 83 characters in length"));
		case BECH32_HRP_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
					errmsg("Bech32 human-readable prefix is too long"),
					errhint("HRP must be between 1 and 83 characters in length"));
		case BECH32_HRP_ILLEGAL_CHAR:
			ereport(ERROR, errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
					errmsg("Bech32 human-readable prefix contains an illegal character"),
					errhint("HRP may contain only US-ASCII character codes 33 through 126"));
		case SEGWIT_VERSION_ILLEGAL:
			ereport(ERROR, errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("witness version is illegal"),
					errhint("witness version must be between 0 and 16"));
		case SEGWIT_PROGRAM_TOO_SHORT:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is too short"),
					errhint("witness program must be between 2 and 40 bytes in length"));
		case SEGWIT_PROGRAM_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is too long"),
					errhint("witness program must be between 2 and 40 bytes in length"));
		case SEGWIT_PROGRAM_ILLEGAL_SIZE:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is of an illegal size"),
					errhint("version 0 witness program size must be either 20 or 32 bytes"));
		case BECH32_TOO_SHORT:
		case BECH32_NO_SEPARATOR:
		case BECH32_MIXED_CASE:
		case BECH32_ILLEGAL_CHAR:
		case BECH32_PADDING_ERROR:
		case BECH32_BUFFER_INADEQUATE:
			__builtin_unreachable();
		case BECH32_CHECKSUM_FAILURE:
			break;
	}
	ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("internal error %d", (int) error));
}

static void
do_encode(
	char out[], size_t n_out,
	const char hrp[], size_t n_hrp,
	const unsigned char in[], size_t nbits_in,
	uint32_t constant)
{
	struct bech32_encoder_state state;

	bech32_check_encode_error(bech32_encode_begin(&state, out, n_out, hrp, n_hrp));

	size_t nbits_extra = nbits_in % BITS_PER_BYTE;
	bech32_check_encode_error(bech32_encode_data(&state, in, nbits_in - nbits_extra));

	if (nbits_extra) {
		unsigned char extra = in[nbits_in / BITS_PER_BYTE] >> BITS_PER_BYTE - nbits_extra;
		bech32_check_encode_error(bech32_encode_data(&state, &extra, nbits_extra));
	}

	bech32_check_encode_error(bech32_encode_finish(&state, constant));
}

static Datum
encode(PG_FUNCTION_ARGS, uint32_t constant)
{
	const text *hrp = PG_GETARG_TEXT_PP(0);
	size_t n_hrp = VARSIZE_ANY_EXHDR(hrp);
	const VarBit *bits = PG_GETARG_VARBIT_P(1);
	size_t nbits = VARBITLEN(bits);

	size_t n_out = bech32_encoded_size(n_hrp, nbits, VARHDRSZ);
	if (_unlikely(n_out > VARHDRSZ + BECH32_MAX_SIZE))
		bech32_check_encode_error(BECH32_TOO_LONG);

	text *out = palloc(n_out);
	do_encode(
		VARDATA(out), n_out - VARHDRSZ,
		VARDATA_ANY(hrp), n_hrp,
		VARBITS(bits), nbits,
		constant);

	SET_VARSIZE(out, n_out);
	PG_RETURN_TEXT_P(out);
}

PG_FUNCTION_INFO_V1(pg_bech32_encode);
Datum pg_bech32_encode(PG_FUNCTION_ARGS) { return encode(fcinfo, 1); }

PG_FUNCTION_INFO_V1(pg_bech32m_encode);
Datum pg_bech32m_encode(PG_FUNCTION_ARGS) { return encode(fcinfo, BECH32M_CONST); }


void
bech32_check_decode_error(ssize_t ret, const char in[], size_t n_in)
{
	if (_likely(ret >= 0)) return;
	switch ((enum bech32_error) ret) {
		case BECH32_TOO_SHORT:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("Bech32 encoding is too short"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("Bech32 encoding is too long"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_NO_SEPARATOR:
			ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("Bech32 encoding contains no separator"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_MIXED_CASE:
			ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("Bech32 encoding uses mixed case"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_ILLEGAL_CHAR:
			ereport(ERROR, errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
					errmsg("Bech32 encoding contains an illegal character"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_PADDING_ERROR:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("Bech32 encoding has a padding error"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_CHECKSUM_FAILURE:
			ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("Bech32 checksum verification failed"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_HRP_TOO_SHORT:
			ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("Bech32 human-readable prefix is empty"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_HRP_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("Bech32 human-readable prefix is too long"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_HRP_ILLEGAL_CHAR:
			ereport(ERROR, errcode(ERRCODE_CHARACTER_NOT_IN_REPERTOIRE),
					errmsg("Bech32 human-readable prefix contains an illegal character"),
					errdetail_internal("%.*s", (int) n_in, in));
		case SEGWIT_VERSION_ILLEGAL:
			ereport(ERROR, errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
					errmsg("witness version is illegal"),
					errdetail_internal("%.*s", (int) n_in, in));
		case SEGWIT_PROGRAM_TOO_SHORT:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is too short"),
					errdetail_internal("%.*s", (int) n_in, in));
		case SEGWIT_PROGRAM_TOO_LONG:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is too long"),
					errdetail_internal("%.*s", (int) n_in, in));
		case SEGWIT_PROGRAM_ILLEGAL_SIZE:
			ereport(ERROR, errcode(ERRCODE_STRING_DATA_LENGTH_MISMATCH),
					errmsg("witness program is of an illegal size"),
					errdetail_internal("%.*s", (int) n_in, in));
		case BECH32_BUFFER_INADEQUATE:
			__builtin_unreachable();
	}
	ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
			errmsg("internal error %d", (int) ret),
			errdetail_internal("%.*s", (int) n_in, in));
}

static Datum
do_decode(const char in[], size_t n_in, uint32_t constant)
{
	struct bech32_decoder_state state;

	bech32_check_decode_error(bech32_decode_begin(&state, in, n_in), in, n_in);

	size_t nbits_out = bech32_decode_bits_remaining(&state), n_out = VARBITTOTALLEN(nbits_out);
	VarBit *out = palloc(n_out);
	SET_VARSIZE(out, n_out);
	VARBITLEN(out) = (int) nbits_out;

	bech32_check_decode_error(bech32_decode_data(&state, VARBITS(out), nbits_out), in, n_in);

	size_t nbits_extra = nbits_out % BITS_PER_BYTE;
	if (nbits_extra)
		VARBITEND(out)[-1] <<= BITS_PER_BYTE - nbits_extra;

	bech32_check_decode_error(bech32_decode_finish(&state, constant), in, n_in);

	PG_RETURN_VARBIT_P(out);
}

static Datum
decode(PG_FUNCTION_ARGS, uint32_t constant)
{
	const text *in = PG_GETARG_TEXT_PP(0);
	size_t n_in = VARSIZE_ANY_EXHDR(in);

	return do_decode(VARDATA_ANY(in), n_in, constant);
}

PG_FUNCTION_INFO_V1(pg_bech32_decode);
Datum pg_bech32_decode(PG_FUNCTION_ARGS) { return decode(fcinfo, 1); }

PG_FUNCTION_INFO_V1(pg_bech32m_decode);
Datum pg_bech32m_decode(PG_FUNCTION_ARGS) { return decode(fcinfo, BECH32M_CONST); }


PG_FUNCTION_INFO_V1(pg_bech32_hrp);
Datum
pg_bech32_hrp(PG_FUNCTION_ARGS)
{
	const text *in = PG_GETARG_TEXT_PP(0);
	size_t n_in = VARSIZE_ANY_EXHDR(in);

	struct bech32_decoder_state state;
	ssize_t n_hrp = bech32_decode_begin(&state, VARDATA_ANY(in), n_in);
	bech32_check_decode_error(n_hrp, VARDATA_ANY(in), n_in);

	PG_RETURN_TEXT_P(cstring_to_text_with_len(VARDATA_ANY(in), (int) n_hrp));
}

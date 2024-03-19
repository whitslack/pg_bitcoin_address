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

struct bitcoin_address_fields {
	bool blech;
	uint8 version;
	int well_known_hrp_idx;
	const char *hrp;
	size_t n_hrp;
	const uint8 *program;
	size_t n_program;
};

/*
 * In memory and on disk, a bitcoin_address consists of an initial HRP length byte, followed by that many bytes of HRP, followed by
 * the 1-byte witness version, followed by the witness program. If the initial byte is 0xFF, it signifies a legacy address, in
 * which case the bytes that follow are exactly the bytes to be encoded in Base58Check. If the initial byte is between 84 and 0x7F,
 * it signifies a Blech32-encoded address, and the initial byte minus 83 gives the length of the HRP, except if the initial byte is
 * 0x7F, in which case the next two bytes give the length of the HRP (in network byte order). As an optimization, if the initial
 * byte has a value between 0x80 and 0xFE, then bit 6 indicates whether the encoding uses Blech32, the lower 6 bits give an index
 * into the table of well-known HRPs below, and no explicit HRP is stored.
 */
static const char *const well_known_hrp[] = { // DO NOT RE-ORDER!
	// see SLIP-0173: Registered human-readable parts for BIP-0173
	"bc", // Bitcoin Mainnet
	"tb", // Bitcoin Testnet
	"bcrt", // Bitcoin Regtest
	"ex", // Liquidv1 explicit
	"lq", // Liquidv1
	"tex", // Liquid Testnet explicit
	"tlq", // Liquid Testnet
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

static void unpack(struct bitcoin_address_fields *restrict f, const bitcoin_address *restrict arg) {
	const uint8 *data = (const uint8 *) VARDATA_ANY(arg);
	size_t n_data = VARSIZE_ANY_EXHDR(arg);
	if (_unlikely(n_data < 1/*n_hrp*/ + 1/*version*/))
corrupted:
		ereport(ERROR, errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
				errmsg("stored bitcoin_address is corrupted"));
	size_t n_hrp = *data++; --n_data;
	if (n_hrp == 0xFF) { // legacy address
		f->blech = false;
		f->well_known_hrp_idx = 0;
		f->hrp = NULL;
		f->n_hrp = 0;
	}
	else if (n_hrp >= 0x80) { // well-known HRP
		if (f->blech = (n_hrp -= 0x80) >= 0x40)
			n_hrp -= 0x40;
		f->well_known_hrp_idx = (int) n_hrp;
		f->n_hrp = strlen(f->hrp = get_well_known_hrp(n_hrp));
	}
	else if (_unlikely(n_hrp == 0))
		goto corrupted;
	else {
		if (f->blech = n_hrp > BECH32_HRP_MAX_SIZE) { // blinding address
			if (n_hrp == 0x7F) { // HRP length stored in next 2 bytes
				if (_unlikely(n_data < 2))
					goto corrupted;
				n_hrp = data[0] << 8 | data[1];
				data += 2, n_data -= 2;
			}
			else
				n_hrp -= BECH32_HRP_MAX_SIZE;
		}
		if (_unlikely(n_data < n_hrp + 1/*version*/))
			goto corrupted;
		f->well_known_hrp_idx = -1;
		f->hrp = (const char *) data, f->n_hrp = n_hrp;
		data += n_hrp, n_data -= n_hrp;
	}
	f->version = *data++, --n_data;
	f->program = data, f->n_program = n_data;
}

static void pack(bitcoin_address *restrict out, const struct bitcoin_address_fields *restrict f) {
	uint8 *data = (uint8 *) VARDATA(out);
	if (!f->hrp) // legacy address
		*data++ = 0xFF;
	else if (f->well_known_hrp_idx >= 0)
		*data++ = (uint8) (f->well_known_hrp_idx + 0x80 + (f->blech ? 0x40 : 0));
	else {
		if (!f->blech)
			*data++ = (uint8) f->n_hrp;
		else if (BECH32_HRP_MAX_SIZE + f->n_hrp < 0x7F)
			*data++ = (uint8) (BECH32_HRP_MAX_SIZE + f->n_hrp);
		else
			*data++ = 0x7F, *data++ = (uint8) (f->n_hrp >> 8), *data++ = (uint8) f->n_hrp;
		for (size_t i = 0; i < f->n_hrp; ++i)
			*data++ = (uint8) (f->hrp[i] | (f->hrp[i] >= 'A' && f->hrp[i] <= 'Z' ? 0x20 : 0));
	}
	*data++ = (uint8) f->version;
	if (f->program != data)
		memcpy(data, f->program, f->n_program);
}


PG_FUNCTION_INFO_V1(pg_bitcoin_address);
Datum
pg_bitcoin_address(PG_FUNCTION_ARGS)
{
	if (_unlikely(PG_ARGISNULL(1) || PG_ARGISNULL(2)))
		PG_RETURN_NULL();

	struct bitcoin_address_fields f;
	uint32 version = PG_GETARG_UINT32(1);
	f.version = (uint8) version;
	{
		const bytea *arg = PG_GETARG_BYTEA_PP(2);
		f.program = (const uint8 *) VARDATA_ANY(arg), f.n_program = VARSIZE_ANY_EXHDR(arg);
	}
	f.blech = PG_NARGS() < 4 || PG_ARGISNULL(3) ? f.n_program > WITNESS_PROGRAM_MAX_SIZE : PG_GETARG_BOOL(3);
	if (!PG_ARGISNULL(0)) { // SegWit address
		{
			const text *arg = PG_GETARG_TEXT_PP(0);
			f.hrp = VARDATA_ANY(arg), f.n_hrp = VARSIZE_ANY_EXHDR(arg);
		}
		const struct bech32_params *const params = f.blech ? &blech32_params : &bech32_params;
		if (_unlikely((*params->encoded_size)(f.n_hrp, 5/*version*/ + f.n_program * BITS_PER_BYTE, 0) > params->max_size))
			bech32_check_encode_error(BECH32_TOO_LONG, params);
		if (_unlikely(f.n_hrp < params->hrp_min_size))
			bech32_check_encode_error(BECH32_HRP_TOO_SHORT, params);
		if (_unlikely(f.n_hrp > params->hrp_max_size))
			bech32_check_encode_error(BECH32_HRP_TOO_LONG, params);
		if (_unlikely(version > WITNESS_MAX_VERSION))
			bech32_check_encode_error(SEGWIT_VERSION_ILLEGAL, params);
		if (_unlikely(f.n_program < params->program_min_size))
			bech32_check_encode_error(SEGWIT_PROGRAM_TOO_SHORT, params);
		if (_unlikely(f.n_program > params->program_max_size))
			bech32_check_encode_error(SEGWIT_PROGRAM_TOO_LONG, params);
		if (_unlikely(version == 0 && f.n_program != params->program_pkh_size && f.n_program != params->program_sh_size))
			bech32_check_encode_error(SEGWIT_PROGRAM_ILLEGAL_SIZE, params);

		f.well_known_hrp_idx = (int) find_well_known_hrp(f.hrp, f.n_hrp);
	}
	else { // legacy address
		f.well_known_hrp_idx = 0;
		f.hrp = NULL, f.n_hrp = 0;
		if (_unlikely(version > UINT8_MAX))
			ereport(ERROR, errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("version is illegal"),
					errhint("legacy address version must be between 0 and 255"));
	}

	size_t n_out = VARHDRSZ + 1/*initial byte*/ +
			(f.well_known_hrp_idx < 0 ? (f.blech && BECH32_HRP_MAX_SIZE + f.n_hrp >= 0x7F ? 2 : 0) + f.n_hrp : 0) +
			1/*version*/ + f.n_program;
	bitcoin_address *out = palloc(n_out);
	pack(out, &f);

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

	const char *sep = memrchr(in, '1', n_in);
	if (!sep)
		goto not_segwit;

	struct bitcoin_address_fields f;
	f.well_known_hrp_idx = (int) find_well_known_hrp(f.hrp = in, f.n_hrp = sep - in);

	static const struct bech32_params *const paramses[] = { &bech32_params, &blech32_params };
	for (size_t params_idx = 0; params_idx < sizeof paramses / sizeof *paramses; ++params_idx) {
		const struct bech32_params *const params = paramses[params_idx];

		f.blech = params == &blech32_params;
		f.n_program = (n_in - f.n_hrp - 1/*separator*/ - 1/*version*/ - params->checksum_size) * 5 / BITS_PER_BYTE;
		if (n_in < params->address_min_size || n_in > params->max_size ||
				f.n_hrp < params->hrp_min_size || f.n_hrp > params->hrp_max_size ||
				n_in < params->address_min_size - params->hrp_min_size + f.n_hrp ||
				f.n_program < params->program_min_size || f.n_program > params->program_max_size)
			continue;

		size_t program_offset = 1/*initial byte*/ +
				(f.well_known_hrp_idx < 0 ? (f.blech && BECH32_HRP_MAX_SIZE + f.n_hrp >= 0x7F ? 2 : 0) + f.n_hrp : 0) +
				1/*version*/;
		if (out) pfree(out);
		out = palloc(n_out = VARHDRSZ + program_offset + f.n_program);
		uint8 *program = (uint8 *) (VARDATA(out) + program_offset);
		f.program = program;

		size_t n_hrp_actual;
		unsigned version;
		ssize_t n_program_actual = (*params->address_decode)(
				program, f.n_program,
				in, n_in, &n_hrp_actual, &version);
		if (n_program_actual < 0)
			switch ((enum bech32_error) n_program_actual) {
				case BECH32_MIXED_CASE:
				case BECH32_ILLEGAL_CHAR:
				case BECH32_HRP_ILLEGAL_CHAR:
				case SEGWIT_VERSION_ILLEGAL:
					goto not_segwit;
				case BECH32_PADDING_ERROR:
				case BECH32_CHECKSUM_FAILURE:
					continue;
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
		else if (_likely((size_t) n_program_actual == f.n_program && n_hrp_actual == f.n_hrp)) {
			f.version = (uint8) version;
			pack(out, &f);
			goto success;
		}
		ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("internal error %d", (int) n_program_actual),
				errdetail_internal("%s", in));
	}

not_segwit:
	if (out) pfree(out), out = NULL, n_out = 0;
	if (_unlikely(base58check_decode((unsigned char **) &out, &n_out, in, n_in, VARHDRSZ + 1) < 0 || n_out <= VARHDRSZ + 1))
		ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("not a valid Bitcoin address"),
				errdetail_internal("%s", in));
	VARDATA(out)[0] = (uint8) 0xFF;

success:
	SET_VARSIZE(out, n_out);
	PG_RETURN_POINTER(out);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_output);
Datum
pg_bitcoin_address_output(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	char *out = NULL;
	size_t n_out;
	if (!f.hrp) { // legacy address
		n_out = 1/*null terminator*/;
		if (_unlikely(base58check_encode(&out, &n_out, f.program - 1, f.n_program + 1, 0) < 0))
			ereport(ERROR, errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("internal error"));
	}
	else { // SegWit address
		const struct bech32_params *params = f.blech ? &blech32_params : &bech32_params;
		n_out = (*params->encoded_size)(f.n_hrp, 5 + f.n_program * BITS_PER_BYTE, VARHDRSZ);
		out = palloc(n_out + 1/*null terminator*/);
		bech32_check_encode_error(
			(*params->address_encode)(out, n_out, f.program, f.n_program, f.hrp, f.n_hrp, f.version),
			params);
	}
	out[n_out] = '\0';
	PG_RETURN_CSTRING(out);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_is_segwit);
Datum __attribute__ ((__pure__))
pg_bitcoin_address_is_segwit(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	PG_RETURN_BOOL(!!f.hrp);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_is_blech32);
Datum __attribute__ ((__pure__))
pg_bitcoin_address_is_blech32(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	PG_RETURN_BOOL(f.blech);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_hrp);
Datum
pg_bitcoin_address_hrp(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	if (!f.hrp)
		PG_RETURN_NULL();
	PG_RETURN_TEXT_P(cstring_to_text_with_len(f.hrp, (int) f.n_hrp));
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_version);
Datum __attribute__ ((__pure__))
pg_bitcoin_address_version(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	PG_RETURN_UINT32(f.version);
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_program);
Datum
pg_bitcoin_address_program(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	PG_RETURN_BYTEA_P(cstring_to_text_with_len((const char *) f.program, (int) f.n_program));
}

PG_FUNCTION_INFO_V1(pg_bitcoin_address_program_size);
Datum
pg_bitcoin_address_program_size(PG_FUNCTION_ARGS)
{
	struct bitcoin_address_fields f;
	unpack(&f, (const bitcoin_address *) PG_GETARG_POINTER(0));

	PG_RETURN_UINT32((uint32) f.n_program);
}

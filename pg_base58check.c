#include <postgres.h>
#include <fmgr.h>

#include <base58check.h>


#pragma GCC visibility push(default)

PG_MODULE_MAGIC;


PG_FUNCTION_INFO_V1(pg_base58check_encode);

Datum
pg_base58check_encode(PG_FUNCTION_ARGS)
{
	const bytea *arg = PG_GETARG_BYTEA_PP(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	size_t n_in = VARSIZE_ANY_EXHDR(arg), n_out = 0;
	char *out = NULL;

	if (base58check_encode(&out, &n_out, in, n_in, VARHDRSZ) < 0)
		ereport(ERROR, errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
				errmsg("encoding would exceed maximum allocation"));

	SET_VARSIZE(out, n_out);
	PG_RETURN_TEXT_P(out);
}


PG_FUNCTION_INFO_V1(pg_base58check_decode);

Datum
pg_base58check_decode(PG_FUNCTION_ARGS)
{
	const text *arg = PG_GETARG_TEXT_PP(0);
	const char *in = VARDATA_ANY(arg);
	size_t n_in = VARSIZE_ANY_EXHDR(arg), n_out = 0;
	unsigned char *out = NULL;

	if (base58check_decode(&out, &n_out, in, n_in, VARHDRSZ) < 0)
		ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("not a valid Base58Check encoding"));

	SET_VARSIZE(out, n_out);
	PG_RETURN_BYTEA_P(out);
}


PG_FUNCTION_INFO_V1(pg_base58check_output);

Datum
pg_base58check_output(PG_FUNCTION_ARGS)
{
	const bytea *arg = PG_GETARG_BYTEA_PP(0);
	const unsigned char *in = (const unsigned char *) VARDATA_ANY(arg);
	size_t n_in = VARSIZE_ANY_EXHDR(arg), n_out = 1/*null terminator*/;
	char *out = NULL;

	if (base58check_encode(&out, &n_out, in, n_in, 0) < 0)
		ereport(ERROR, errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
				errmsg("encoding would exceed maximum allocation"));

	out[n_out] = '\0';
	PG_RETURN_CSTRING(out);
}


PG_FUNCTION_INFO_V1(pg_base58check_input);

Datum
pg_base58check_input(PG_FUNCTION_ARGS)
{
	const char *in = PG_GETARG_CSTRING(0);
	size_t n_in = strlen(in), n_out = 0;
	unsigned char *out = NULL;

	if (base58check_decode(&out, &n_out, in, n_in, VARHDRSZ) < 0)
		ereport(ERROR, errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("not a valid Base58Check encoding"));

	SET_VARSIZE(out, n_out);
	PG_RETURN_BYTEA_P(out);
}


void base58check_free(void *ptr) {
	pfree(ptr);
}

void * base58check_malloc(size_t size) {
	return palloc(size);
}

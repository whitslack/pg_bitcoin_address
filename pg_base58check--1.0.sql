\echo Execute "CREATE EXTENSION pg_base58check;" to use this extension. \quit


--
-- General-purpose functions
--

CREATE FUNCTION base58check_encode(bytea) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS '$libdir/pg_base58check', 'pg_base58check_encode';

CREATE FUNCTION base58check_decode(text) RETURNS bytea
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS '$libdir/pg_base58check', 'pg_base58check_decode';


--
-- User-defined base type
--

CREATE TYPE base58check;

CREATE FUNCTION base58check_input(cstring) RETURNS base58check
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS '$libdir/pg_base58check', 'pg_base58check_input';

CREATE FUNCTION base58check_output(base58check) RETURNS cstring
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS '$libdir/pg_base58check', 'pg_base58check_output';

CREATE FUNCTION base58check_receive(internal) RETURNS base58check
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'bytearecv';

CREATE FUNCTION base58check_send(base58check) RETURNS bytea
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteasend';

CREATE TYPE base58check (
	INPUT = base58check_input,
	OUTPUT = base58check_output,
	RECEIVE = base58check_receive,
	SEND = base58check_send,
	LIKE = bytea,
	STORAGE = external
);


--
-- Casts
--

CREATE CAST (base58check AS bytea) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (bytea AS base58check) WITHOUT FUNCTION AS IMPLICIT;

CREATE CAST (base58check AS text) WITH FUNCTION base58check_encode(bytea);
CREATE CAST (text AS base58check) WITH FUNCTION base58check_decode(text);


--
-- Comparison functions
--

CREATE FUNCTION base58check_cmp(base58check, base58check) RETURNS integer
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteacmp';

CREATE FUNCTION base58check_eq(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteaeq';

CREATE FUNCTION base58check_ge(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteage';

CREATE FUNCTION base58check_gt(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteagt';

CREATE FUNCTION base58check_le(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteale';

CREATE FUNCTION base58check_lt(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'bytealt';

CREATE FUNCTION base58check_ne(base58check, base58check) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteane';


--
-- Operators
--

CREATE OPERATOR < (
	FUNCTION = base58check_lt,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR <= (
	FUNCTION = base58check_le,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR = (
	FUNCTION = base58check_eq,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR >= (
	FUNCTION = base58check_ge,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR > (
	FUNCTION = base58check_gt,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR <> (
	FUNCTION = base58check_ne,
	LEFTARG = base58check,
	RIGHTARG = base58check
);

CREATE OPERATOR CLASS base58check_ops DEFAULT FOR TYPE base58check
	USING btree AS
	OPERATOR 1 <,
	OPERATOR 2 <=,
	OPERATOR 3 =,
	OPERATOR 4 >=,
	OPERATOR 5 >,
	FUNCTION 1 base58check_cmp(base58check, base58check);

\echo Execute "CREATE EXTENSION pg_bitcoin_address;" to use this extension. \quit


--
-- Encoding/decoding functions
--

CREATE FUNCTION base58check_encode(bytea) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_base58check_encode';

CREATE FUNCTION base58check_decode(text) RETURNS bytea
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_base58check_decode';


CREATE FUNCTION bech32_encode(hrp text, bit varying) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_bech32_encode';

CREATE FUNCTION bech32m_encode(hrp text, bit varying) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_bech32m_encode';

CREATE FUNCTION bech32_decode(text) RETURNS bit varying
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_bech32_decode';

CREATE FUNCTION bech32m_decode(text) RETURNS bit varying
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_bech32m_decode';

CREATE FUNCTION bech32_hrp(text) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_bech32_hrp';


--
-- User-defined base types
--

CREATE TYPE base58check;

CREATE FUNCTION base58check_input(cstring) RETURNS base58check
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_base58check_input';

CREATE FUNCTION base58check_output(base58check) RETURNS cstring
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_base58check_output';

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


CREATE TYPE bitcoin_address;

CREATE FUNCTION bitcoin_address_input(cstring) RETURNS bitcoin_address
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_input';

CREATE FUNCTION bitcoin_address_output(bitcoin_address) RETURNS cstring
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 1000
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_output';

CREATE FUNCTION bitcoin_address_receive(internal) RETURNS bitcoin_address
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'bytearecv';

CREATE FUNCTION bitcoin_address_send(bitcoin_address) RETURNS bytea
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteasend';

CREATE TYPE bitcoin_address (
	INPUT = bitcoin_address_input,
	OUTPUT = bitcoin_address_output,
	RECEIVE = bitcoin_address_receive,
	SEND = bitcoin_address_send,
	LIKE = bytea,
	STORAGE = external
);


--
-- Constructor/accessor functions
--

CREATE FUNCTION bitcoin_address(hrp text, version integer, program bytea) RETURNS bitcoin_address
	LANGUAGE c IMMUTABLE PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address';

CREATE FUNCTION is_segwit(bitcoin_address) RETURNS boolean
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_is_segwit';

CREATE FUNCTION hrp(bitcoin_address) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_hrp';

CREATE FUNCTION version(bitcoin_address) RETURNS integer
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_version';

CREATE FUNCTION program(bitcoin_address) RETURNS bytea
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_program';


--
-- Convenience functions
--

CREATE FUNCTION is_mainnet(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN hrp(address) = 'bc'
		ELSE version(address) IN (0, 5)
	END;

CREATE FUNCTION is_testnet(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN hrp(address) = 'tb'
		ELSE version(address) IN (111, 196)
	END;

CREATE FUNCTION is_p2pkh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN NOT is_segwit(address) AND version(address) IN (0, 111);

CREATE FUNCTION is_p2sh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN NOT is_segwit(address) AND version(address) IN (5, 196);

CREATE FUNCTION is_p2wpkh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 0 AND length(program(address)) = 20;

CREATE FUNCTION is_p2wsh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 0 AND length(program(address)) = 32;

CREATE FUNCTION is_p2tr(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 1 AND length(program(address)) = 32;


--
-- Convenience domains
--

CREATE DOMAIN mainnet_address AS bitcoin_address CHECK (is_mainnet(VALUE));

CREATE DOMAIN testnet_address AS bitcoin_address CHECK (is_testnet(VALUE));


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


CREATE FUNCTION bitcoin_address_cmp(bitcoin_address, bitcoin_address) RETURNS integer
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteacmp';

CREATE FUNCTION bitcoin_address_eq(bitcoin_address, bitcoin_address) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteaeq';

CREATE FUNCTION bitcoin_address_ge(bitcoin_address, bitcoin_address) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteage';

CREATE FUNCTION bitcoin_address_gt(bitcoin_address, bitcoin_address) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteagt';

CREATE FUNCTION bitcoin_address_le(bitcoin_address, bitcoin_address) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'byteale';

CREATE FUNCTION bitcoin_address_lt(bitcoin_address, bitcoin_address) RETURNS boolean
	LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE
	AS 'bytealt';

CREATE FUNCTION bitcoin_address_ne(bitcoin_address, bitcoin_address) RETURNS boolean
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


CREATE OPERATOR < (
	FUNCTION = bitcoin_address_lt,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR <= (
	FUNCTION = bitcoin_address_le,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR = (
	FUNCTION = bitcoin_address_eq,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR >= (
	FUNCTION = bitcoin_address_ge,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR > (
	FUNCTION = bitcoin_address_gt,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR <> (
	FUNCTION = bitcoin_address_ne,
	LEFTARG = bitcoin_address,
	RIGHTARG = bitcoin_address
);

CREATE OPERATOR CLASS bitcoin_address_ops DEFAULT FOR TYPE bitcoin_address
	USING btree AS
	OPERATOR 1 <,
	OPERATOR 2 <=,
	OPERATOR 3 =,
	OPERATOR 4 >=,
	OPERATOR 5 >,
	FUNCTION 1 bitcoin_address_cmp(bitcoin_address, bitcoin_address);

\echo Execute "CREATE EXTENSION pg_bitcoin_address;" to use this extension. \quit


--
-- Encoding/decoding functions
--

CREATE FUNCTION blech32_encode(hrp text, bit varying) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_blech32_encode';

CREATE FUNCTION blech32m_encode(hrp text, bit varying) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_blech32m_encode';

CREATE FUNCTION blech32_decode(text) RETURNS bit varying
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_blech32_decode';

CREATE FUNCTION blech32m_decode(text) RETURNS bit varying
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_blech32m_decode';

CREATE FUNCTION blech32_hrp(text) RETURNS text
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE COST 10
	AS 'MODULE_PATHNAME', 'pg_blech32_hrp';


--
-- Constructor/accessor functions
--

CREATE FUNCTION bitcoin_address(hrp text, version integer, program bytea, blech boolean) RETURNS bitcoin_address
	LANGUAGE c IMMUTABLE PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address';

CREATE FUNCTION is_blech32(bitcoin_address) RETURNS boolean
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_is_blech32';

CREATE FUNCTION program_size(bitcoin_address) RETURNS integer
	LANGUAGE c IMMUTABLE STRICT PARALLEL SAFE
	AS 'MODULE_PATHNAME', 'pg_bitcoin_address_program_size';


--
-- Convenience functions
--

CREATE OR REPLACE FUNCTION is_mainnet(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN NOT is_blech32(address) AND hrp(address) = 'bc'
		ELSE program_size(address) = 20 AND version(address) IN (0, 5)
	END;

CREATE OR REPLACE FUNCTION is_testnet(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN NOT is_blech32(address) AND hrp(address) = 'tb'
		ELSE program_size(address) = 20 AND version(address) IN (111, 196)
	END;

CREATE FUNCTION is_liquidv1(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN hrp(address) = CASE WHEN is_blech32(address) THEN 'lq' ELSE 'ex' END
		ELSE version(address) = ANY(CASE program_size(address)
			WHEN 20 THEN ARRAY[57, 39]
			WHEN 54 THEN ARRAY[12]
		END) IS TRUE
	END;

CREATE FUNCTION is_liquidtestnet(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN hrp(address) = CASE WHEN is_blech32(address) THEN 'tlq' ELSE 'tex' END
		ELSE version(address) = ANY(CASE program_size(address)
			WHEN 20 THEN ARRAY[36, 19]
			WHEN 54 THEN ARRAY[23]
		END) IS TRUE
	END;

CREATE OR REPLACE FUNCTION is_p2pkh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN NOT is_segwit(address) AND version(address) = ANY(CASE program_size(address)
		WHEN 20 THEN ARRAY[0, 111, 57, 36]
		WHEN 54 THEN CASE get_byte(program(address), 0)
			WHEN 57 THEN ARRAY[12]
			WHEN 36 THEN ARRAY[23]
		END
	END) IS TRUE;

CREATE OR REPLACE FUNCTION is_p2sh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN NOT is_segwit(address) AND version(address) = ANY(CASE program_size(address)
		WHEN 20 THEN ARRAY[5, 196, 39, 19]
		WHEN 54 THEN CASE get_byte(program(address), 0)
			WHEN 39 THEN ARRAY[12]
			WHEN 19 THEN ARRAY[23]
		END
	END) IS TRUE;

CREATE OR REPLACE FUNCTION is_p2wpkh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 0 AND
		program_size(address) = CASE WHEN is_blech32(address) THEN 53 ELSE 20 END;

CREATE OR REPLACE FUNCTION is_p2wsh(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 0 AND
		program_size(address) = CASE WHEN is_blech32(address) THEN 65 ELSE 32 END;

CREATE OR REPLACE FUNCTION is_p2tr(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN is_segwit(address) AND version(address) = 1 AND
		program_size(address) = CASE WHEN is_blech32(address) THEN 65 ELSE 32 END;

CREATE FUNCTION is_blinding(address bitcoin_address) RETURNS boolean
	IMMUTABLE STRICT PARALLEL SAFE
	RETURN CASE
		WHEN is_segwit(address) THEN program_size(address) = ANY(CASE version(address)
			WHEN 0 THEN ARRAY[53, 65]
			WHEN 1 THEN ARRAY[65]
		END) IS TRUE
		ELSE version(address) IN (12, 23) AND program_size(address) = 54
	END;


--
-- Convenience domains
--

CREATE DOMAIN liquidv1_address AS bitcoin_address CHECK (is_liquidv1(VALUE));

CREATE DOMAIN liquidtestnet_address AS bitcoin_address CHECK (is_liquidtestnet(VALUE));

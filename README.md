# pg_bitcoin_address

This is a PostgreSQL extension that provides functions for encoding and decoding Base58Check and Bech32/Bech32m as well as user-defined base types to hold Base58Check-encoded data and Bitcoin addresses.
The upshot is that you can insert and select human-readable native SegWit addresses and legacy Bitcoin addresses and Base58Check-encoded strings, while PostgreSQL stores the raw bytes to save space and improve performance.
You get input validation “for free” when you use the base types in your tables.

## Prerequisites

This extension depends on [libbase58check][] and [libbech32][].

[libbase58check]: https://github.com/whitslack/libbase58check
[libbech32]: https://github.com/whitslack/libbech32

## Building

You need pkg-config and PostgreSQL installed. Then building and installing this extension is simply `make` and `make install`.

## Instantiating

You can instantiate the extension in your default schema:
```sql
=> CREATE EXTENSION pg_bitcoin_address;
CREATE EXTENSION
```

Or you can create a dedicated schema to host the extension:
```sql
=> CREATE SCHEMA bitcoin_address;
CREATE SCHEMA

=> CREATE EXTENSION pg_bitcoin_address WITH SCHEMA bitcoin_address;
CREATE EXTENSION

=> SET search_path TO public, bitcoin_address;
SET
```

## Functions

### Base58Check encoding/decoding

* **`base58check_encode(bytea)` → `text`**  
    Encodes a binary string using Base58Check.
    * `base58check_encode('\x123456'::bytea)` → `h1iFS7nKb`
* **`base58check_decode(text)` → `bytea`**  
    Decodes a Base58Check encoding into a binary string.
    * `base58check_decode('h1iFS7nKb')` → `\x123456`

### Bech32/Bech32m encoding/decoding

* **<code>bech32_encode(<em>hrp</em> text, bit varying)</code> → `text`**  
    Encodes a bit string using Bech32 with the given human-readable prefix.
    The bit string will be padded on the right with `0` bits as necessary to bring its length up to a whole multiple of 5 bits.
    * `bech32_encode('bc', B'101010')` → `bc14qv6z84v`
* **<code>bech32m_encode(<em>hrp</em> text, bit varying)</code> → `text`**  
    Encodes a bit string using Bech32m with the given human-readable prefix.
    The bit string will be padded on the right with `0` bits as necessary to bring its length up to a whole multiple of 5 bits.
    * `bech32m_encode('bc', B'101010')` → `bc14qexjtsw`
* **`bech32_decode(text)` → `bit varying`**  
    Decodes a Bech32 encoding into a bit string, whose length will always be a whole multiple of 5 bits.
    * `bech32_decode('bc14qv6z84v')` → `1010100000`
* **`bech32m_decode(text)` → `bit varying`**  
    Decodes a Bech32m encoding into a bit string, whose length will always be a whole multiple of 5 bits.
    * `bech32m_decode('bc14qexjtsw')` → `1010100000`
* **`bech32_hrp(text)` → `text`**  
    Returns the human-readable prefix of the given Bech32/Bech32m encoding.
    * `bech32_hrp('bc14qexjtsw')` → `bc`

### Bitcoin addresses

* **<code>bitcoin_address(<em>hrp</em> text, <em>version</em> integer, <em>program</em> bytea)</code> → `bitcoin_address`**  
    Constructs a `bitcoin_address` from the given human-readable prefix, version, and program.
    If *`hrp`* is null, then the address will be a legacy address constructed by prepending *`version`* (as a byte) to *`program`*, and the resulting address will use Base58Check encoding when presented textually.
    If *`hrp`* is not null, then the address will be a native SegWit address having *`version`* as its witness version and *`program`* as its witness program, and the resulting address will use Bech32 or Bech32m (depending on the witness version) when presented textually.
    * `bitcoin_address(NULL, 0, '\x759d6677091e973b9e9d99f19c68fbf43e3f05f9'::bytea)` → `1BitcoinEaterAddressDontSendf59kuE`
    * `bitcoin_address(NULL, 5, '\x759d6677091e973b9e9d99f19c68fbf43e3f05f9'::bytea)` → `3CQuYMDDnVD2wLL4ykYTeS9pbB5MCgiYUV`
    * `bitcoin_address('bc', 2, '\x751e76e8199196d454941c45d1b3a323'::bytea)` → `bc1zw508d6qejxtdg4y5r3zarvaryvaxxpcs`
    * `bitcoin_address('bc', 16, '\x751e'::bytea)` → `bc1sw50qgdz25j`
* **`is_segwit(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a native SegWit address.
* **`hrp(bitcoin_address)` → `text`**  
    Returns the human-readable prefix of a native SegWit address.
    Returns null if the given address is not a native SegWit address.
* **`version(bitcoin_address)` → `integer`**  
    Returns the version of the given Bitcoin address,
    which is either the version byte of a legacy address or the witness version of a native SegWit address.
* **`program(bitcoin_address)` → `bytea`**  
    Returns the program of the given Bitcoin address,
    which is either the bytes following the version byte of a legacy address or the witness program of a native SegWit address.
    * `program('1BitcoinEaterAddressDontSendf59kuE'::bitcoin_address)` → `\x759d6677091e973b9e9d99f19c68fbf43e3f05f9`
    * `program('3CQuYMDDnVD2wLL4ykYTeS9pbB5MCgiYUV'::bitcoin_address)` → `\x759d6677091e973b9e9d99f19c68fbf43e3f05f9`
    * `program('bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'::bitcoin_address)` → `\x751e76e8199196d454941c45d1b3a323f1433bd6`
    * `program('bc1sw50qgdz25j'::bitcoin_address)` → `\x751e`
* **`is_mainnet(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a Mainnet address.
* **`is_testnet(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a Testnet address.
* **`is_p2pkh(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a pay-to-public-key-hash (P2PKH) address.
* **`is_p2sh(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a pay-to-script-hash (P2SH) address.
* **`is_p2wpkh(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a pay-to-witness-public-key-hash (P2WPKH) address.
* **`is_p2wsh(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a pay-to-witness-script-hash (P2WSH) address.
* **`is_p2tr(bitcoin_address)` → `boolean`**  
    Returns whether the given Bitcoin address is a pay-to-taproot (P2TR) address.

## Types

### `base58check`

The `base58check` type holds a binary string exactly like a `bytea` but presents it in Base58Check.

```sql
=> SELECT 'h1iFS7nKb'::base58check::bytea;
bytea | \x123456

=> SELECT '\x123456'::bytea::base58check;
base58check | h1iFS7nKb

=> SELECT pg_column_size('h1iFS7nKb'::text);
pg_column_size | 13

=> SELECT pg_column_size('h1iFS7nKb'::base58check);
pg_column_size | 7

=> SELECT pg_column_size('\x123456'::bytea);
pg_column_size | 7
```

### `bitcoin_address`

The `bitcoin_address` type holds either a legacy Bitcoin address or a native SegWit address.
It stores the bytes of the address in raw binary format and only encodes to Base58Check or Bech32/Bech32m for presentation.

```sql
=> SELECT pg_column_size('1BitcoinEaterAddressDontSendf59kuE'::text);
pg_column_size | 38

=> SELECT pg_column_size('1BitcoinEaterAddressDontSendf59kuE'::bitcoin_address);
pg_column_size | 26

=> SELECT pg_column_size('bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'::text);
pg_column_size | 46

=> SELECT pg_column_size('bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'::bitcoin_address);
pg_column_size | 26
```

## Domains

### `mainnet_address`

A `mainnet_address` is a `bitcoin_address` that is constrained such that passing it to `is_mainnet` must return true.

```sql
=> SELECT '1BitcoinEaterAddressDontSendf59kuE'::mainnet_address;
mainnet_address | 1BitcoinEaterAddressDontSendf59kuE

=> SELECT 'bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'::mainnet_address;
mainnet_address | bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4

=> SELECT 'mrEqurom3cKudH7FaDrF3j1DJePLcjAU3m'::mainnet_address;
ERROR:  value for domain mainnet_address violates check constraint "mainnet_address_check"

=> SELECT 'tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx'::mainnet_address;
ERROR:  value for domain mainnet_address violates check constraint "mainnet_address_check"
```

### `testnet_address`

A `testnet_address` is a `bitcoin_address` that is constrained such that passing it to `is_testnet` must return true.

```sql
=> SELECT 'mrEqurom3cKudH7FaDrF3j1DJePLcjAU3m'::testnet_address;
testnet_address | mrEqurom3cKudH7FaDrF3j1DJePLcjAU3m

=> SELECT 'tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx'::testnet_address;
testnet_address | tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx

=> SELECT '1BitcoinEaterAddressDontSendf59kuE'::testnet_address;
ERROR:  value for domain testnet_address violates check constraint "testnet_address_check"

=> SELECT 'bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'::testnet_address;
ERROR:  value for domain testnet_address violates check constraint "testnet_address_check"
```

## Examples

```sql
=> CREATE TEMPORARY TABLE addresses (a bitcoin_address UNIQUE);
CREATE TABLE

=> INSERT INTO addresses VALUES
	('1BitcoinEaterAddressDontSendf59kuE'),
	('mrEqurom3cKudH7FaDrF3j1DJePLcjAU3m'),
	('3CQuYMDDnVD2wLL4ykYTeS9pbB5MCgiYUV'),
	('2N3y7c69FPwiP97xcetALGP95oXHWzPNHEk'),
	('bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4'),
	('tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx'),
	('bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3'),
	('tb1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q0sl5k7'),
	('bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0'),
	('tb1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq47zagq'),
	('bc1sw50qgdz25j'),
	('tb1sw50qadvs0e');
INSERT 0 12

=> SELECT a AS bitcoin_address, is_segwit(a) AS sw, hrp(a), version(a) AS ver, is_mainnet(a) AS main, is_testnet(a) AS test, is_p2pkh(a) AS pkh, is_p2sh(a) AS sh, is_p2wpkh(a) AS wpkh, is_p2wsh(a) AS wsh, is_p2tr(a) AS tr FROM addresses;
                        bitcoin_address                         | sw | hrp | ver | main | test | pkh | sh | wpkh | wsh | tr 
----------------------------------------------------------------+----+-----+-----+------+------+-----+----+------+-----+----
 1BitcoinEaterAddressDontSendf59kuE                             | f  |     |   0 | t    | f    | t   | f  | f    | f   | f
 mrEqurom3cKudH7FaDrF3j1DJePLcjAU3m                             | f  |     | 111 | f    | t    | t   | f  | f    | f   | f
 3CQuYMDDnVD2wLL4ykYTeS9pbB5MCgiYUV                             | f  |     |   5 | t    | f    | f   | t  | f    | f   | f
 2N3y7c69FPwiP97xcetALGP95oXHWzPNHEk                            | f  |     | 196 | f    | t    | f   | t  | f    | f   | f
 bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4                     | t  | bc  |   0 | t    | f    | f   | f  | t    | f   | f
 tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx                     | t  | tb  |   0 | f    | t    | f   | f  | t    | f   | f
 bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3 | t  | bc  |   0 | t    | f    | f   | f  | f    | t   | f
 tb1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3q0sl5k7 | t  | tb  |   0 | f    | t    | f   | f  | f    | t   | f
 bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0 | t  | bc  |   1 | t    | f    | f   | f  | f    | f   | t
 tb1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vq47zagq | t  | tb  |   1 | f    | t    | f   | f  | f    | f   | t
 bc1sw50qgdz25j                                                 | t  | bc  |  16 | t    | f    | f   | f  | f    | f   | f
 tb1sw50qadvs0e                                                 | t  | tb  |  16 | f    | t    | f   | f  | f    | f   | f
(12 rows)

=> INSERT INTO addresses VALUES ('1BitcoinEaterAddressDontSendf59kuE');
ERROR:  duplicate key value violates unique constraint "addresses_a_key"
DETAIL:  Key (a)=(1BitcoinEaterAddressDontSendf59kuE) already exists.

=> INSERT INTO addresses VALUES ('1BitcoinEaterAddressDontSendffffff');
ERROR:  not a valid Bitcoin address
DETAIL:  1BitcoinEaterAddressDontSendffffff

=> SELECT program('1BitcoinEaterAddressDontSendf59kuE'::bitcoin_address);
program | \x759d6677091e973b9e9d99f19c68fbf43e3f05f9

=> SELECT program('h1iFS7nKb'::bitcoin_address);
ERROR:  not a valid Bitcoin address
DETAIL:  h1iFS7nKb

=> SELECT 'h1iFS7nKb'::base58check::bytea;
bytea | \x123456
```

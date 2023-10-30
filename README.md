# pg_base58check

This is a PostgreSQL extension with functions for encoding and decoding Base58Check and a user-defined base type that extends `bytea` with casts to and from `text` that perform Base58Check encoding and decoding. The upshot is that you can insert and select human-readable Base58Check strings, but PostgreSQL will store the raw bytes to save space and improve performance. You get input validation "for free" when you use the `base58check` type in your tables.

```
=> CREATE EXTENSION pg_base58check;
CREATE EXTENSION
```

```
=> SELECT base58check_decode('1BitcoinEaterAddressDontSendf59kuE');
base58check_decode | \x00759d6677091e973b9e9d99f19c68fbf43e3f05f9

=> SELECT base58check_decode('1BitcoinEaterAddressDontSendFFFFFF');
ERROR:  not a valid Base58Check encoding

=> CREATE TABLE addresses (
	user_id bigint NOT NULL,
	address base58check NOT NULL UNIQUE,
	PRIMARY KEY (user_id, address)
   );
CREATE TABLE

=> INSERT INTO addresses VALUES (21, '1BitcoinEaterAddressDontSendf59kuE');
INSERT 0 1

=> SELECT * FROM addresses;
user_id | 21
address | 1BitcoinEaterAddressDontSendf59kuE

=> SELECT user_id, address::bytea FROM addresses;
user_id | 21
address | \x00759d6677091e973b9e9d99f19c68fbf43e3f05f9

=> SELECT '\x00759d6677091e973b9e9d99f19c68fbf43e3f05f9'::bytea::base58check;
base58check | 1BitcoinEaterAddressDontSendf59kuE
```

## Prerequisites

This extension depends on [libbase58check](https://github.com/whitslack/libbase58check).

## Building

You need pkg-config and PostgreSQL installed. Then building and installing this extension is simply `make` and `make install`.

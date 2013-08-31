//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
//===========================================================================================================
//
// MsgPack is an external data representation format which is compact and rather efficient.
// This file implements version v5 described at https://gist.github.com/frsyuki/5432559
// It is byte-oriented and fits well for compact network serialization.
// See end of file for definition of the supported types representations.
//
#pragma once

#include <boost/endian/conversion2.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include "archive_helper.h"
#include "byte_array.h"
#include "binary_literal.h"

namespace msgpack {

class encode_error : public std::exception
{};

class decode_error : public std::exception
{};

template<class Archive>
inline void encode_boolean(Archive& oa, const bool flag)
{
    uint8_t true_value = 0xc3;
    uint8_t false_value = 0xc2;
    oa << (flag ? true_value : false_value);
}

template<class Archive>
inline bool decode_boolean(Archive& ia)
{
    uint8_t flag;
    ia >> flag;
    if (flag == 0xc3)
        return true;
    if (flag == 0xc2)
        return false;
    throw decode_error();
}

// Encode msgpack vector: fixed-size array.
// Msgpack arrays always include length, as it is a tagged format, but we break
// compatibility here a little by making untagged direct writes with given size.
// Client must know the exact size, no padding.
template<class Archive>
inline void encode_vector(Archive& oa, const byte_array& ba, uint32_t maxlen)
{
    assert(ba.length() == maxlen);
    oa << ba;
}

template<class Archive>
inline void decode_vector(Archive& ia, byte_array& ba, uint32_t maxlen)
{
    ba.resize(maxlen);
    ia.load_binary(ba.data(), ba.length());
}

// Encode msgpack bin: variable-size byte array.
template<class Archive>
inline void encode_array(Archive& oa, const byte_array& ba, uint32_t maxlen)
{
    assert(ba.length() <= maxlen);
    uint32_t size = ba.length();
    uint8_t flag;

    if (size < 256) // bin8
    {
        uint8_t ssize = size & 0xff;
        flag = 0xc4;
        oa << flag << ssize;
    }
    else if (size < 65536) // bin16
    {
        uint16_t ssize = size & 0xffff;
        ssize = boost::endian2::big(ssize);

        flag = 0xc5;
        oa << flag << ssize;
    }
    else if (size <= 0xffffffff) // bin32
    {
        uint32_t ssize = size & 0xffffffff; // for the time when size will be 64 bit
        ssize = boost::endian2::big(ssize);

        flag = 0xc6;
        oa << flag << ssize;
    }
    else {
        throw encode_error();
    }
    oa << ba;
}

template<class Archive>
inline void decode_array(Archive& ia, byte_array& ba, uint32_t maxlen)
{
    uint64_t size;
    uint8_t flag;

    ia >> flag;

    if (flag == 0xc4) // bin8
    {
        uint8_t ssize = size & 0xff;
        ia >> ssize;
        size = ssize;
    }
    else if (flag == 0xc5) // bin16
    {
        uint16_t ssize;
        ia >> ssize;
        size = boost::endian2::big(ssize);
    }
    else if (flag == 0xc6) // bin32
    {
        uint32_t ssize;
        ia >> ssize;
        size = boost::endian2::big(ssize);
    }
    else {
        throw decode_error();
    }

    if (size > maxlen)
        throw decode_error();
    ba.resize(size);
    ia >> ba;
}

// Encode msgpack list: variable-size array of arbitrary types.
template<class Archive, typename T>
inline void
encode_list(Archive& oa, const std::vector<T>& ba, uint32_t maxlen)
{
    assert(ba.size() <= maxlen);
    uint32_t size = ba.size();
    uint8_t flag;

    if (size < 16) // use fixarray
    {
        flag = 10010000_b | (size & 0xf);
        oa << flag;
    }
    else if (size < 65536) // array 16
    {
        uint16_t ssize = size & 0xffff;
        ssize = boost::endian2::big(ssize);

        flag = 0xdc;
        oa << flag << ssize;
    }
    else if (size <= 0xffffffff) // array 32
    {
        uint32_t ssize = size & 0xffffffff; // for the time when size will be 64 bit
        ssize = boost::endian2::big(ssize);

        flag = 0xdd;
        oa << flag << ssize;
    }
    else {
        throw encode_error();
    }

    for (uint32_t index = 0; index < ba.size(); ++index)
        oa << ba[index];
}

template<class Archive, typename T>
inline void decode_list(Archive& ia, std::vector<T>& ba, uint32_t maxlen)
{
    uint64_t size;
    uint8_t flag;

    ia >> flag;

    if ((flag & 10010000_b) == 10010000_b) // use fixarray
    {
        size = flag & 0xf;
    }
    else if (flag == 0xdc) // array 16
    {
        uint16_t ssize;
        ia >> ssize;
        size = boost::endian2::big(ssize);
    }
    else if (flag == 0xdd) // array 32
    {
        uint32_t ssize;
        ia >> ssize;
        size = boost::endian2::big(ssize);
    }
    else {
        throw decode_error();
    }

    if (size > maxlen)
        throw decode_error();
    ba.resize(size);
    for (uint32_t index = 0; index < size; ++index)
        ia >> ba[index];
}

// Regular option type over boost.optional is defined in custom_optional.h

// Length-delimited option type.
//
// Store an optional chunk into a substream, then feed it and its size into the archive.
template<class Archive, typename T>
inline void encode_option(Archive& oa, const T& t, uint32_t maxlen)
{
    byte_array arr;
    {
        byte_array_owrap<boost::archive::binary_oarchive> w(arr);
        w.archive() << t;
    }

    uint32_t size = arr.size();
    size = boost::endian2::big(size);

    oa << size << arr; // TODO: this is not very well thought out... also mismatches description of format below
}

template<class Archive, typename T>
inline void decode_option(Archive& ia, T& t, uint32_t maxlen)
{
    uint32_t size;
    ia >> size;
    size = boost::endian2::big(size);

    if (size > maxlen) // FIXME: not checking for 0 size?
        return;

    byte_array arr;
    arr.resize(size);

    ia >> arr;

    {
        boost::iostreams::filtering_istream iv(boost::make_iterator_range(arr.as_vector()));
        boost::archive::binary_iarchive ia_buf(iv, boost::archive::no_header);
        ia_buf >> t;
    }
}

} // namespace msgpack

/**
The encoding method looks similar to that of UTF-8.

nil
integer, limited between -(2^63) up to (2^64)-1
boolean, true or false
float, single or double precision IEEE 754
string, with maximum length up to (2^32)-1
binary, with maximum length up to (2^32)-1
array  (sequence), with maximum number of entries up to (2^32)-1
map, with maximum number of entries up to (2^32)-1
extended, with up to 128 custom application specific types

one byte:
+--------+
|        |
+--------+

a variable number of bytes:
+========+
|        |
+========+

variable number of objects stored in MessagePack format:
+~~~~~~~~~~~~~~~~~+
|                 |
+~~~~~~~~~~~~~~~~~+

* X, Y, Z, A, B are individual bits
* N is the length of data

== nil format ==

nil:
+--------+
|  0xc0  |
+--------+

== bool format ==

false:
+--------+
|  0xc2  |
+--------+

true:
+--------+
|  0xc3  |
+--------+

== int format ==

positive fixnum stores 7-bit positive integer
+--------+
|0XXXXXXX|
+--------+

negative fixnum stores 5-bit negative integer
+--------+
|111YYYYY|
+--------+

* 0XXXXXXX is 8-bit unsigned integer
* 111YYYYY is 8-bit signed integer (sign-extended)

uint 8 stores a 8-bit unsigned integer
+--------+--------+
|  0xcc  |ZZZZZZZZ|
+--------+--------+

uint 16 stores a 16-bit big-endian unsigned integer
+--------+--------+--------+
|  0xcd  |ZZZZZZZZ|ZZZZZZZZ|
+--------+--------+--------+

uint 32 stores a 32-bit big-endian unsigned integer
+--------+--------+--------+--------+--------+
|  0xce  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ
+--------+--------+--------+--------+--------+

uint 64 stores a 64-bit big-endian unsigned integer
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|  0xcf  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|
+--------+--------+--------+--------+--------+--------+--------+--------+--------+

int 8 stores a 8-bit signed integer
+--------+--------+
|  0xd0  |ZZZZZZZZ|
+--------+--------+

int 16 stores a 16-bit big-endian signed integer
+--------+--------+--------+
|  0xd1  |ZZZZZZZZ|ZZZZZZZZ|
+--------+--------+--------+

int 32 stores a 32-bit big-endian signed integer
+--------+--------+--------+--------+--------+
|  0xd2  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|
+--------+--------+--------+--------+--------+

int 64 stores a 64-bit big-endian signed integer
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|  0xd3  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|
+--------+--------+--------+--------+--------+--------+--------+--------+--------+

== float format ==

float 32 stores a floating point number in IEEE 754 single precision
floating point big-endian number format:
+--------+--------+--------+--------+--------+
|  0xca  |XXXXXXXX|XXXXXXXX|XXXXXXXX|XXXXXXXX
+--------+--------+--------+--------+--------+
         | MSB    |                 | LSB    |
         <--------------32 bits-------------->

float 64 stores a floating point number in IEEE 754 double precision
floating point big-endian number format:
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|  0xca  |YYYYYYYY|YYYYYYYY|YYYYYYYY|YYYYYYYY|YYYYYYYY|YYYYYYYY|YYYYYYYY|YYYYYYYY|
+--------+--------+--------+--------+--------+--------+--------+--------+--------+
         | MSB    |                                                     | LSB    |
         <-----------------------------64 bits----------------------------------->

== bin format (variable length opaque) ==

bin 8 stores a byte array whose length is upto (2^8)-1 bytes:
+--------+--------+========+
|  0xc4  |XXXXXXXX|  data  |
+--------+--------+========+
* XXXXXXXX is a 8-bit unsigned integer which represents N

bin 16 stores a byte array whose length is upto (2^16)-1 bytes:
+--------+--------+--------+========+
|  0xc5  |YYYYYYYY|YYYYYYYY|  data  |
+--------+--------+--------+========+
* YYYYYYYY_YYYYYYYY is a 16-bit big-endian unsigned integer which represents N

bin 32 stores a byte array whose length is upto (2^32)-1 bytes:
+--------+--------+--------+--------+--------+========+
|  0xc6  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|  data  |
+--------+--------+--------+--------+--------+========+
* ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ is a 32-bit big-endian unsigned integer
which represents N

== string format ==

(variable length utf8 string, possibly with corrupt codepoints)

fixstr stores a byte array whose length is upto 31 bytes:
+--------+========+
|101XXXXX|  data  |
+--------+========+
* XXXXX is a 5-bit unsigned integer which represents N

str 8 stores a byte array whose length is upto (2^8)-1 bytes:
+--------+--------+========+
|  0xd9  |YYYYYYYY|  data  |
+--------+--------+========+
* YYYYYYYY is a 8-bit unsigned integer which represents N

str 16 stores a byte array whose length is upto (2^16)-1 bytes:
+--------+--------+--------+========+
|  0xda  |ZZZZZZZZ|ZZZZZZZZ|  data  |
+--------+--------+--------+========+
* ZZZZZZZZ_ZZZZZZZZ is a 16-bit big-endian unsigned integer which represents N

str 32 stores a byte array whose length is upto (2^32)-1 bytes:
+--------+--------+--------+--------+--------+========+
|  0xdb  |AAAAAAAA|AAAAAAAA|AAAAAAAA|AAAAAAAA|  data  |
+--------+--------+--------+--------+--------+========+
* AAAAAAAA_AAAAAAAA_AAAAAAAA_AAAAAAAA is a 32-bit big-endian unsigned integer
which represents N

== array format ==

fixarray stores an array whose length is upto 15 elements:
0x9X
+--------+~~~~~~~~~~~~~~~~~+
|1001XXXX|    N objects    |
+--------+~~~~~~~~~~~~~~~~~+
* XXXX is a 4-bit unsigned integer which represents N

array 16 stores an array whose length is upto (2^16)-1 elements:
+--------+--------+--------+~~~~~~~~~~~~~~~~~+
|  0xdc  |YYYYYYYY|YYYYYYYY|    N objects    |
+--------+--------+--------+~~~~~~~~~~~~~~~~~+
* YYYYYYYY_YYYYYYYY is a 16-bit big-endian unsigned integer which represents N

array 32 stores an array whose length is upto (2^32)-1 elements:
+--------+--------+--------+--------+--------+~~~~~~~~~~~~~~~~~+
|  0xdd  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|    N objects    |
+--------+--------+--------+--------+--------+~~~~~~~~~~~~~~~~~+
* ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ is a 32-bit big-endian unsigned integer
which represents N

== map format ==

* odd elements in objects are keys of a map
* the next element of a key is its associated value

fixmap stores a map whose length is upto 15 elements
0x8X
+--------+~~~~~~~~~~~~~~~~~+
|1000XXXX|   N*2 objects   |
+--------+~~~~~~~~~~~~~~~~~+
* XXXX is a 4-bit unsigned integer which represents N

map 16 stores a map whose length is upto (2^16)-1 elements
+--------+--------+--------+~~~~~~~~~~~~~~~~~+
|  0xde  |YYYYYYYY|YYYYYYYY|   N*2 objects   |
+--------+--------+--------+~~~~~~~~~~~~~~~~~+
* YYYYYYYY_YYYYYYYY is a 16-bit big-endian unsigned integer which represents N

map 32 stores a map whose length is upto (2^32)-1 elements
+--------+--------+--------+--------+--------+~~~~~~~~~~~~~~~~~+
|  0xdf  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|   N*2 objects   |
+--------+--------+--------+--------+--------+~~~~~~~~~~~~~~~~~+
* ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ is a 32-bit big-endian unsigned integer
which represents N

== extended format ==

* type is a signed 8-bit signed integer
* type < 0 is reserved for future extension including 2-byte type information

fixext 1 stores an integer and a byte array whose length is 1 byte
+--------+--------+--------+
|  0xd4  |  type  |  data  |
+--------+--------+--------+

fixext 2 stores an integer and a byte array whose length is 2 bytes
+--------+--------+--------+--------+
|  0xd5  |  type  |       data      |
+--------+--------+--------+--------+

fixext 4 stores an integer and a byte array whose length is 4 bytes
+--------+--------+--------+--------+--------+--------+
|  0xd6  |  type  |                data               |
+--------+--------+--------+--------+--------+--------+

fixext 8 stores an integer and a byte array whose length is 8 bytes
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|  0xd7  |  type  |                                  data                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+

fixext 16 stores an integer and a byte array whose length is 16 bytes
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
|  0xd8  |  type  |                                  data                                  
+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
+--------+--------+--------+--------+--------+--------+--------+--------+
                              data (cont.)                              |
+--------+--------+--------+--------+--------+--------+--------+--------+

ext 8 stores an integer and a byte array whose length is upto (2^8)-1 bytes:
+--------+--------+--------+========+
|  0xc7  |XXXXXXXX|  type  |  data  |
+--------+--------+--------+========+
* XXXXXXXX is a 8-bit unsigned integer which represents N

ext 16 stores an integer and a byte array whose length is upto (2^16)-1 bytes:
+--------+--------+--------+--------+========+
|  0xc8  |YYYYYYYY|YYYYYYYY|  type  |  data  |
+--------+--------+--------+--------+========+
* YYYYYYYY_YYYYYYYY is a 16-bit big-endian unsigned integer which represents N

ext 32 stores an integer and a byte array whose length is upto (2^32)-1 bytes:
+--------+--------+--------+--------+--------+--------+========+
|  0xc9  |ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|ZZZZZZZZ|  type  |  data  |
+--------+--------+--------+--------+--------+--------+========+
* ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ_ZZZZZZZZ is a big-endian 32-bit unsigned integer
which represents N

THINGS MISSING IN MSGPACK
* union
* void
* optional - can be represented as "obj or nil"
* length-delimited optional

They need no additional types and can be implemented as follows deriving from their
inherent structure.

union switch (discriminant-declaration) {
 case discriminant-value-A:
    arm-declaration-A;
 case discriminant-value-B:
    arm-declaration-B;
 ...
 default: default-declaration;
} identifier;

         +~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~+
         |  discriminant |  implied arm  |          DISCRIMINATED UNION
         +~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~+

Using msgpack it is easily representable by discriminant-declaration type first and
then structure for the implied arm.

Void:

void is entirely empty and is a notational class only, it does not take any space
in serialized form.

           ++
           ||                                                     VOID
           ++
         --><-- 0 bytes

Optional:

type-name *identifier;

This is equivalent to the following union:

 union switch (bool opted) {
 case TRUE:
    type-name element;
 case FALSE:
    void;
 } identifier;

Using msgpack it can be represented by a nil for FALSE arm or an object for TRUE arm.
The represented object must not be nil.

Length-delimited optional:

type-name ?identifier;

Uses size field at the start of an optional, to allow skipping unknown types.

Using msgpack can be represented with a size field first,
and then TRUE-arm if size is non-zero.

*/

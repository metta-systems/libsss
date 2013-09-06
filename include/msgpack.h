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

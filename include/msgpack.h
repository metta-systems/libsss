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

    oa << size << arr;
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

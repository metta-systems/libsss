// UNUSED: Remove!
#pragma once

// @todo Port length-delimited option and remove this file
//
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

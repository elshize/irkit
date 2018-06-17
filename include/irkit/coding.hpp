// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//!
//! \file
//! \author      Michal Siedlaczek
//! \copyright   MIT License

#pragma once

#include <iostream>

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/concept_interface.hpp>
#include <boost/type_erasure/rebind_any.hpp>

#include <irkit/bitptr.hpp>
#include <irkit/concepts.hpp>
#include <irkit/types.hpp>

namespace irk {

//! Encoder concept for type erasure.
template<class C, class T>
struct has_encode {
    static std::ostream& apply(const C& codec, T n, std::ostream& sink)
    {
        return codec.encode(n, sink);
    }
};

//! Decoder concept for type erasure.
template<class C, class T>
struct has_decode {
    static std::streamsize apply(const C& codec, std::istream& source, T& n)
    {
        return codec.decode(source, n);
    }
};

};  // namespace irk

namespace boost::type_erasure {

template<class C, class T, class Base>
struct concept_interface<irk::has_encode<C, T>, Base, C> : Base
{
    typename as_param<Base, std::ostream&>::type
    encode(typename as_param<Base, T>::type n, std::ostream& sink) const
    {
        return call(irk::has_encode<C, T>(), *this, n, sink);
    }
    using value_type = T;
};

template<class C, class T, class Base>
struct concept_interface<irk::has_decode<C, T>, Base, C> : Base
{
    std::streamsize
    decode(std::istream& source, typename as_param<Base, T&>::type n) const
    {
        return call(irk::has_decode<C, T>(), *this, source, n);
    }
    using value_type = T;
};

};  // namespace boost::type_erasure

namespace irk {

//! Type for any codec, i.e., an object able to encode and decode type `T`.
template<class T, class S = boost::type_erasure::_self>
using any_codec = boost::type_erasure::any<boost::mpl::vector<has_encode<S, T>,
    has_decode<S, T>,
    boost::type_erasure::copy_constructible<S>,
    boost::type_erasure::assignable<S>>>;

template<class, class, class = void>
struct has_pointer_decode : std::false_type {};

template<class Codec, class T>
struct has_pointer_decode<Codec,
    T,
    std::void_t<decltype(std::declval<T>().decode(std::declval<T>(),
        std::declval<std::ostream&>()))>> : std::true_type {};

// TODO:
//template<class T, class Codec>
//T decode(const char* data, const any_codec<T>& codec)
//{
//    if constexpr () {
//    }
//}

//! Encodes a range of integer values to an output stream.
/*!
    \param int_range a range of integers to encode
    \param sink      an output stream to write to
    \param codec     a codec used to encode integers
    \returns         `sink` stream
 */
template<class Codec, class InputRange>
std::ostream& encode(const InputRange& int_range,
    std::ostream& sink,
    const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((boost::InputIterator<iterator_t<InputRange>>));
    for (const auto& n : int_range) {
        codec.encode(n, sink);
    }
    return sink;
}

//! Encodes a range of integer values to a byte vector.
/*!
    \param int_range a range of integers to encode
    \param codec     a codec used to encode integers
    \returns         a vector of encoded bytes
 */
template<class Codec, class InputRange>
std::vector<char>
encode(const InputRange& int_range, const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
    std::vector<char> bytes;
    boost::iostreams::stream<
        boost::iostreams::back_insert_device<std::vector<char>>>
        buffer(boost::iostreams::back_inserter(bytes));
    encode(int_range, buffer, codec);
    return bytes;
}

//! Encodes an initializer list of integer values to a byte vector.
/*!
    \param integers  an initializer list of integers to encode
    \param codec     a codec used to encode integers
    \returns         a vector of encoded bytes
 */
template<class Codec>
std::vector<char>
encode(std::initializer_list<typename Codec::value_type> integers,
    const Codec& codec = Codec{})
{
    std::vector<typename Codec::value_type> v(integers);
    return encode(v, codec);
}

//! Encodes a range of values, mapped to integers, to an output stream.
/*!
    \param range     a range of values to map to integers and encode
    \param fn        a function mapping elements of `range` to integers
    \param sink      an output stream to write to
    \param codec     a codec used to encode integers
    \returns         `sink` stream
 */
template<class Codec, class InputRange, class TransformFn>
std::ostream& encode_fn(const InputRange& range,
    TransformFn fn,
    std::ostream& sink,
    const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
    BOOST_CONCEPT_ASSERT((boost::UnaryFunction<TransformFn,
        typename Codec::value_type,
        element_t<InputRange>>));
    for (const auto& n : range) {
        codec.encode(fn(n), sink);
    }
    return sink;
}

//! Encodes a range of values, mapped to integers, to a byte vector.
/*!
    \param range     a range of values to map to integers and encode
    \param fn        a function mapping elements of `range` to integers
    \param codec     a codec used to encode integers
    \returns         a vector of encoded bytes
 */
template<class Codec, class InputRange, class TransformFn>
std::vector<char>
encode_fn(const InputRange& range, TransformFn fn, const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
    BOOST_CONCEPT_ASSERT((boost::UnaryFunction<TransformFn,
        typename Codec::value_type,
        element_t<InputRange>>));
    std::vector<char> bytes;
    boost::iostreams::stream<
        boost::iostreams::back_insert_device<std::vector<char>>>
        buffer(boost::iostreams::back_inserter(bytes));
    encode_fn(range, fn, buffer, codec);
    return bytes;
}

//! Encodes a range of integers to an output stream, applying delta encoding.
/*!
    \param int_range     a range of integers to encode
    \param sink          an output stream to write to
    \param codec         a codec used to encode integers
    \param initial_value the initial value to subtract from the first encoded
                         integer (0 by default)
    \returns         `sink` stream
 */
template<class Codec, class InputRange>
std::ostream& encode_delta(const InputRange& int_range,
    std::ostream& sink,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
    typename Codec::value_type prev = initial_value;
    for (const auto& n : int_range) {
        codec.encode(n - prev, sink);
        prev = n;
    }
    return sink;
}

//! Encodes a range of integer values to a byte vector.
/*!
    \param int_range     a range of integers to encode
    \param codec         a codec used to encode integers
    \param initial_value the initial value to subtract from the first encoded
                         integer (0 by default)
    \returns             a vector of encoded bytes
 */
template<class Codec, class InputRange>
std::vector<char> encode_delta(const InputRange& int_range,
    const Codec& codec = Codec{},
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
    std::vector<char> bytes;
    boost::iostreams::stream<
        boost::iostreams::back_insert_device<std::vector<char>>>
        buffer(boost::iostreams::back_inserter(bytes));
    encode_delta(int_range, buffer, codec, initial_value);
    return bytes;
}


////! Prefix-encodes a range of strings values to a byte vector.
///*!
//    \param string_range  a range of values to encode
//    \param codec         a codec used to encode strings
//    \returns             a vector of encoded bytes
// */
//template<class Codec, class InputRange>
//std::vector<char>
//encode_delta(const InputRange& string_range, const Codec& codec)
//{
//    BOOST_CONCEPT_ASSERT((concept::InputRange<InputRange>));
//    std::vector<char> bytes;
//
//    return bytes;
//}

//! Decodes an entire input stream to an output iterator.
/*!
    \param output   an output iterator, such as `std::back_inserter`
    \param source   an input stream with the encoded elements
    \param codec    a codec to use for decoding
    \returns        `source` stream
 */
template<class Codec, class OutputIterator>
std::istream& decode(
    OutputIterator output, std::istream& source, const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT(
        (boost::OutputIterator<OutputIterator, typename Codec::value_type>));
    typename Codec::value_type val;
    while (codec.decode(source, val)) { *output++ = val; }
    return source;
}

//! Decodes a range of encoded bytes to a vector.
/*!
    \param source   a range of encoded bytes
    \param codec    a codec to use for decoding
    \returns        a vector of decoded values
 */
template<class Codec, class SourceRange>
std::vector<typename Codec::value_type>
decode(const SourceRange& source, const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((boost::InputIterator<iterator_t<SourceRange>>));
    std::vector<typename Codec::value_type> result;
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> buffer(
        source.data(), source.size());
    decode<Codec>(std::back_inserter(result), buffer, codec);
    return result;
}

//! Decodes an initializer list of bytes to a vector.
/*!
    \param bytes    an initializer list of bytes
    \param codec    a codec to use for decoding
    \returns        a vector of decoded values
 */
template<class Codec>
std::vector<typename Codec::value_type>
decode(std::initializer_list<char> bytes, const Codec& codec = Codec{})
{
    std::vector<char> v(bytes);
    return decode(v, codec);
}

//! Decodes `n` encoded symbols from an input stream to an output iterator.
/*!
    \param output   an output iterator, such as `std::back_inserter`
    \param source   an input stream with the encoded elements
    \param n        how many symbols to decode
    \param codec    a codec to use for decoding
    \returns        `source` stream
 */
template<class Codec, class OutputIterator>
std::istream& decode_n(OutputIterator output,
    std::istream& source,
    std::size_t n,
    const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT(
        (boost::OutputIterator<OutputIterator, typename Codec::value_type>));
    typename Codec::value_type integer;
    for (std::size_t idx = 0; idx < n; ++idx) {
        codec.decode(source, integer);
        *output++ = integer;
    }
    return source;
}

//! Decodes `n` encoded symbols from an input stream to a vector.
/*!
    \param source   an input stream with the encoded elements
    \param n        how many symbols to decode
    \param codec    a codec to use for decoding
    \returns        a vector of decoded values
 */
template<class Codec>
std::vector<typename Codec::value_type>
decode_n(std::istream& source, std::size_t n, const Codec& codec = Codec())
{
    std::vector<typename Codec::value_type> result;
    decode_n(std::back_inserter(result), source, n, codec);
    return result;
}

//! Decodes `n` encoded symbols from a range to a vector.
/*!
    \param source   an input stream with the encoded elements
    \param n        how many symbols to decode
    \param codec    a codec to use for decoding
    \returns        a vector of decoded values
 */
template<class Codec, class SourceRange>
std::vector<typename Codec::value_type>
decode_n(const SourceRange& source, std::size_t n, const Codec& codec = Codec())
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<SourceRange>));
    std::vector<typename Codec::value_type> result;
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> buffer(
        source.data(), source.size());
    decode_n(std::back_inserter(result), buffer, n, codec);
    return result;
}

//! Decodes an entire input stream, applying delta coding.
/*!
    \param output   an output iterator, such as `std::back_inserter`
    \param source   an input stream with the encoded elements
    \param codec    a codec to use for decoding
    \returns        `source` stream
 */
template<class Codec, class OutputIterator>
std::istream& decode_delta(OutputIterator output,
    std::istream& source,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT(
        (boost::OutputIterator<OutputIterator, typename Codec::value_type>));
    typename Codec::value_type integer;
    typename Codec::value_type prev = initial_value;
    while (codec.decode(source, integer)) {
        prev = integer + prev;
        *output++ = prev;
    }
    return source;
}

//! Decodes a range of bytes, applying delta coding.
/*!
    \param source           a range of bytes with the encoded elements
    \param codec            a codec to use for decoding
    \param initial_value    a value to add to the first element
    \returns                a vector of decoded values
 */
template<class Codec, class SourceRange>
std::vector<typename Codec::value_type> decode_delta(const SourceRange& source,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<SourceRange>));
    std::vector<typename Codec::value_type> result;
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> buffer(
        source.data(), source.size());
    decode_delta(std::back_inserter(result), buffer, codec, initial_value);
    return result;
}

//! Decodes `n` symbols from an input stream, applying delta coding.
/*!
    \param output           an output iterator, such as `std::back_inserter`
    \param source           an input stream with the encoded elements
    \param n                how many symbols to decode
    \param codec            a codec to use for decoding
    \param initial_value    a value to add to the first element
    \returns                `source` stream
 */
template<class Codec, class OutputIterator>
std::istream& decode_delta_n(OutputIterator output,
    std::istream& source,
    std::size_t num = 0,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT(
        (boost::OutputIterator<OutputIterator, typename Codec::value_type>));
    typename Codec::value_type integer;
    typename Codec::value_type prev = initial_value;
    for (std::size_t n = 0; n < num; ++n) {
        codec.decode(source, integer);
        prev = integer + prev;
        *output = prev;
    }
    return source;
}

//! Decodes `n` symbols from an input stream, applying delta coding.
/*!
    \param source           an input stream with the encoded elements
    \param n                how many symbols to decode
    \param codec            a codec to use for decoding
    \param initial_value    a value to add to the first element
    \returns                `source` stream
 */
template<class Codec>
std::vector<typename Codec::value_type> decode_delta_n(std::istream& source,
    std::size_t num,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    std::vector<typename Codec::value_type> result;
    decode_delta_n(
        std::back_inserter(result), source, num, codec, initial_value);
    return result;
}

//! Decodes `n` symbols from an input stream, applying delta coding.
/*!
    \param source   an input stream with the encoded elements
    \param n        how many symbols to decode
    \param codec    a codec to use for decoding
    \returns        a vector of decoded values
 */
template<class Codec, class SourceRange>
std::vector<typename Codec::value_type>
decode_delta_n(const SourceRange& source,
    std::size_t n,
    const Codec& codec = Codec(),
    typename Codec::value_type initial_value = typename Codec::value_type(0))
{
    BOOST_CONCEPT_ASSERT((concept::InputRange<SourceRange>));
    std::vector<typename Codec::value_type> result;
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> buffer(
        source.data(), source.size());
    decode_delta_n(std::back_inserter(result), buffer, n, codec, initial_value);
    return result;
}

template<class SizeCodec>
std::ostream& encode_bits(const bitword& bits,
    std::ostream& out,
    const SizeCodec& size_codec = SizeCodec())
{
    std::vector<bitword::block_type> bytes;
    boost::to_block_range(bits, std::back_inserter(bytes));
    std::size_t bits_num = bits.size();
    size_codec.encode(bits_num, out);
    out.write(reinterpret_cast<char*>(bytes.data()), (bits_num + 7) / 8);
    return out;
}

template<class SizeCodec>
bitword decode_bits(std::istream& in, const SizeCodec& size_codec = SizeCodec())
{
    bitword bits;
    std::size_t bits_num;
    size_codec.decode(in, bits_num);
    std::size_t bytes_num = (bits_num + 7) / 8;
    std::vector<char> bytes(bytes_num);
    in.read(bytes.data(), bytes_num);
    bitptr<char> p(bytes.data());
    for (std::size_t bitn = 0; bitn < bits_num; ++bitn) {
        bits.push_back(p[bitn]);
    }
    return bits;
}

};  // namespace irk

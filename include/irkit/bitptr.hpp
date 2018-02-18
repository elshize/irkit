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

//! \file bitptr.hpp
//! \author Michal Siedlaczek
//! \copyright MIT License

#pragma once

#include <boost/dynamic_bitset.hpp>
#include <iostream>

namespace irk {

//! A bit pointer class.
/*!
    This class represents a pointer to a particular bit in the underlying data
    of type `Block`.

    Although the behavior is quite similar to regular pointers, there are some
    fundamental differences.

    **Note**: no decrement or minus operator are defined as of now; therefore,
    we can only move forward. This will be implemented in the future.

    @tparam Block   The underlying type. Any bit operations will be performed
                    on a variable of this type. If `sizeof(Block) > 1`, then it
                    is on the user to make sure that it does not exceed the
                    bounds. E.g., if we create a `bitptr<std::uint16_t>(arr)`
                    where `char* arr = new char[3]`, there is no way to read
                    bits of the last byte of that array without going out of
                    bounds, because the underlying pointer would be to long. On
                    the other hand, it can be more efficient to use longer types
                    than `char`. Therefore, it is recommended to use whatever
                    type the underlying structure is defined with: `char` for
                    `char[]`, `int` for `int[]` and so on. Otherwise, more
                    careful bound checks are required.
 */
template<class Block>
class bitptr {
    static_assert(sizeof(Block) <= 16, "block must be smaller than 16 bytes");

private:
    Block* block_ptr_;
    std::uint8_t shift_;

    static constexpr std::uint8_t block_bit_size = sizeof(Block) * 8;

    struct bit_reference {
        bitptr ptr;
        void operator=(bool bit)
        {
            *ptr.block_ptr_ ^= (-bit ^ *ptr.block_ptr_) & (1 << ptr.shift_);
        }
        operator bool() const { return *ptr.block_ptr_ & (1 << ptr.shift_); }
    };

public:
    //! Creates a pointer to the bit in position `shift` in `block_ptr`.
    /*!
     * It is prefectly legal to set `shift > sizeof(Block)`; then, it will
     * point to the bit in position `shift % block_bit_size` in
     * `block_ptr + (shift / block_bit_size)`.
     *
     * @param block_ptr the created object will point to the least significant
     *                  bit of this block if `shift = 0` (default)
     * @param shift     the bit offset from the least significant bit of
     *                  `block_ptr`.
     */
    bitptr(Block* block_ptr, std::uint8_t shift = 0)
        : block_ptr_(block_ptr + (shift / block_bit_size)),
          shift_(shift % block_bit_size)
    {}

    //! Copy constructor.
    bitptr(const bitptr<Block>& other)
        : block_ptr_(other.block_ptr_), shift_(other.shift_)
    {}

    //! Assignment operator
    void operator=(const bitptr<Block>& other)
    {
        block_ptr_ = other.block_ptr_;
        shift_ = other.shift_;
    }

    //! Sets the bit to the chosen value.
    /*!
     * @param bit   the value to set the bit to
     *
     * If you can determine `bit` at compile time, you might consider using
     * `set()` or `clear()` functions, as they can be slightly more efficient.
     */
    void set(bool bit) { *block_ptr_ ^= (-bit ^ *block_ptr_) & (1 << shift_); }

    //! Sets the bit to 1.
    void set() { *block_ptr_ |= 1 << shift_; }

    //! Sets the bit to 0.
    void clear() { *block_ptr_ &= ~(1UL << shift_); }

    //! Returns a non-const reference to the bit.
    /*!
     * It can be later used to assign a value: `*ptr = true`.
     * There is an overhead of creating a proxy reference; therefore, using
     * `set(bool)`, `set()` and `clear()` may be more efficient. This operator
     * have been implemented in order to conform to a conventional pointer
     * interface, which might be more generic and interchangable with other
     * interfaces, such as `std::vector<bool>`.
     */
    bit_reference operator*() { return {*this}; }

    //! Returns a non-const reference to the `n`-th bit.
    /*!
     * Calling p[n] is equivalent to calling *(p + n) (see
     * `bit_reference operator*()` documentation).
     */
    bit_reference operator[](int n) { return *(*this + n); }

    //! Returns the bit value.
    /*!
     * As opposed to the non-const version of this operator, this one does not
     * create a proxy reference.
     */
    bool operator*() const { return *block_ptr_ & (1 << shift_); }

    //! Returns the value of the `n`-th bit.
    /*!
     * Calling p[n] is equivalent to calling *(p + n) (see `bool operator*()`
     * documentation).
     */
    bool operator[](int n) const { return *(*this + n); }

    //! Increments the pointer to the next bit.
    bitptr& operator++() { return operator+=(1); }

    //! Returns a new pointer to the `n`-th bit.
    bitptr operator+(int n) const
    {
        bitptr result(*this);
        result += n;
        return result;
    }

    //! Increments the pointer by `n` bits.
    bitptr& operator+=(int n)
    {
        shift_ += n;
        block_ptr_ += (shift_ / block_bit_size);
        shift_ %= block_bit_size;
        return *this;
    }

    //! A type that can be used as a bit input stream that automatically moves
    //! its pointer.
    /*!
     * **Warning**: The type is incomplete! So far it only implements `read()`
     *              function.
     */
    struct reader {
        bitptr& pos;

        bool read()
        {
            bool bit = *pos;
            ++pos;
            return bit;
        }
    };

    //! Returns a bit reader starting from the pointer.
    reader reader() { return {*this}; }
};

//! Copies bits of a `boost::dynamic_bitset` to the chunk of underlying data
//! starting at `target`.
/*!
 * @tparam PtrBlock     bit pointer's underlying block type
 * @tparam BitsetBlock  bitset's underlying block type
 *
 * @param target    where the bits will be written
 * @param source    where the bits will be read from
 */
template<class PtrBlock, class BitsetBlock>
void bitcpy(
    bitptr<PtrBlock> target, const boost::dynamic_bitset<BitsetBlock>& source)
{
    std::size_t copied_bits = 0;
    while (copied_bits < source.size()) {
        target.set(source[copied_bits++]);
        ++target;
    }
}

//! Copies bits between bit pointers.
/*!
 * @tparam Block    bit pointer's underlying block type
 *
 * @param target    where the bits will be written
 * @param source    where the bits will be read from
 * @param length    how many bits will be copied
 */
template<class Block>
void bitcpy(bitptr<Block> target, bitptr<Block> source, std::size_t length)
{
    std::size_t copied_bits = 0;
    while (copied_bits < length) {
        target.set(*source);
        ++target;
        ++source;
        ++copied_bits;
    }
}

}  // namespace irk

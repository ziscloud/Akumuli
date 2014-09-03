/**
 * PRIVATE HEADER
 *
 * Compression algorithms
 *
 * Copyright (c) 2013 Eugene Lazin <4lazin@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#pragma once

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <bits/stl_iterator.h>
#include <iterator>
#include <vector>

namespace Akumuli {

    //! Base 128 encoded integer
    template<class TVal>
    class Base128Int {
        TVal value_;
        typedef unsigned char byte_t;
        typedef byte_t* byte_ptr;
    public:

        Base128Int(TVal val) : value_(val) {
        }

        Base128Int() : value_() {
        }

        /** Read base 128 encoded integer from the binary stream
        *  FwdIter - forward iterator
        */
        template<class FwdIter>
        FwdIter get(FwdIter begin, FwdIter end) {
            assert(begin < end);

            auto acc = TVal();
            auto cnt = TVal();
            FwdIter p = begin;

            while (true) {
                auto i = static_cast<byte_t>(*p & 0x7F);
                acc |= i << cnt;
                if ((*p++ & 0x80) == 0) {
                    break;
                }
                cnt += 7;
            }
            value_ = acc;
            return p;
        }

        /** Write base 128 encoded integer to the binary stream.
        * @returns 'begin' on error, iterator to next free region otherwise
        */
        template<class FwdIter>
        FwdIter put(FwdIter begin, FwdIter end) const {
            if (begin >= end) {
                return begin;
            }

            TVal value = value_;
            FwdIter p = begin;

            while (true) {
                *p = value & 0x7F;
                value >>= 7;
                if (value != 0) {
                    *p++ |= 0x80;
                    if (p == end) {
                        return begin;
                    }
                } else {
                    p++;
                    break;
                }
            }
            return p;
        }

        //! Write base 128 encoded integer to the binary stream (unchecked)
        template<class Inserter>
        void put(Inserter& p) const {
            TVal value = value_;
            byte_t result = 0;

            while(true) {
                result = value & 0x7F;
                value >>= 7;
                if (value != 0) {
                    result |= 0x80;
                    *p++ = result;
                } else {
                    *p++ = result;
                    break;
                }
            }
        }

        //! turn into integer
        operator TVal() const {
            return value_;
        }
    };

    //! Base128 encoder
    template<class TVal>
    struct Base128StreamWriter {
        // underlying memory region
        std::vector<unsigned char>& data_;

        Base128StreamWriter(std::vector<unsigned char>& data) : data_(data) {}

        /** Put value into stream.
         */
        void put(TVal value) {
            Base128Int<TVal> val(value);
            auto it = std::back_inserter(data_);
            val.put(it);
        }

        //! Close stream
        void close() {}

        size_t size() const {
            return data_.size();
        }
    };

    //! Base128 decoder
    template<class TVal>
    struct Base128StreamReader {
        std::vector<unsigned char> const& data_;
        std::vector<unsigned char>::const_iterator pos_;
        std::vector<unsigned char>::const_iterator end_;

        Base128StreamReader(std::vector<unsigned char> const& data)
            : data_(data)
            , pos_(data.begin())
            , end_(data.end())
        {
        }

        TVal next() {
            Base128Int<TVal> value;
            pos_ = value.get(pos_, end_);
            return static_cast<TVal>(value);
        }
    };


    template<class Stream, typename TVal>
    struct DeltaStreamWriter {
        Stream& stream_;
        TVal prev_;

        DeltaStreamWriter(Stream& stream)
            : stream_(stream)
            , prev_()
        {
        }

        void put(TVal value) {
            assert(value >= prev_);  // delta encoding must be used for sorted sequences
            auto delta = value - prev_;
            stream_.put(delta);
            prev_ = value;
        }

        size_t size() const {
            return stream_.size();
        }

        void close() {
            stream_.close();
        }
    };


    template<class Stream, typename TVal>
    struct DeltaStreamReader {
        Stream& stream_;
        TVal prev_;

        DeltaStreamReader(Stream& stream)
            : stream_(stream)
            , prev_()
        {
        }

        TVal next() {
            TVal delta = stream_.next();
            TVal value = prev_ + delta;
            prev_ = value;
            return value;
        }
    };


    template<class Stream, typename TVal>
    struct RLEStreamWriter {
        Stream& stream_;
        TVal prev_;
        TVal reps_;

        RLEStreamWriter(Stream& stream)
            : stream_(stream)
            , prev_()
            , reps_()
        {}

        void put(TVal value) {
            if (value != prev_) {
                if (reps_) {
                    // commit changes
                    stream_.put(reps_);
                    stream_.put(prev_);
                }
                prev_ = value;
                reps_ = TVal();
            }
            reps_++;
        }

        size_t size() const {
            return stream_.size();
        }

        void close() {
            stream_.put(reps_);
            stream_.put(prev_);
        }
    };

    template<class Stream, typename TVal>
    struct RLEStreamReader {
        Stream& stream_;
        TVal prev_;
        TVal reps_;

        RLEStreamReader(Stream& stream)
            : stream_(stream)
            , prev_()
            , reps_()
        {}

        TVal next() {
            if (reps_ == 0) {
                reps_ = stream_.next();
                prev_ = stream_.next();
            }
            reps_--;
            return prev_;
        }
    };
}

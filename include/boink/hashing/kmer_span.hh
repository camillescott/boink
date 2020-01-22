/**
 * (c) Camille Scott, 2019
 * File   : kmer_span.hh
 * License: MIT
 * Author : Camille Scott <camille.scott.w@gmail.com>
 * Date   : 10.01.2020
 */

#ifndef BOINK_KMER_SPAN_HH
#define BOINK_KMER_SPAN_HH

#include <iostream>
#include <type_traits>
#include <deque>

#include "boink/ring_span.hpp"

namespace boink::hashing {

void test();

template <bool Enable = true>
class KmerSpanMixinImpl {

private:

    uint16_t _K;

public:

    const bool is_loaded() const {
        return loaded;
    }

protected:

    KmerSpanMixinImpl(uint16_t K)
        : _K(K),
          kmer_buffer(new char[K]),
          kmer_window(kmer_buffer, kmer_buffer + K, kmer_buffer, K),
          loaded(false)
    {
        std::cout << "KmerSpan enabled: " << this << std::endl;
    }

    ~KmerSpanMixinImpl() {
        delete [] kmer_buffer;
    }

    char *                  kmer_buffer;
    nonstd::ring_span<char> kmer_window;
    bool                    loaded;

    const char& front() const {
        return kmer_window.front();
    }

    const char& back() const {
        return kmer_window.back();
    }

    void load(const std::string& sequence) {
        for (uint16_t i = 0; i < _K; ++i)  {
            kmer_window.push_back(sequence[i]);
        }
        loaded = true;
    }

    void load(const char * sequence) {
        for (uint16_t i = 0; i < _K; ++i)  {
            kmer_window.push_back(sequence[i]);
        }
        loaded = true;
    }

    template <typename Iterator>
    void load(Iterator begin, Iterator end) {
        std::copy(begin, end, kmer_window);
    }

    template <typename Iterator>
    void load(Iterator begin) {
        for (uint16_t i = 0; i < _K; ++i)  {
            kmer_window.push_back(*begin);
            begin++;
        }
        loaded = true;
    }

    const std::string to_string() const {
        return std::string(kmer_window.begin(),
                           kmer_window.end());
    }

    void to_deque(std::deque<char>& d) const {

        for (auto symbol : kmer_window) {
            d.push_back(symbol);
        }
    }
};


template<>
class KmerSpanMixinImpl<false>{

protected:

    KmerSpanMixinImpl(uint16_t K) {
        std::cout << "KmerSpan disabled: " << this << std::endl;
    }
};


template <class ShifterType = void>
struct KmerSpanMixin {


    typedef typename std::conditional<std::is_base_of<KmerSpanMixinImpl<true>, ShifterType>::value,
                                      KmerSpanMixinImpl<false>,
                                      KmerSpanMixinImpl<true>>::type type;

};


}


#endif

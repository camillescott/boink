/**
 * (c) Camille Scott, 2019
 * File   : hashshifter.hh
 * License: MIT
 * Author : Camille Scott <camille.scott.w@gmail.com>
 * Date   : 30.07.2019
 */

#ifndef BOINK_HASHSHIFTER_HH
#define BOINK_HASHSHIFTER_HH

#include <algorithm>
#include <cstring>
#include <deque>
#include <iterator>
#include <string>
#include <vector>

#include "boink/kmers/kmerclient.hh"
#include "boink/hashing/canonical.hh"
#include "boink/boink.hh"

#include "boink/sequences/alphabets.hh"
#include "boink/sequences/exceptions.hh"

#include "boink/meta.hh"

#include "boink/hashing/rollinghashshifter.hh"


namespace boink::hashing {


class UninitializedShifterException : public BoinkException {
public:
    explicit UninitializedShifterException(const std::string& msg = "Shifter used without hash_base being called.")
        : BoinkException(msg)
    {
    }
};


template<class ShifterType>
struct has_minimizer {
    static const bool value = false;
};


/**
 * @Synopsis  Policy client class for shifter adapters. Implementations
 *            must define _shift_left, _shift_right, _get, _hash_base,
 *            and _hash. This class is mostly stateless; it just does basic
 *            error handling and dispatches to the implementation.
 *
 * @tparam Derived   The implementation type.
 * @tparam HashType  Hash return type. Specialize for canonical.
 * @tparam Alphabet  The alphabet to hash over.
 */

template<class T>
struct HashShifter;

template <template<typename, typename> typename ShiftPolicy,
                                       typename HashType,
                                       typename Alphabet>
class HashShifter<ShiftPolicy<HashType, Alphabet>>
    : public ShiftPolicy<HashType, Alphabet>,
      public Tagged<HashShifter<ShiftPolicy<HashType, Alphabet>>> {

protected:

    bool initialized;
    typedef Tagged<HashShifter<ShiftPolicy<HashType, Alphabet>>> tagged_type;

public:

    typedef HashShifter<ShiftPolicy<HashType, Alphabet>> type;

    typedef ShiftPolicy<HashType, Alphabet>              shift_policy;
    typedef typename shift_policy::value_type            value_type;
    typedef HashType                                     hash_type;
    typedef typename shift_policy::kmer_type             kmer_type;
    typedef Alphabet                                     alphabet;

    using tagged_type::NAME;
    using tagged_type::OBJECT_ABI_VERSION;

    using shift_policy::K;

    template<typename... ExtraArgs>
    explicit HashShifter(const std::string& start,
                         uint16_t           K,
                         ExtraArgs&&...     args)
        : shift_policy(K, std::forward<ExtraArgs>(args)...),
          initialized(false)
    {
        hash_base(start);
        std::cout << "HashShifter ctor " << this << std::endl;
    }

    template<typename... ExtraArgs>
    explicit HashShifter(uint16_t        K,
                         ExtraArgs&&... args)
        : shift_policy(K, std::forward<ExtraArgs>(args)...),
          initialized(false)
    {
        std::cout << "HashShifter ctor " << this << std::endl;
    }

    explicit HashShifter(const HashShifter& other)
        : shift_policy(static_cast<shift_policy>(other)),
          initialized(false)
    {
    }

    ~HashShifter()
    {
        std::cout << "HashShifter dstor" << this << std::endl;
    }

    hash_type get() {
        return this->get_impl();
    }

    hash_type shift_right(const char& out, const char& in) {
        if (!initialized) {
            throw UninitializedShifterException();
        }
        return this->shift_right_impl(out, in);
    }

    hash_type shift_left(const char& in, const char& out) {
        if (!initialized) {
            throw UninitializedShifterException();
        }
        return this->shift_left_impl(in, out);
    }

    hash_type hash_base(const std::string& sequence) {
        if (sequence.length() < K) {
            throw SequenceLengthException("Sequence must at least length K");
        }

        hash_type h = hash_base(sequence.c_str());
        return h;
    }

    template<class It>
    hash_type hash_base(It begin, It end) {
        if (std::distance(begin, end) != K) {
            throw SequenceLengthException("Iterator distance must be length K");
        }
        hash_type h = this->hash_base_impl(begin, end);
        initialized = true;
        return h;
    }

    hash_type hash_base(const char * sequence) {
        std::cout << "HashShifter::hash_base(const char *) " << sequence << std::endl;
        auto h = this->hash_base_impl(sequence);
        initialized = true;
        return h;
    }

    const hash_type hash(const std::string& sequence) const {
        type hasher(*this);
        return hasher.hash_base(sequence);
    }

    template<typename... ExtraArgs>
    static hash_type hash(const std::string& sequence,
                          const uint16_t K,
                          ExtraArgs&&... args) {
        if (sequence.length() < K) {
            throw SequenceLengthException("Sequence must at least length K");
        }
        //std::cout << "static HashShifter::hash" << std::endl;
        return hash(sequence.c_str(), K, std::forward<ExtraArgs>(args)...);
    }

    template<typename... ExtraArgs>
    static hash_type hash(const char * sequence,
                          const uint16_t K,
                          ExtraArgs&&... args) {
        
        type hasher(K, std::forward<ExtraArgs>(args)...);
        return hasher.hash_base(sequence);
    }

    bool is_initialized() const {
        return initialized;
    }
};

extern template class HashShifter<FwdLemirePolicy>;
extern template class HashShifter<CanLemirePolicy>;

typedef HashShifter<FwdLemirePolicy> FwdRollingShifter;
typedef HashShifter<CanLemirePolicy> CanRollingShifter;

} // boink

#endif

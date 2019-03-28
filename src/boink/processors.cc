/* boink.hh
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "boink/processors.hh"

#include "sourmash/kmer_min_hash.hh"

namespace boink {

SourmashSignatureProcessor
::SourmashSignatureProcessor(KmerMinHash * signature,
                             uint64_t fine_interval,
                             uint64_t medium_interval,
                             uint64_t coarse_interval)
    : Base(fine_interval, medium_interval, coarse_interval),
      signature(signature)
{
}

void SourmashSignatureProcessor::process_sequence(const parsing::Read& read) {
    signature->add_sequence(read.cleaned_seq.c_str(), false);
}

void SourmashSignatureProcessor::report() {

}

}

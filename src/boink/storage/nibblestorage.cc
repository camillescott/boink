/* nibblestorage.cc -- boink-modified oxli storage
 *
 * Copyright (C) 2018 Camille Scott
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 *
 *** END BOINK LICENSE BLOCK
 *
 * This file is part of khmer, https://github.com/dib-lab/khmer/, and is
 * Copyright (C) 2016, The Regents of the University of California.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 * 
 *     * Neither the name of the University of California nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * LICENSE (END)
 * 
 * Contact: khmer-project@idyll.org
 */

#include "boink/storage/nibblestorage.hh"

#include <cstring>
#include <errno.h>
#include <sstream> // IWYU pragma: keep
#include <fstream>
#include <iostream>

#include "boink/boink.hh"
#include "boink/hashing/hashing_types.hh"

using namespace std;
using namespace boink;
using namespace boink::storage;
using namespace boink::hashing;


void NibbleStorage::save(std::string outfilename, uint16_t ksize)
{
    if (!_counts[0]) {
        throw BoinkException();
    }

    unsigned int save_ksize = ksize;
    unsigned char save_n_tables = _n_tables;
    unsigned long long save_tablesize;
    unsigned long long save_occupied_bins = _occupied_bins;

    ofstream outfile(outfilename.c_str(), ios::binary);

    outfile.write(SAVED_SIGNATURE, 4);
    unsigned char version = SAVED_FORMAT_VERSION;
    outfile.write((const char *) &version, 1);

    unsigned char ht_type = SAVED_SMALLCOUNT;
    outfile.write((const char *) &ht_type, 1);

    outfile.write((const char *) &save_ksize, sizeof(save_ksize));
    outfile.write((const char *) &save_n_tables, sizeof(save_n_tables));
    outfile.write((const char *) &save_occupied_bins,
                  sizeof(save_occupied_bins));

    for (unsigned int i = 0; i < save_n_tables; i++) {
        save_tablesize = _tablesizes[i];

        outfile.write((const char *) &save_tablesize, sizeof(save_tablesize));
        outfile.write((const char *) _counts[i], save_tablesize / 2 + 1);
    }
}

void NibbleStorage::load(std::string infilename, uint16_t& ksize)
{
    ifstream infile;
    // configure ifstream to raise exceptions for everything.
    infile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                      std::ifstream::eofbit);

    try {
        infile.open(infilename.c_str(), ios::binary);
    } catch (std::ifstream::failure &e) {
        std::string err;
        if (!infile.is_open()) {
            err = "Cannot open k-mer count file: " + infilename;
        } else {
            err = "Unknown error in opening file: " + infilename;
        }
        throw BoinkFileException(err + " " + strerror(errno));
    } catch (const std::exception &e) {
        std::string err = "Unknown error opening file: " + infilename + " "
                          + strerror(errno);
        throw BoinkFileException(err);
    }

    if (_counts) {
        for (unsigned int i = 0; i < _n_tables; i++) {
            delete[] _counts[i];
            _counts[i] = NULL;
        }
        delete[] _counts;
        _counts = NULL;
    }
    _tablesizes.clear();

    try {
        unsigned int save_ksize = 0;
        unsigned char save_n_tables = 0;
        unsigned long long save_tablesize = 0;
        unsigned long long save_occupied_bins = 0;
        char signature [4];
        unsigned char version = 0, ht_type = 0;

        infile.read(signature, 4);
        infile.read((char *) &version, 1);
        infile.read((char *) &ht_type, 1);
        if (!(std::string(signature, 4) == SAVED_SIGNATURE)) {
            std::ostringstream err;
            err << "Does not start with signature for a oxli file: 0x";
            for(size_t i=0; i < 4; ++i) {
                err << std::hex << (int) signature[i];
            }
            err << " Should be: " << SAVED_SIGNATURE;
            throw BoinkFileException(err.str());
        } else if (!(version == SAVED_FORMAT_VERSION)) {
            std::ostringstream err;
            err << "Incorrect file format version " << (int) version
                << " while reading k-mer count file from " << infilename
                << "; should be " << (int) SAVED_FORMAT_VERSION;
            throw BoinkFileException(err.str());
        } else if (!(ht_type == SAVED_SMALLCOUNT)) {
            std::ostringstream err;
            err << "Incorrect file format type " << (int) ht_type
                << " while reading k-mer count file from " << infilename;
            throw BoinkFileException(err.str());
        }

        infile.read((char *) &save_ksize, sizeof(save_ksize));
        infile.read((char *) &save_n_tables, sizeof(save_n_tables));
        infile.read((char *) &save_occupied_bins, sizeof(save_occupied_bins));

        ksize = (uint16_t) save_ksize;
        _n_tables = (unsigned int) save_n_tables;
        _occupied_bins = save_occupied_bins;

        _counts = new byte_t*[_n_tables];
        for (unsigned int i = 0; i < _n_tables; i++) {
            _counts[i] = NULL;
        }

        for (unsigned int i = 0; i < _n_tables; i++) {
            uint64_t tablesize;
            uint64_t tablebytes;

            infile.read((char *) &save_tablesize, sizeof(save_tablesize));

            tablebytes = save_tablesize / 2 + 1;
            tablesize = save_tablesize;
            _tablesizes.push_back(tablesize);

            _counts[i] = new byte_t[tablebytes];

            unsigned long long loaded = 0;
            while (loaded != tablebytes) {
                infile.read((char *) _counts[i], tablebytes - loaded);
                loaded += infile.gcount();
            }
        }
        infile.close();
    } catch (std::ifstream::failure &e) {
        std::string err;
        if (infile.eof()) {
            err = "Unexpected end of k-mer count file: " + infilename;
        } else {
            err = "Error reading from k-mer count file: " + infilename + " "
                  + strerror(errno);
        }
        throw BoinkFileException(err);
    } catch (const std::exception &e) {
        std::string err = "Error reading from k-mer count file: " + infilename + " "
                          + strerror(errno);
        throw BoinkFileException(err);
    }
}

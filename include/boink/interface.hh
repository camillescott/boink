#include "boink/storage/nibblestorage.hh"
#include "boink/storage/qfstorage.hh"

#include "boink/processors.hh"

#include "boink/ukhs_signature.hh"

#include <set>

#define STLTYPES_EXPLICIT_INSTANTIATION_DECL(STLTYPE, TTYPE)                      \
template class std::STLTYPE< TTYPE >;                                             \
template class __gnu_cxx::__normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >;      \
template class __gnu_cxx::__normal_iterator<const TTYPE*, std::STLTYPE< TTYPE > >;\
namespace __gnu_cxx {                                                             \
template bool operator==(const std::STLTYPE< TTYPE >::iterator&,                  \
                         const std::STLTYPE< TTYPE >::iterator&);                 \
template bool operator!=(const std::STLTYPE< TTYPE >::iterator&,                  \
                         const std::STLTYPE< TTYPE >::iterator&);                 \
}

//#define STLTYPES_EXPLICIT_INSTANTIATION_DECL(STLTYPE, TTYPE)                        \
template class std::STLTYPE< TTYPE >;                                               \
template class __gnu_cxx::__normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >;\
template class __gnu_cxx::__normal_iterator<const TTYPE*, std::STLTYPE< TTYPE > >; \
namespace __gnu_cxx {                                                           \
template bool operator==(const __normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >&,  \
                         const __normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >&); \
template bool operator!=(const __normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >&,  \
                         const __normal_iterator<TTYPE*, std::STLTYPE< TTYPE > >&); \
}
//template bool operator!=(const std::STLTYPE< TTYPE >::iterator&,                  \
                         const std::STLTYPE< TTYPE >::iterator&);                 \




STLTYPES_EXPLICIT_INSTANTIATION_DECL(vector, boink::hashing::hash_t)
STLTYPES_EXPLICIT_INSTANTIATION_DECL(vector, boink::hashing::shift_t)
STLTYPES_EXPLICIT_INSTANTIATION_DECL(vector, boink::hashing::kmer_t)


template class std::pair<boink::hashing::shift_t, boink::hashing::shift_t>;

template class boink::hashing::KmerIterator<boink::hashing::RollingHashShifter>;

template <typename ReaderType=boink::parsing::FastxReader>
struct ParserFactory {
	static std::shared_ptr<boink::parsing::ReadParser<boink::parsing::FastxReader>> build(const std::string& filename) {
		return boink::parsing::get_parser<boink::parsing::FastxReader>(filename);
	}
};

template struct ParserFactory<boink::parsing::FastxReader>;
//
// dBG storage specializations
//

// BitStorage
template class boink::dBG<boink::storage::BitStorage,
                          boink::hashing::RollingHashShifter>;
template class std::enable_shared_from_this<boink::dBG<boink::storage::BitStorage,
                                            boink::hashing::RollingHashShifter>>;
template class boink::AssemblerMixin<boink::dBG<boink::storage::BitStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::CompactorMixin<boink::dBG<boink::storage::BitStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::DecisionNodeProcessor<boink::dBG<boink::storage::BitStorage,
                                                       boink::hashing::RollingHashShifter>>;
template class boink::StreamingCompactorProcessor<boink::dBG<boink::storage::BitStorage,
                                                             boink::hashing::RollingHashShifter>>;
template class boink::FileConsumer<boink::dBG<boink::storage::BitStorage,
                                              boink::hashing::RollingHashShifter>>;

// ByteStorage
template class boink::dBG<boink::storage::ByteStorage,
                          boink::hashing::RollingHashShifter>;
template class std::enable_shared_from_this<boink::dBG<boink::storage::ByteStorage,
                                            boink::hashing::RollingHashShifter>>;
template class boink::AssemblerMixin<boink::dBG<boink::storage::ByteStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::CompactorMixin<boink::dBG<boink::storage::ByteStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::DecisionNodeProcessor<boink::dBG<boink::storage::ByteStorage,
                                                       boink::hashing::RollingHashShifter>>;
template class boink::StreamingCompactorProcessor<boink::dBG<boink::storage::ByteStorage,
                                                             boink::hashing::RollingHashShifter>>;
template class boink::FileConsumer<boink::dBG<boink::storage::ByteStorage,
                                              boink::hashing::RollingHashShifter>>;

// NibbleStorage
template class boink::dBG<boink::storage::NibbleStorage,
                          boink::hashing::RollingHashShifter>;
template class std::enable_shared_from_this<boink::dBG<boink::storage::NibbleStorage,
                                            boink::hashing::RollingHashShifter>>;
template class boink::AssemblerMixin<boink::dBG<boink::storage::NibbleStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::CompactorMixin<boink::dBG<boink::storage::NibbleStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::DecisionNodeProcessor<boink::dBG<boink::storage::NibbleStorage,
                                                       boink::hashing::RollingHashShifter>>;
template class boink::StreamingCompactorProcessor<boink::dBG<boink::storage::NibbleStorage,
                                                             boink::hashing::RollingHashShifter>>;
template class boink::FileConsumer<boink::dBG<boink::storage::NibbleStorage,
                                              boink::hashing::RollingHashShifter>>;

// QFStorage
template class boink::dBG<boink::storage::QFStorage,
                          boink::hashing::RollingHashShifter>;
template class std::enable_shared_from_this<boink::dBG<boink::storage::QFStorage,
                                            boink::hashing::RollingHashShifter>>;
template class boink::AssemblerMixin<boink::dBG<boink::storage::QFStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::CompactorMixin<boink::dBG<boink::storage::QFStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::DecisionNodeProcessor<boink::dBG<boink::storage::QFStorage,
                                                       boink::hashing::RollingHashShifter>>;
template class boink::StreamingCompactorProcessor<boink::dBG<boink::storage::QFStorage,
                                                             boink::hashing::RollingHashShifter>>;
template class boink::FileConsumer<boink::dBG<boink::storage::QFStorage,
                                              boink::hashing::RollingHashShifter>>;

// SparseppSetStorage
template class boink::dBG<boink::storage::SparseppSetStorage,
                          boink::hashing::RollingHashShifter>;
template class std::enable_shared_from_this<boink::dBG<boink::storage::SparseppSetStorage,
                                            boink::hashing::RollingHashShifter>>;
template class boink::AssemblerMixin<boink::dBG<boink::storage::SparseppSetStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::CompactorMixin<boink::dBG<boink::storage::SparseppSetStorage,
                                                 boink::hashing::RollingHashShifter>>;
template class boink::DecisionNodeProcessor<boink::dBG<boink::storage::SparseppSetStorage,
                                                       boink::hashing::RollingHashShifter>>;
template class boink::StreamingCompactorProcessor<boink::dBG<boink::storage::SparseppSetStorage,
                                                             boink::hashing::RollingHashShifter>>;
template class boink::FileConsumer<boink::dBG<boink::storage::SparseppSetStorage,
                                              boink::hashing::RollingHashShifter>>;


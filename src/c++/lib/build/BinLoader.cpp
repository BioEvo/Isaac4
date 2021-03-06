/**
 ** Isaac Genome Alignment Software
 ** Copyright (c) 2010-2017 Illumina, Inc.
 ** All rights reserved.
 **
 ** This software is provided under the terms and conditions of the
 ** GNU GENERAL PUBLIC LICENSE Version 3
 **
 ** You should have received a copy of the GNU GENERAL PUBLIC LICENSE Version 3
 ** along with this program. If not, see
 ** <https://github.com/illumina/licenses/>.
 **
 ** \file BinLoader.cpp
 **
 ** Reorders aligned data and stores results in bam file.
 ** 
 ** \author Roman Petrovski
 **/

#include <boost/foreach.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "bam/Bam.hh"

#include "build/BinLoader.hh"
#include "common/Memory.hh"

namespace isaac
{
namespace build
{

void verifyFragmentIntegrity(const io::FragmentAccessor &fragment)
{
/*
    if (fragment.getTotalLength() != fragment.totalLength_)
    {
        ISAAC_THREAD_CERR << pFragment << std::endl;
        BOOST_THROW_EXCEPTION(common::IoException(
            errno, (boost::format("Corrupt fragment (fragment total length is broken at %ld) read from %s") %
                (reinterpret_cast<const char *>(pFragment) - &data_.front()) %
                bin.getPathString()).str()));
    }
    if (io::FragmentAccessor::magicValue_ != fragment.magic_)
    {
        ISAAC_THREAD_CERR << pFragment << std::endl;
        BOOST_THROW_EXCEPTION(common::IoException(
            errno, (boost::format("Corrupt fragment (magic is broken) read from %s") % bin.getPathString()).str()));
    }
*/
}

void BinLoader::loadData(BinData &binData)
{
    ISAAC_THREAD_CERR << "Loading unsorted data" << std::endl;
    const clock_t startLoad = clock();

    if(binData.isUnalignedBin())
    {
        loadUnalignedData(binData);
    }
    else
    {
        loadAlignedData(binData);
    }

    ISAAC_THREAD_CERR << "Loading unsorted data done in " << (clock() - startLoad) / 1000 << "ms" << std::endl;
}

void BinLoader::loadUnalignedData(BinData &binData)
{
    if(binData.bin_.getDataSize())
    {
        ISAAC_THREAD_CERR << "Reading unaligned records from " << binData.bin_ << std::endl;
        std::istream isData(&binData.inputFileBuf_);
        if (!isData) {
            BOOST_THROW_EXCEPTION(common::IoException(errno, "Failed to open " + binData.bin_.getPathString()));
        }

        if (!isData.seekg(binData.bin_.getDataOffset(), std::ios_base::beg))
        {
            BOOST_THROW_EXCEPTION(common::IoException(
                errno, (boost::format("Failed to seek to position %d in %s") % binData.bin_.getDataOffset() % binData.bin_.getPathString()).str()));
        }
        // TODO: this takes time to fill it up with 0... 2 seconds per bin easily
        binData.data_.resize(binData.bin_);
        if (!isData.read(&binData.data_.front(), binData.bin_.getDataSize())) {
            BOOST_THROW_EXCEPTION(common::IoException(
                errno, (boost::format("Failed to read %d bytes from %s") % binData.bin_.getDataSize() % binData.bin_.getPathString()).str()));
        }

/*
        unsigned count = 0;
        uint64_t offset = 0;
        while(data_.size() != offset)
        {
            const io::FragmentAccessor &fragment = data_.getFragment(offset);
            offset += fragment.getTotalLength();
            ++count;
            ISAAC_THREAD_CERR << "offset " << offset << fragment << std::endl;
            ISAAC_ASSERT_MSG(offset <= data_.size(), "Fragment crosses bin boundary."
                " offset:" << offset << " data_.size():" << data_.size() << " " << bin_);
        }
*/
        ISAAC_THREAD_CERR << "Reading unaligned records done from " << binData.bin_ << std::endl;
    }
    else
    {
//        ISAAC_THREAD_CERR << "No unaligned records to read done from " << bin_ << std::endl;
    }
}


std::size_t BinLoader::loadFragment(BinData &binData, std::istream &isData)
{
    std::size_t offset = 0;
    io::FragmentHeader header;
    if (!isData.read(reinterpret_cast<char*>(&header), sizeof(header)))
    {
        if (isData.eof())
        {
            return INVALID_OFFSET;
        }
        BOOST_THROW_EXCEPTION(common::IoException(
            errno, (boost::format("Failed to read FragmentHeader bytes from %s") % binData.bin_).str()));
    }

    ISAAC_ASSERT_MSG(header.flags_.initialized_, "Uninitialized header read from " << binData.bin_ <<
                     " isData.tellg() " << isData.tellg() <<
                     " offset " << offset <<
                     " " << header);

    const unsigned fragmentLength = header.getTotalLength();
    // fragments that don't belong to the bin are supposed to go into chunk 0
    offset = binData.data_.size();

    // TODO: this takes time to fill it up with 0... 2 seconds per bin easily
//    binData.data_.resize(std::max(binData.data_.size(), offset + fragmentLength));
    ISAAC_ASSERT_MSG(binData.data_.capacity() >= offset + fragmentLength,
                     "Insufficient buffer " << binData.bin_ <<
                     " isData.tellg() " << isData.tellg() <<
                     " offset " << offset <<
                     " fragmentLength " << fragmentLength <<
                     " " << header);
    binData.data_.resize(offset + fragmentLength);

    io::FragmentAccessor &fragment = binData.data_.getFragment(offset);
    io::FragmentHeader &headerRef = fragment;
    headerRef = header;
    if (!isData.read(reinterpret_cast<char*>(&fragment) + sizeof(header), fragmentLength - sizeof(header))) {
        BOOST_THROW_EXCEPTION(common::IoException(
            errno, (boost::format("Failed to read %d bytes from %s") % fragmentLength % binData.bin_.getPathString()).str()));
    }

    //ISAAC_THREAD_CERR << "LOADED: " << fragment << std::endl;

    return offset;
}

void BinLoader::storeFragmentIndex(
    const io::FragmentAccessor& fragment,
    uint64_t offset,
    uint64_t mateOffset, BinData& binData)
{
    if (fragment.flags_.reverse_ || fragment.flags_.unmapped_)
    {
        RStrandOrShadowFragmentIndex rsIdx(
            fragment.fStrandPosition_, // shadows are stored at the position of their singletons,
            io::FragmentIndexAnchor(fragment),
            FragmentIndexMate(fragment.flags_.mateUnmapped_,
                              fragment.flags_.mateReverse_,
                              fragment.mateStorageBin_,
                              fragment.mateAnchor_),
            fragment.duplicateClusterRank_);
        rsIdx.dataOffset_ = offset;
        rsIdx.mateDataOffset_ = mateOffset;
        binData.rIdx_.push_back(rsIdx);
    }
    else
    {
        FStrandFragmentIndex fIdx(
            fragment.fStrandPosition_,
            FragmentIndexMate(fragment.flags_.mateUnmapped_,
                              fragment.flags_.mateReverse_,
                              fragment.mateStorageBin_,
                              fragment.mateAnchor_),
            fragment.duplicateClusterRank_);
        fIdx.dataOffset_ = offset;
        fIdx.mateDataOffset_ = mateOffset;
        binData.fIdx_.push_back(fIdx);
    }
}

bool fragmentCrossesBin(const io::FragmentAccessor &fragment, const alignment::BinMetadata &bin)
{
    if (!fragment.isAligned())
    {
        return false;
    }

    for (alignment::CigarPosition<const unsigned *> it(
        fragment.cigarBegin(), fragment.cigarEnd(), fragment.getFStrandReferencePosition(), fragment.isReverse(), fragment.readLength_);
        !it.end(); ++it)
    {
        if (alignment::Cigar::ALIGN == it.component().second)
        {
            if (bin.coversPosition(it.referencePos_) || bin.coversPosition(it.referencePos_ + it.component().first - 1))
            {
                return true;
            }
        }
    }

    return false;
}

void BinLoader::loadAlignedData(BinData &binData)
{
    if(binData.bin_.getDataSize())
    {
        ISAAC_THREAD_CERR << "Reading alignment records from " << binData.bin_ << std::endl;
        uint64_t dataSize = 0;
        std::istream isData(&binData.inputFileBuf_);
        if (!isData) {
            BOOST_THROW_EXCEPTION(common::IoException(errno, "Failed to open " + binData.bin_.getPathString()));
        }
        ISAAC_ASSERT_MSG(0 == binData.bin_.getDataOffset(), "Unexpected offset:" << binData.bin_);
        if (!isData.seekg(binData.bin_.getDataOffset()))
        {
            BOOST_THROW_EXCEPTION(common::IoException(
                errno, (boost::format("Failed to seek to position %d in %s") % binData.bin_.getDataOffset() % binData.bin_.getPathString()).str()));
        }

        binData.rIdx_.clear();
        binData.fIdx_.clear();
        binData.seIdx_.clear();

        io::FragmentHeader lastFragmentHeader;
        io::FragmentHeader lastMateHeader;
        for(std::size_t offset = loadFragment(binData, isData);
            INVALID_OFFSET != offset;
            offset = loadFragment(binData, isData))
        {
            const io::FragmentAccessor &fragment = binData.data_.getFragment(offset);

            verifyFragmentIntegrity(fragment);

            if (!fragment.flags_.paired_)
            {
                if (fragmentCrossesBin(fragment, binData.bin_))
                {
                    dataSize += fragment.getTotalLength();
                    // same fragment can be in the same file multiple times. this is a bit wasteful, but not storing them there
                    // creates a challenge of predicting when to stop when reading for data of a bunch of merged bins
                    if (lastFragmentHeader != fragment)
                    {
                        SeFragmentIndex seIdx(fragment.fStrandPosition_);
                        seIdx.dataOffset_ = offset;
                        binData.seIdx_.push_back(seIdx);
                        lastFragmentHeader = fragment;
                        continue;
                    }
                }
            }
            else
            {
                const std::size_t mateOffset = loadFragment(binData, isData);

                ISAAC_ASSERT_MSG(INVALID_OFFSET != mateOffset, "Paired data is missing a mate in " << binData.bin_ << " fragment " << fragment);
                const io::FragmentAccessor &mateFragment = binData.data_.getFragment(mateOffset);
                verifyFragmentIntegrity(mateFragment);
                const bool fragmentBelongs = fragmentCrossesBin(fragment, binData.bin_);
                const bool mateBelongs = fragmentCrossesBin(mateFragment, binData.bin_);
                // mates are present even if they belong to a different bin

                if (fragmentBelongs || mateBelongs)
                {
                    ISAAC_ASSERT_MSG(mateFragment.tile_ == fragment.tile_, "mateFragment.tile_ != fragment.tile_ " << fragment << " " << mateFragment);
                    ISAAC_ASSERT_MSG(mateFragment.clusterId_ == fragment.clusterId_, "mateFragment.clusterId_ != fragment.clusterId_" << fragment << " " << mateFragment);
                    ISAAC_ASSERT_MSG(mateFragment.flags_.unmapped_ == fragment.flags_.mateUnmapped_, "mateFragment.flags_.unmapped_ != fragment.flags_.mateUnmapped_" << fragment << " " << mateFragment);
                    ISAAC_ASSERT_MSG(mateFragment.flags_.reverse_ == fragment.flags_.mateReverse_,
                                     "mateFragment.flags_.reverse_ != fragment.flags_.mateReverse_" << fragment << " " << mateFragment);

                    dataSize += fragment.getTotalLength();
                    dataSize += mateFragment.getTotalLength();

                    if (lastFragmentHeader != fragment)
                    {
                        ISAAC_ASSERT_MSG(lastMateHeader != mateFragment, "New fragment but same mate: " << mateFragment << " fragment: " << fragment);
                        storeFragmentIndex(mateFragment, mateOffset, offset, binData);
                        storeFragmentIndex(fragment, offset, mateOffset, binData);
                        lastFragmentHeader = fragment;
                        lastMateHeader = mateFragment;
                        continue;
                    }
                    else
                    {
                        ISAAC_ASSERT_MSG(lastMateHeader == mateFragment, "same fragment but new mate: " << mateFragment << " fragment: " << fragment);
                    }
                }
            }
            // otherwise the fragment is not relevant, revert buffer back to before loading it
            binData.data_.resize(offset);
        }
        ISAAC_THREAD_CERR << "Reading alignment records done from " << binData.bin_ << std::endl;

        ISAAC_ASSERT_MSG(binData.bin_.getDataSize() >= dataSize, "Too much data seen:" << dataSize << " for " << binData.bin_);
        binData.finalize();
    }
}

} // namespace build
} // namespace isaac

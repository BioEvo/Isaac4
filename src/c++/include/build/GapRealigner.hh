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
 ** \file GapRealigner.hh
 **
 ** Attempts to reduce read mismatches by introducing gaps found on other reads.
 ** 
 ** \author Roman Petrovski
 **/

#ifndef iSAAC_BUILD_GAP_REALIGNER_HH
#define iSAAC_BUILD_GAP_REALIGNER_HH

#include <boost/math/special_functions/binomial.hpp>

#include "alignment/Cigar.hh"
#include "alignment/TemplateLengthStatistics.hh"
#include "build/gapRealigner/RealignerGaps.hh"
#include "build/PackedFragmentBuffer.hh"
#include "flowcell/BarcodeMetadata.hh"
#include "reference/Contig.hh"
#include "reference/ReferencePosition.hh"

namespace isaac
{
namespace build
{

/**
 * \brief Attempts to insert gaps found on other fragments while preserving the ones that
 *        are already there.
 */
class GapRealigner
{
public:
    typedef uint64_t GapChoiceBitmask;
private:
    // number of bits that can represent the on/off state for each gap.
    // Currently unsigned is used to hold the choice
    static const unsigned MAX_GAPS_AT_A_TIME = 64;

    const bool realignGapsVigorously_;
    const bool realignDodgyFragments_;
    const unsigned gapsPerFragmentMax_;
    const unsigned combinationsLimit_;
    // Recommended value to be lower than gapOpenCost_ in a way that
    // no less than two mismatches would warrant adding a gap
    const unsigned mismatchCost_;// = 3;
    const unsigned gapOpenCost_;// = 4;
    // Recommended 0 as it does not matter how long the introduced gap is for realignment
    const unsigned gapExtendCost_;// = 0;
    static const int mismatchPercentReductionMin_ = 20;

    const flowcell::BarcodeMetadataList &barcodeMetadataList_;

    gapRealigner::Gaps currentAttemptGaps_;

    gapRealigner::RealignerGaps fragmentGaps_;

public:
    typedef gapRealigner::Gap GapType;
    GapRealigner(
        const bool realignGapsVigorously,
        const bool realignDodgyFragments,
        const unsigned gapsPerFragmentMax,
        const unsigned mismatchCost,
        const unsigned gapOpenCost,
        const unsigned gapExtendCost,
        const flowcell::BarcodeMetadataList &barcodeMetadataList):
            realignGapsVigorously_(realignGapsVigorously),
            realignDodgyFragments_(realignDodgyFragments),
            gapsPerFragmentMax_(gapsPerFragmentMax),
            combinationsLimit_(boost::math::binomial_coefficient<double>(MAX_GAPS_AT_A_TIME, gapsPerFragmentMax_)),
            mismatchCost_(mismatchCost),
            gapOpenCost_(gapOpenCost),
            gapExtendCost_(gapExtendCost),
            barcodeMetadataList_(barcodeMetadataList)
    {
        reserve();
    }

    void reserve()
    {
        currentAttemptGaps_.reserve(MAX_GAPS_AT_A_TIME * 10);
        // number of existing gaps to be expected in one fragment. No need to be particularly precise.
        fragmentGaps_.reserve(currentAttemptGaps_.capacity());
    }

    bool realign(
        const gapRealigner::RealignerGaps &realignerGaps,
        const reference::ReferencePosition binStartPos,
        const reference::ReferencePosition binEndPos,
        const io::FragmentAccessor &fragment,
        PackedFragmentBuffer::Index &index,
        reference::ReferencePosition &newRStrandPosition,
        unsigned short &newEditDistance,
        PackedFragmentBuffer &dataBuffer,
        alignment::Cigar &realignedCigars,
        const reference::ContigLists &contigLists);

    // This one finds mate in the dataBuffer and updates it. Make sure no other thread is workin on the same pair at the same time
    static void updatePairDetails(
        const std::vector<alignment::TemplateLengthStatistics> &barcodeTemplateLengthStatistics,
        const PackedFragmentBuffer::Index &index,
        const reference::ReferencePosition newRStrandPosition,
        const unsigned short newEditDistance,
        io::FragmentAccessor &fragment,
        PackedFragmentBuffer &dataBuffer);

private:

    struct RealignmentBounds
    {
        /*
         * \brief Position of the first non soft-clipped base of the read
         */
        reference::ReferencePosition beginPos_;
        /*
         * \breif   Position of the first insertion base or the first base before the first deletion.
         *          If there are no indels, equals to endPos.
         */
        reference::ReferencePosition firstGapStartPos_;
        /*
         * \brief   Position of the first base following the last insertion or the first base
         *          that is not part of the last deletion. If there are no indels, equals to beginPos_
         */
        reference::ReferencePosition lastGapEndPos_;
        /*
         * \brief   Position of the base that follows the last non soft-clipped base of the read
         */
        reference::ReferencePosition endPos_;
    };
    friend std::ostream & operator << (std::ostream &os, const GapRealigner::RealignmentBounds &fragmentGaps);

    const gapRealigner::GapsRange findMoreGaps(
        gapRealigner::GapsRange range,
        const gapRealigner::Gaps &gaps,
        const reference::ReferencePosition binStartPos,
        const reference::ReferencePosition binEndPos);

    const gapRealigner::GapsRange findGaps(
        const unsigned sampleId,
        const reference::ReferencePosition binStartPos,
        const reference::ReferencePosition binEndPos,
        const reference::ReferencePosition rangeBegin,
        const reference::ReferencePosition rangeEnd);

    bool applyChoice(
        const GapChoiceBitmask &choice,
        const gapRealigner::GapsRange &gaps,
        const reference::ReferencePosition binEndPos,
        const reference::ReferencePosition contigEndPos,
        PackedFragmentBuffer::Index &index,
        const io::FragmentAccessor &fragment,
        alignment::Cigar &realignedCigars);


    struct GapChoice
    {
        GapChoice() : choice_(0), editDistance_(0), mismatches_(0), mismatchesPercent_(0), cost_(0), totalPriority_(0), mappedLength_(0){}
        GapChoiceBitmask choice_;
        unsigned editDistance_;
        unsigned mismatches_;
        unsigned mismatchesPercent_;
        unsigned cost_;
        unsigned totalPriority_;
        unsigned mappedLength_;
        reference::ReferencePosition startPos_;

        friend std::ostream & operator <<(std::ostream &os, const GapChoice &gapChoice)
        {
            return os << "GapChoice(" << gapChoice.choice_ << "," <<
                gapChoice.editDistance_ << "ed," <<
                gapChoice.mismatches_ << "mm," <<
                gapChoice.cost_ << "c," <<
                gapChoice.totalPriority_ << "tp," <<
                gapChoice.mappedLength_ << "ml," <<
                gapChoice.startPos_ << ")";
        }

        void addPriority(const gapRealigner::Gap &gap)
        {
            if (gap.HIGHEST_PRIORITY - totalPriority_ >= gap.priority_)
            {
                totalPriority_ += gap.priority_;
            }
            else
            {
                totalPriority_ = gapRealigner::Gap::HIGHEST_PRIORITY;
            }
        }
    };


    GapChoice verifyGapsChoice(
        const GapChoiceBitmask &choice,
        const gapRealigner::GapsRange &gaps,
        const reference::ReferencePosition newBeginPos,
        const io::FragmentAccessor &fragment,
        const reference::ContigList &reference);

    bool isBetterChoice(
        const GapChoice &choice,
        const unsigned maxMismatchesPercent,
        const GapChoice &bestChoice) const;

    const RealignmentBounds extractRealignmentBounds(const PackedFragmentBuffer::Index &index) const;

    bool findStartPos(
        const GapChoiceBitmask &choice,
        const gapRealigner::GapsRange &gaps,
        const reference::ReferencePosition binStartPos,
        const reference::ReferencePosition binEndPos,
        const unsigned pivotGapIndex,
        const reference::ReferencePosition pivotPos,
        int64_t alignmentPos,
        reference::ReferencePosition &ret);

    bool compactCigar(
        const reference::ContigList &reference,
        const reference::ReferencePosition binEndPos,
        const io::FragmentAccessor &fragment,
        PackedFragmentBuffer::Index &index,
        reference::ReferencePosition &newRStrandPosition,
        unsigned short &newEditDistance,
        alignment::Cigar &realignedCigars);

    GapChoice getAlignmentCost(
        const io::FragmentAccessor &fragment,
        const PackedFragmentBuffer::Index &index) const;

    void compactRealignedCigarBuffer(
        std::size_t bufferSizeBeforeRealignment,
        PackedFragmentBuffer::Index &index,
        alignment::Cigar &realignedCigars);

    bool findBetterGapsChoice(
        const gapRealigner::GapsRange& gaps,
        const reference::ReferencePosition& binStartPos,
        const reference::ReferencePosition& binEndPos,
        const reference::ContigList& reference,
        const io::FragmentAccessor& fragment,
        const PackedFragmentBuffer::Index& index,
        unsigned &leftToEvaluate,
        GapChoice &bestChoice);

    int64_t undoExistingGaps(const PackedFragmentBuffer::Index& index,
                          const reference::ReferencePosition& pivotPos);

    bool verifyGapsChoice(
        const GapChoiceBitmask &choice,
        const gapRealigner::GapsRange& gaps,
        const reference::ReferencePosition& binStartPos,
        const reference::ReferencePosition& binEndPos,
        const io::FragmentAccessor& fragment,
        const reference::ContigList& reference,
        const int originalMismatchesPercent,
        const int64_t undoneAlignmentPos,
        GapChoice& bestChoice);
};

} // namespace build
} // namespace isaac

#endif // #ifndef iSAAC_BUILD_GAP_REALIGNER_HH

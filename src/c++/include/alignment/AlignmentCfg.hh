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
 ** \file AlignmentConfig.hh
 **
 ** \brief Structure for various details of how alignment gets penalized for mismatches, gaps, etc
 ** 
 ** \author Roman Petrovski
 **/

#ifndef iSAAC_ALIGNMENT_ALIGNMENT_CFG_HH
#define iSAAC_ALIGNMENT_ALIGNMENT_CFG_HH

namespace isaac
{
namespace alignment
{

struct AlignmentCfg
{
    AlignmentCfg(
        const int gapMatchScore,
        const int gapMismatchScore,
        const int gapOpenScore,
        const int gapExtendScore,
        const int minGapExtendScore,
        const unsigned splitGapLength)
    : matchScore_(gapMatchScore)
    , mismatchScore_(gapMismatchScore)
    , gapOpenScore_(gapOpenScore)
    , gapExtendScore_(gapExtendScore)
    , minGapExtendScore_(minGapExtendScore)
    , splitGapLength_(splitGapLength)
    {
    }

    const int matchScore_;
    const int mismatchScore_;
    const int gapOpenScore_;
    const int gapExtendScore_;
    const int minGapExtendScore_;

    const unsigned splitGapLength_;
};

} // namespace alignment
} // namespace isaac

#endif // #ifndef iSAAC_ALIGNMENT_ALIGNMENT_CFG_HH

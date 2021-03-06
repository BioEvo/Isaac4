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
 ** \file AlignWorkflow.hh
 **
 ** \brief Top level component to controll the analysis process.
 **
 ** \author Come Raczy
 **/

#ifndef iSAAC_WORKFLOW_ALIGN_WORKFLOW_HH
#define iSAAC_WORKFLOW_ALIGN_WORKFLOW_HH

#include <string>
#include <vector>

#include <boost/serialization/access.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>

#include "demultiplexing/BarcodePathMap.hh"
#include "alignment/BinMetadata.hh"
#include "alignment/MatchDistribution.hh"
#include "alignment/TemplateBuilder.hh"
#include "alignment/TemplateLengthStatistics.hh"
#include "alignment/matchFinder/TileClusterInfo.hh"
#include "build/BinSorter.hh"
#include "common/Threads.hpp"
#include "demultiplexing/BarcodeLoader.hh"
#include "demultiplexing/BarcodeResolver.hh"
#include "flowcell/Layout.hh"
#include "flowcell/BarcodeMetadata.hh"
#include "flowcell/ReadMetadata.hh"
#include "flowcell/TileMetadata.hh"
#include "oligo/Kmer.hh"
#include "reference/ReferenceMetadata.hh"

#include "workflow/alignWorkflow/FindHashMatchesTransition.hh"
#include "workflow/alignWorkflow/FoundMatchesMetadata.hh"

#include "reports/AlignmentReportGenerator.hh"

namespace isaac
{
namespace workflow
{

namespace bfs = boost::filesystem;

class AlignWorkflow: boost::noncopyable
{
public:

    // note, the tags are sorted by name and the numeric values must represent sequential bit positions
    enum OptionalFeatures
    {
        Nothing = 0,
        BamAS = 0x01,
        BamBC = 0x02,
        BamNM = 0x04,
        BamOC = 0x08,
        BamRG = 0x10,
        BamSM = 0x20,
        BamZX = 0x40,
        BamZY = 0x80,
        Everything = BamAS|BamBC|BamNM|BamOC|BamRG|BamSM|BamZX|BamZY
    };

    AlignWorkflow(
        const std::vector<std::string> &argv,
        const std::string &description,
        const std::size_t hashTableBucketCount,
        const std::vector<flowcell::Layout> &flowcellLayoutList,
        const unsigned seedLength,
        const flowcell::BarcodeMetadataList &barcodeMetadataList,
        const bool cleanupIntermediary,
        const unsigned bclTilesPerChunk,
        const bool ignoreMissingBcls,
        const bool ignoreMissingFilters,
        const unsigned expectedCoverage,
        const uint64_t matchesPerBin,
        const reference::ReferenceMetadataList &referenceMetadataList,
        const bfs::path &tempDirectory,
        const bfs::path &outputDirectory,
        const unsigned maxThreadCount,
        const std::size_t candidateMatchesMax,
        const unsigned matchFinderTooManyRepeats,
        const unsigned matchFinderWayTooManyRepeats,
        const unsigned matchFinderShadowSplitRepeats,
        const unsigned seedBaseQualityMin,
        const unsigned repeatThreshold,
        const int mateDriftRange,
        const unsigned neighborhoodSizeThreshold,
        const uint64_t availableMemory,
        const unsigned clustersAtATimeMax,
        const bool ignoreNeighbors,
        const bool ignoreRepeats,
        const int mapqThreshold,
        const bool perTileTls,
        const bool pfOnly,
        const unsigned baseQualityCutoff,
        const bool keepUnaligned,
        const bool preSortBins,
        const bool preAllocateBins,
        const bool putUnalignedInTheBack,
        const bool realignGapsVigorously,
        const bool realignDodgyFragments,
        const unsigned realignedGapsPerFragment,
        const bool clipSemialigned,
        const bool clipOverlapping,
        const bool scatterRepeats,
        const bool rescueShadows,
        const bool trimPEAdapters,
        const unsigned gappedMismatchesMax,
        const unsigned smitWatermanGapsMax,
        const bool smartSmithWaterman,
        const unsigned smitWatermanGapSizeMax,
        const bool splitAlignments,
        const int gapMatchScore,
        const int gapMismatchScore,
        const int gapOpenScore,
        const int gapExtendScore,
        const int minGapExtendScore,
        const unsigned splitGapLength,
        const alignment::TemplateBuilder::DodgyAlignmentScore dodgyAlignmentScore,
        const unsigned anomalousPairHandicap,
        const unsigned inputLoadersMax,
        const unsigned tempSaversMax,
        const unsigned tempLoadersMax,
        const unsigned outputSaversMax,
        const build::GapRealignerMode realignGaps,
        const unsigned realignMapqMin,
        const boost::filesystem::path &knownIndelsPath,
        const int bamGzipLevel,
        const std::string &bamPuFormat,
        const bool bamProduceMd5,
        const std::vector<std::string> &bamHeaderTags,
        const double expectedBgzfCompressionRatio,
        const bool singleLibrarySamples,
        const bool keepDuplicates,
        const bool markDuplicates,
        const bool anchorMate,
        const std::string &binRegexString,
        const std::string &decoyRegexString,
        const common::ScopedMallocBlock::Mode memoryControl,
        const std::vector<std::size_t> &clusterIdList,
        const alignment::TemplateLengthStatistics &userTemplateLengthStatistics,
        const reports::AlignmentReportGenerator::ImageFileFormat statsImageFormat,
        const bool qScoreBin,
        const boost::array<char, 256> &fullBclQScoreTable,
        const OptionalFeatures optionalFeatures,
        const bool pessimisticMapQ,
        const unsigned detectTemplateBlockSize);

    /**
     * \brief Runs end-to-end alignment from the beginning
     */
    void run();


    enum State
    {
        Invalid = -2,
        Last = -1,
        Start = 0,          // constructor completed
        AlignDone,    // MatchFinder done, foundMatchesMetadata_ is valid
        AlignmentReportsDone,
        BamDone,            // Bam file generated
        Finish = BamDone
    };

    AlignWorkflow::State getNextState() const;

    /**
     * \brief Performs single step of aligner state transition
     *
     * \return The new state
     */
    AlignWorkflow::State step();

    /**
     * \brief Erases all intermediary files that are not required for the stages that have been completed
     */
    void cleanupIntermediary();

    /**
     * \brief Changes the aligner state to the specified, provided the prerequisite data is available
     *
     * \return the new state
     */
    AlignWorkflow::State rewind(AlignWorkflow::State to);

private:
    static const unsigned READS_MAX = 2;

    template<class Archive> friend void serialize(Archive & ar, AlignWorkflow &, const unsigned int file_version);

    typedef alignment::BinMetadataList SelectedMatchesMetadata;

    const std::vector<std::string> &argv_;
    const std::string &description_;
    const std::size_t hashTableBucketCount_;
    const std::vector<flowcell::Layout> &flowcellLayoutList_;
    const unsigned seedLength_;
    const bfs::path tempDirectory_;
    const bfs::path statsDirectory_;
    const bfs::path reportsDirectory_;
    const bfs::path projectsDirectory_;
    const bfs::path matchSelectorStatsXmlPath_;
    const unsigned coresMax_;
    const std::size_t candidateMatchesMax_;
    const unsigned matchFinderTooManyRepeats_;
    const unsigned matchFinderWayTooManyRepeats_;
    const unsigned matchFinderShadowSplitRepeats_;
    const unsigned seedBaseQualityMin_;
    const unsigned repeatThreshold_;
    const int mateDriftRange_;
    const unsigned neighborhoodSizeThreshold_;
    const bool ignoreNeighbors_;
    const bool ignoreRepeats_;
    const std::vector<std::size_t> &clusterIdList_;
    const flowcell::BarcodeMetadataList &barcodeMetadataList_;
    const bool cleanupIntermediary_;
    const unsigned bclTilesPerChunk_;
    const bool ignoreMissingBcls_;
    const bool ignoreMissingFilters_;
    const uint64_t availableMemory_;

    const unsigned expectedCoverage_;
    const unsigned int estimatedFragmentSize_;
    const double expectedBgzfCompressionRatio_;
    const uint64_t targetFragmentsPerBin_;
    const uint64_t targetBinLength_;
    const uint64_t targetBinSize_;
    const unsigned clustersAtATimeMax_;
    const int mapqThreshold_;
    const bool perTileTls_;
    const bool pfOnly_;
    const unsigned baseQualityCutoff_;
    const bool keepUnaligned_;
    const bool preSortBins_;
    const bool preAllocateBins_;
    const bool putUnalignedInTheBack_;
    const bool realignGapsVigorously_;
    const bool realignDodgyFragments_;
    const unsigned realignedGapsPerFragment_;
    const bool clipSemialigned_;
    const bool clipOverlapping_;
    const bool scatterRepeats_;
    const bool rescueShadows_;
    const bool trimPEAdapters_;
    const unsigned gappedMismatchesMax_;
    const unsigned smitWatermanGapsMax_;
    const bool smartSmithWaterman_;
    const unsigned smitWatermanGapSizeMax_;
    const bool splitAlignments_;
    const alignment::AlignmentCfg alignmentCfg_;
    const alignment::TemplateBuilder::DodgyAlignmentScore dodgyAlignmentScore_;
    const unsigned anomalousPairHandicap_;
    const unsigned inputLoadersMax_;
    const unsigned tempSaversMax_;
    const unsigned tempLoadersMax_;
    const unsigned outputSaversMax_;
    const build::GapRealignerMode realignGaps_;
    const unsigned realignMapqMin_;
    const boost::filesystem::path &knownIndelsPath_;
    const int bamGzipLevel_;
    const std::string &bamPuFormat_;
    const bool bamProduceMd5_;
    const std::vector<std::string> &bamHeaderTags_;
    const bool singleLibrarySamples_;
    const bool keepDuplicates_;
    const bool markDuplicates_;
    const bool anchorMate_;
    const bool qScoreBin_;
    const boost::array<char, 256> &fullBclQScoreTable_;
    const OptionalFeatures optionalFeatures_;
    const bool pessimisticMapQ_;
    const std::string &binRegexString_;
    const common::ScopedMallocBlock::Mode memoryControl_;
    const alignment::TemplateLengthStatistics userTemplateLengthStatistics_;
    const bfs::path demultiplexingStatsXmlPath_;
    const reports::AlignmentReportGenerator::ImageFileFormat statsImageFormat_;

    const reference::ReferenceMetadataList &referenceMetadataList_;
    const reference::SortedReferenceMetadataList sortedReferenceMetadataList_;
    const reference::NumaContigLists contigLists_;

    State state_;
    alignWorkflow::FoundMatchesMetadata foundMatchesMetadata_;
    SelectedMatchesMetadata selectedMatchesMetadata_;
    std::vector<alignment::TemplateLengthStatistics> barcodeTemplateLengthStatistics_;
    demultiplexing::BarcodePathMap barcodeBamMapping_;
    const unsigned detectTemplateBlockSize_;


    static reference::SortedReferenceMetadataList loadSortedReferenceXml(
        const reference::ReferenceMetadataList &referenceMetadataList,
        const unsigned coresMax);

    void findMatches(
        alignWorkflow::FoundMatchesMetadata &foundMatches,
        alignment::BinMetadataList &binMetadataList,
        std::vector<alignment::TemplateLengthStatistics> &barcodeTemplateLengthStatistics) const;
    void cleanupBins() const;
    void generateAlignmentReports() const;
    const demultiplexing::BarcodePathMap generateBam(
        const SelectedMatchesMetadata &binPaths,
        const std::vector<alignment::TemplateLengthStatistics> &barcodeTemplateLengthStatistics) const;
    void selectMatches(
        alignment::matchSelector::FragmentStorage& fragmentStorage,
        alignWorkflow::FoundMatchesMetadata& foundMatches,
        std::vector<alignment::TemplateLengthStatistics>& barcodeTemplateLengthStatistics) const;
};
} // namespace workflow
} // namespace isaac

#endif // #ifndef iSAAC_WORKFLOW_ALIGN_WORKFLOW_HH

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
 ** \file KmerGenerator.hpp
 **
 ** A component providing a simple way to iterate over a sequence and generate
 ** the corresponding kmer.
 **
 ** \author Come Raczy
 **/


#ifndef iSAAC_OLIGO_KMER_GENERATOR_HH
#define iSAAC_OLIGO_KMER_GENERATOR_HH

#include "oligo/Kmer.hh"
#include "oligo/Nucleotides.hh"

namespace isaac
{
namespace oligo
{

/**
 ** \brief A component to generate successive kmers from a sequence.
 **
 ** \param T the type of the kmer
 **/
template <unsigned kmerLength, class T, typename InputIteratorT, unsigned step = 1, typename TranslatorT = Translator<> >
class KmerGenerator
{
public:
    /**
     ** \build a KmerGenerator for the given sequence.
     **
     ** \param begin the beginning of the sequence
     **
     ** \param end the end of the sequence (STL-like end)
     **
     ** param kmerLength the length of the kmers to produce.
     **/
    KmerGenerator(
        const InputIteratorT begin,
        const InputIteratorT end,
        const TranslatorT &translator = DEFAULT_TRANSLATOR())
        : current_(begin)
        , end_(end)
        , mask_(~oligo::shlBases(T(~T(0)), kmerLength))
        , kmer_(0)
        , translator_(translator)
    {
        ISAAC_ASSERT_MSG(1 < kmerLength, "1-mers not supported");
        ISAAC_ASSERT_MSG(end_ != current_, "Empty sequence not supported");
        const T one(1);
        ISAAC_ASSERT_MSG((oligo::BITS_PER_BASE * kmerLength) <= (BITS_PER_BYTE * sizeof(T)), "Type " << typeid(T).name() <<
            "is insufficient to accommodate kmer length " << kmerLength);
        // in Intel left shift by number of bits >= of type width does not do anything. Make sure this is not happening again
        ISAAC_VERIFY_MSG(!((oligo::shlBases(one, kmerLength) & mask_)), "Left shift failed");
//        assert((oligo::shlBases(one, kmerLength_) - T(1)) == ((oligo::shlBases(one, kmerLength_) - T(1)) & mask_));
        initialize<1>(0);
        initialize<kmerLength - 1>(kmerLength - 2);
    }

    /**
     ** \brief Retrieve the next k-mer hat does not contain any N.
     **
     ** \param kmer the next kmer if any
     **
     ** \param start position of the produced kmer, if any
     **
     ** \return true if a kmer was produced. False otherwise (the end of the
     ** sequence has been reached)
     **/
    bool next(T &kmer, InputIteratorT &position)
    {
        if (initialize<kmerLength>(1))
        {
            kmer = kmer_ & mask_;
            position = current_ - (kmerLength - 1) * step;
            return true;
        }
        return false;
    }

    /**
     * \brief skip n kmers
     *
     * \param n number of would be generated kmers to skip including those containing INVALID_OLIGO
     */
    void skip(const unsigned n)
    {
        initialize<kmerLength - 1>(n);
    }

    static const TranslatorT &DEFAULT_TRANSLATOR()
    {
        static const TranslatorT t;
        return t;
    }
private:
    static const unsigned BITS_PER_BASE = 2;
    static const unsigned BITS_PER_BYTE = 8;
    InputIteratorT current_;
    const InputIteratorT end_;
    const T mask_;
    T kmer_;
    const TranslatorT &translator_;


    /**
     *  \brief initialize the internal kmer_, skipping the Ns.
     *  \postcondition  current_ points at the last base of current k-mer
     */
    template <unsigned resetLen>
    bool initialize(unsigned uninitializedBases)
    {
        do
        {
            if (uninitializedBases)
            {
                if (std::distance(current_, end_) <= step)
                {
                    return false;
                }
                current_ += step;
            }
            const unsigned baseValue = translator_[*current_];
            if(INVALID_OLIGO > baseValue)
            {
                kmer_ <<= BITS_PER_BASE;
                kmer_ |= T(baseValue);
                if (uninitializedBases)
                {
                    --uninitializedBases;
                }
            }
            else
            {
                // N found, start over
                uninitializedBases = resetLen;
            }
        }
        while(uninitializedBases);
        return !uninitializedBases;
    }
};

template <unsigned kmerLength, class T, typename InputIteratorT, unsigned step, typename TranslatorT, unsigned offset>
class InterleavedKmerGeneratorImpl :  InterleavedKmerGeneratorImpl<kmerLength, T, InputIteratorT, step, TranslatorT, offset - 1>
{
    typedef InterleavedKmerGeneratorImpl<kmerLength, T, InputIteratorT, step, TranslatorT, offset - 1> BaseT;
    // each generator works on a single sequence from begin to end, skipping step-1 number of bases
    typedef KmerGenerator<kmerLength, T, InputIteratorT, step, TranslatorT> GeneratorType;
    GeneratorType generator_;
    InputIteratorT basePosition_;
    T baseKmer_;
    bool basePositionAvailable_;
    InputIteratorT ourPosition_;
    T ourKmer_;
    bool ourPositionAvailable_;
    bool lastWasOurs_;
public:
    InterleavedKmerGeneratorImpl(
        const InputIteratorT begin,
        const InputIteratorT end,
        const TranslatorT &translator = GeneratorType::DEFAULT_TRANSLATOR()) :
            BaseT(begin, end, translator),
            generator_(begin + offset, end, translator),
            baseKmer_(0),
            basePositionAvailable_(false),
            ourKmer_(0),
            ourPositionAvailable_(generator_.next(ourKmer_, ourPosition_)),
            lastWasOurs_(false)
    {
    }

    bool next(T &kmer, InputIteratorT &position)
    {
        if (lastWasOurs_)
        {
            ourPositionAvailable_ = generator_.next(ourKmer_, ourPosition_);
        }
        else
        {
            basePositionAvailable_ = BaseT::next(baseKmer_, basePosition_);
        }

        if (basePositionAvailable_)
        {
            if (ourPositionAvailable_ && ourPosition_ < basePosition_)
            {
                lastWasOurs_ = true;
                kmer = ourKmer_;
                position = ourPosition_;
//                ISAAC_THREAD_CERR << "ours:" << ourPosition_ << std::endl;
            }
            else
            {
                lastWasOurs_ = false;
                kmer = baseKmer_;
                position = basePosition_;
//                ISAAC_THREAD_CERR << "thrs:" << basePosition_ << std::endl;
            }
            return true;
        }
        else if (ourPositionAvailable_)
        {
            lastWasOurs_ = true;
            kmer = ourKmer_;
            position = ourPosition_;
//            ISAAC_THREAD_CERR << "oors:" << ourPosition_ << std::endl;
            return true;
        }

        return false;
    }

    /**
     * \brief skip n kmers on the last generator used.
     *
     * \param n number of would be generated kmers to skip including those containing INVALID_OLIGO
     */
    void skip(const unsigned n)
    {
        if (lastWasOurs_)
        {
            generator_.skip(n);
        }
        else
        {
            BaseT::skip(n);
        }
    }

};

template <unsigned kmerLength, class T, typename InputIteratorT, unsigned step, typename TranslatorT>
class InterleavedKmerGeneratorImpl<kmerLength, T, InputIteratorT, step, TranslatorT, 0>
{
    typedef KmerGenerator<kmerLength, T, InputIteratorT, step, TranslatorT> GeneratorType;
    GeneratorType generator_;
public:
    InterleavedKmerGeneratorImpl(
        const InputIteratorT begin,
        const InputIteratorT end,
        const TranslatorT &translator = GeneratorType::DEFAULT_TRANSLATOR()) :
            generator_(begin, end, translator)
    {
    }

    bool next(T &kmer, InputIteratorT &position)
    {
        return generator_.next(kmer, position);
    }


    /**
     * \brief skip n kmers on the last generator used.
     *
     * \param n number of would be generated kmers to skip including those containing INVALID_OLIGO
     */
    void skip(const unsigned n)
    {
        generator_.skip(n);
    }
};

template <unsigned kmerLength, class T, typename InputIteratorT, unsigned step, typename TranslatorT = Translator<> >
class InterleavedKmerGenerator : InterleavedKmerGeneratorImpl<kmerLength, T, InputIteratorT, step, TranslatorT, step - 1>
{
    typedef InterleavedKmerGeneratorImpl<kmerLength, T, InputIteratorT, step, TranslatorT, step - 1> BaseT;
public:
    InterleavedKmerGenerator(
        const InputIteratorT begin, const InputIteratorT end, const TranslatorT &translator) : BaseT(begin, end, translator)
    {
    }

    InterleavedKmerGenerator(const InputIteratorT begin, const InputIteratorT end) : BaseT(begin, end)
    {
    }

    using BaseT::next;
    using BaseT::skip;
};


} // namespace oligo
} // namespace isaac

#endif // #ifndef iSAAC_OLIGO_KMER_GENERATOR_HH

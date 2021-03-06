// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2015, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Enrico Siragusa <enrico.siragusa@fu-berlin.de>
// ==========================================================================
// Approximate string matching via backtracking on VSTrees
// ==========================================================================

#ifndef APP_BENCH_FIND_BACKTRACKING_BFS_H_
#define APP_BENCH_FIND_BACKTRACKING_BFS_H_

//#define SEQAN_DEBUG

namespace seqan {

// ============================================================================
// Metafunctions
// ============================================================================

template <typename TIterator, typename TErrors>
struct State_
{
    typedef Pair<TIterator, TErrors>    Type;
};

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

template <typename TSpec = void>
struct MatchesDelegate_
{
    unsigned long matches;

    MatchesDelegate_() : matches(0) {}
};

// ============================================================================

// NOTE(esiragusa): TIterator should be a template argument instead of TIndex
template <typename TIndex, typename TErrors, typename TDelegate>
struct NFA_
{
    typedef typename Size<TIndex>::Type                         TSize;
    typedef typename Iterator<TIndex, TopDown<> >::Type         TIterator;
    typedef typename State_<TIterator, TErrors>::Type           TState;
    typedef String<TState>                                      TStates;

    Holder<TIndex>      data_host;
    TSize               patternsLength;
    TSize               currentDepth;
    TErrors             errors;
    String<TStates>     states;
    TDelegate           & delegate;

    NFA_(TIndex & index, TSize patternsLength, TErrors errors, TDelegate & delegate) :
        data_host(index),
        patternsLength(patternsLength),
        currentDepth(0),
        errors(errors),
        delegate(delegate)
    {
        _initStates(*this);
        goInitialState(*this);
    }
};

// ============================================================================
// Functions
// ============================================================================

template <typename TSpec, typename TTextIndexIterator, typename TPatternIndexIterator, typename TErrors>
inline void onMatch(MatchesDelegate_<TSpec> & delegate,
                    TTextIndexIterator const & textIt,
                    TPatternIndexIterator const & nfaIt,
                    TErrors /* errors */)
{
    delegate.matches += countOccurrences(textIt) * countOccurrences(nfaIt);
}

//template <typename TSpec, typename TTextIndexIterator, typename TPatternIndexIterator, typename TErrors>
//inline void onMatch(MatchesDelegate_<TSpec> & delegate,
//                    TTextIndexIterator const & textIt,
//                    TPatternIndexIterator const & nfaIt,
//                    TErrors /* errors */)
//{
//    unsigned long textOccurrences = countOccurrences(textIt);
//    unsigned long patternOccurrences = countOccurrences(nfaIt);
//
//    for (unsigned long i = 0; i < textOccurrences; ++i)
//        for (unsigned long j = 0; j < patternOccurrences; ++j)
//            std::cout << '[' << getOccurrences(textIt)[i] <<
//                         ',' << getOccurrences(textIt)[i] <<
//                         ")\t" << representative(textIt) << std::endl;
//
//    delegate.matches += countOccurrences(textIt) * countOccurrences(nfaIt);
//}

// ============================================================================

template <typename TNFA>
inline void _initStates(TNFA & nfa)
{
    typedef typename TNFA::TState       TState;
    typedef typename TNFA::TIterator    TIterator;
    resize(nfa.states, nfa.patternsLength + 1, Exact());

    TIterator nfaIt(value(nfa.data_host));
    appendValue(nfa.states[nfa.currentDepth], TState(nfaIt, 0));

    // TODO(esiragusa): reserve space for states given errors and patternsLength.
}

template <typename TNFA>
inline void goInitialState(TNFA & nfa)
{
    while (nfa.currentDepth)
        goBack(nfa);
}

template <typename TNFA>
inline bool goBack(TNFA & nfa)
{
    if (nfa.currentDepth == 0) return false;

    // Clear current states.
    SEQAN_ASSERT_GT(length(nfa.states), nfa.currentDepth);
    clear(nfa.states[nfa.currentDepth]);

    // Go back to previous states.
    nfa.currentDepth--;

#ifdef SEQAN_DEBUG
    std::cout << "back to depth:  " << nfa.currentDepth << std::endl;
#endif

    return true;
}

template <typename TIndex, typename TErrors, typename TDelegate>
inline bool accept(NFA_<TIndex, TErrors, TDelegate> const & nfa)
{
    return nfa.currentDepth == nfa.patternsLength;
}

// TODO(esiragusa): rename atEnd() and make nfa a const &
template <typename TIndex, typename TErrors, typename TDelegate>
inline bool atEnd(NFA_<TIndex, TErrors, TDelegate> & nfa)
{
    // The NFA is at end if it accepts or if there are no active states.
    SEQAN_ASSERT_GT(length(nfa.states), nfa.currentDepth);
    return accept(nfa) || empty(nfa.states[nfa.currentDepth]);
}

template <typename TIndex, typename TErrors, typename TDelegate, typename TTextIterator>
inline bool goNext(NFA_<TIndex, TErrors, TDelegate> & nfa, TTextIterator & textIt)
{
    typedef NFA_<TIndex, TErrors, TDelegate>            TNFA;
    typedef typename TNFA::TIterator                    TPatternIterator;
    typedef typename TNFA::TState                       TState;
    typedef typename TNFA::TStates                      TStates;
    typedef typename Iterator<TStates, Standard>::Type  TStatesIterator;
    typedef typename EdgeLabel<TTextIterator>::Type     TTextLabel;
    typedef typename EdgeLabel<TPatternIterator>::Type  TPatternLabel;

    // Get active states.
    SEQAN_ASSERT_GT(length(nfa.states), nfa.currentDepth);
    TStates & activeStates = nfa.states[nfa.currentDepth];

    TStatesIterator statesIt = begin(activeStates, Standard());
    TStatesIterator statesEnd = end(activeStates, Standard());
    SEQAN_ASSERT_NEQ(statesIt, statesEnd);

    // The next states become active.
    nfa.currentDepth++;

    // Read text label.
    TTextLabel textLabel = parentEdgeLabel(textIt);

#ifdef SEQAN_DEBUG
    std::cout << "text:           " << textLabel << std::endl;
#endif

    // NOTE(esiragusa): Using repLength() is fine only for tries.
    SEQAN_ASSERT_EQ(repLength(textIt), nfa.currentDepth);

    // Clear next states.
    SEQAN_ASSERT_GT(length(nfa.states), nfa.currentDepth);
    TStates & nextStates = nfa.states[nfa.currentDepth];
    clear(nextStates);

#ifdef SEQAN_DEBUG
    std::cout << "representative: " << representative(textIt) << std::endl;
    std::cout << "repLength:      " << repLength(textIt) << std::endl;
    std::cout << "current depth:  " << nfa.currentDepth << std::endl;
#endif

//    if (statesEnd - statesIt == 1 && getValueI2(value(statesIt)) == nfa.errors)
//        std::cout << "One exact search" << std::endl;

    // Apply the transition to all active states.
    for (; statesIt != statesEnd; ++statesIt)
    {
        TPatternIterator patternIt = getValueI1(value(statesIt));
        TErrors oldErrors = getValueI2(value(statesIt));

        if (oldErrors == nfa.errors)
        {
            // TODO(esiragusa): goDown(nfaIt, Dna5) must be overloaded not to follow Ns.
//            if (ordValue(textLabel) < 4 && goDown(patternIt, textLabel))
            if (goDown(patternIt, textLabel))
            {
#ifdef SEQAN_DEBUG
                std::cout << "pattern:        " << textLabel << std::endl;
                std::cout << "distance:       " << 0 << std::endl;
                std::cout << "errors:         " << static_cast<unsigned>(oldErrors) << std::endl;
#endif

                if (accept(nfa) || isLeaf(patternIt))
                    onMatch(nfa.delegate, textIt, patternIt, oldErrors);
                else
                {
//                    if (nfa.currentDepth >= 10)
//                        // TODO(esiragusa): Fix cast for BucketRefinement QGramIndex
//                        _search(static_cast<typename TTextIterator::TBase>(textIt), patternIt,
//                                nfa.patternsLength, nfa.errors,
//                                nfa.currentDepth, oldErrors,
//                                nfa.delegate);
//                    else
                        appendValue(nextStates, TState(patternIt, oldErrors));
                }
            }
        }
        else
        {
            // Traverse all children of current state.
            if (goDown(patternIt))
            {
                do
                {
                    // Align edge labels.
                    TPatternLabel patternLabel = parentEdgeLabel(patternIt);
                    TErrors distance = (textLabel == patternLabel) ? 0 : 1;
                    TErrors errors = oldErrors + distance;

#ifdef SEQAN_DEBUG
                    std::cout << "pattern:        " << patternLabel << std::endl;
                    std::cout << "distance:       " << static_cast<unsigned>(distance) << std::endl;
                    std::cout << "errors:         " << static_cast<unsigned>(errors) << std::endl;
#endif

                    // Add children to next states.
                    if (errors <= nfa.errors)
                    {
                        // The NFA moved into an acceptance state.
                        // NOTE(esiragusa): patternIt should be a leaf.
                        if (accept(nfa) || isLeaf(patternIt))
                            onMatch(nfa.delegate, textIt, patternIt, errors);

                        // The NFA moved into a non-acceptance state.
                        else
                        {
                            // TODO(esiragusa): Debug this.
//                            if (nfa.currentDepth >= 10)
//                            {
//                                if (errors > 0)
//                                    _dfs(static_cast<typename TTextIterator::TBase>(textIt), patternIt,
//                                         nfa.patternsLength, nfa.errors,
//                                         nfa.currentDepth, errors,
//                                         nfa.delegate);
//                                else
//                                    _search(static_cast<typename TTextIterator::TBase>(textIt), patternIt,
//                                            nfa.patternsLength, nfa.errors,
//                                            nfa.currentDepth, errors,
//                                            nfa.delegate);
//                            }
//                            else
                                appendValue(nextStates, TState(patternIt, errors));
                        }
                    }
                }
                while (goRight(patternIt));
            }
        }
    }

    // The NFA moved into some new non-acceptance states.
    return !atEnd(nfa);
}

// ============================================================================

template <typename TTextIterator, typename TNFA>
inline void _cut(TTextIterator & textIt, TNFA & nfa)
{
    if (goRight(textIt))
    {
        goBack(nfa);
    }
    else
    {
        while (goUp(textIt))
        {
            goBack(nfa);

            if (goRight(textIt))
            {
                goBack(nfa);
                break;
            }
        }
    }
}

template <typename TTextIterator, typename TNFA>
inline void _bfs(TTextIterator & textIt, TNFA & nfa)
{
    if (goDown(textIt))
    {
        do
        {
            if (!goNext(nfa, textIt) || !goDown(textIt))
                _cut(textIt, nfa);
        }
        while (!isRoot(textIt));
    }
}

// NOTE(esiragusa): TTextIndex and TPatternIndex must be tries
template <typename TTextIndex, typename TPatternIndex, typename TSize, typename TErrors, typename TDelegate>
void find(TTextIndex & text,
          TPatternIndex & pattern,
          TSize patternsLength,
          TErrors errors,
          TDelegate & delegate,
          BfsIterator const & /* tag */)
{
    typedef TopDown<ParentLinks<> >                                 TTextIteratorSpec;
    typedef typename Iterator<TTextIndex, TTextIteratorSpec>::Type  TTextIterator;
    typedef NFA_<TPatternIndex, TErrors, TDelegate>                 TNFA;

    TTextIterator textIt(text);
    TNFA nfa(pattern, patternsLength, errors, delegate);

    _bfs(textIt, nfa);
}

}

#endif  // #ifndef APP_BENCH_FIND_BACKTRACKING_BFS_H_

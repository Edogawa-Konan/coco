//
// Created by mayuan on 2020/3/19.
//




#ifndef COCO_BOWMODEL_H
#define COCO_BOWMODEL_H

#include "bow.h"
#include <unordered_set>
#include <unordered_map>
#include "pattern.h"
#include "patternmodel.h"
#include <map>



struct BagOfWordsHash {
    size_t operator()(const BagOfWords& bagOfWords) const {
        auto v = bagOfWords.bow;
        std::hash<int> hasher;
        size_t seed = 0;
        for (size_t i : v) {
            seed ^= hasher(i) + 0x9e3779b9 + (seed<<6u) + (seed>>2u);
        }
        return seed;
    }
};



typedef std::unordered_map<BagOfWords, std::unordered_map<Pattern, IndexedData, std::hash<Pattern>>, BagOfWordsHash> BagOfWordsStore;


class BowModel {
public:

//    std::unordered_map<Pattern, IndexedData, std::hash<Pattern>> pattern2index;


//    std::map<BagOfWords, std::unordered_set<Pattern, std::hash<Pattern>>> bow2pattern;

    BagOfWordsStore data;


    std::map<int, std::vector<uint32_t> > gapmasks; ///< pre-computed masks representing possible gap configurations for various pattern lengths

    explicit BowModel(IndexedCorpus *indexedCorpus) : reverseindex(indexedCorpus) {}

    bool has(const PatternPointer &pattern) const;

    void add(PatternPointer &patternPointer, IndexReference &indexReference);

    /**
         * Prune all patterns under the specified occurrence threshold (or -1
         * for all). Pruning can be limited to patterns of a particular size or
         * category.
         * @param threshold The occurrence threshold (set to -1 to prune everything)
         * @param _n The size constraint, limit to patterns of this size only (set to 0 for no constraint, default)
         * @param category Category constraint
         * @return the number of distinct patterns pruned
         */
    unsigned int prune(int threshold, int _n = 0, int category = 0);

    /**
     * Prune skipgrams based on an occurrence threshold, and a skiptype
     * threshold. The latter enforces that at least the specified number of
     * distinct patterns must fit in the gap for the skipgram to be retained.
    * @param _n Set to any value above zero to only include patterns of the specified length.
     */
    unsigned int pruneskipgrams(int minskiptypes, int _n = 0);

    unsigned int occurrencecount(const Pattern &pattern) const;

    unsigned int occurrencecount(const BagOfWords &bagOfWords) const;

    void compute_bow_from_ngram(const std::string &filename, PatternModelOptions options,
                                ClassDecoder *classDecoder = nullptr);

    void compute_bow_from_skipgram(PatternModelOptions &options);

    int computeskipgrams(const PatternPointer &pattern, PatternModelOptions &options, IndexedData &multiplerefs);

    IndexedCorpus *reverseindex; ///< Pointer to the reverse index and corpus data for this model (or NULL)

    size_t size() const;

    size_t remove(const BagOfWords &bagOfWords);

    BagOfWordsStore::iterator remove(BagOfWordsStore::iterator it);

    void to_csv(std::ostream &out, ClassDecoder &decoder);

};

#endif //COCO_BOWMODEL_H

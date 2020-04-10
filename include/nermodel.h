
#ifndef COCO_NERMODEL_H
#define COCO_NERMODEL_H

#include <unordered_map>
#include "patternmodel.h"
#include "pattern.h"
#include <string>
#include "classdecoder.h"

struct PatternHash {
    size_t operator()(const Pattern &pattern) const {
        std::hash<std::string> hasher;
        return hasher(pattern.tostring(*Pattern::classDecoder));
    }
};

struct PatternEqual
{
    bool operator()( const Pattern& lhs, const Pattern& rhs ) const
    {
        return lhs.tostring(*Pattern::classDecoder) == rhs.tostring(*Pattern::classDecoder);
    }
};


typedef std::unordered_map<Pattern, IndexedData, PatternHash, PatternEqual> NerModelStore;

class NerModel{
public:
    NerModelStore data;

    IndexedCorpus *reverseindex; ///< Pointer to the corpus data for this model>

    uint64_t totaltokens; ///< Total number of tokens in the original corpus, so INCLUDES TOKENS NOT COVERED BY THE MODEL!
    uint64_t totaltypes; ///< Total number of unigram/word types in the original corpus, SO INCLUDING NOT COVERED BY THE MODEL!

    std::map<int, std::vector<uint32_t> > gapmasks; ///< pre-computed masks representing possible gap configurations for various pattern lengths

    NerModel(IndexedCorpus *reverseindex, ClassDecoder* classDecoder):reverseindex(reverseindex)
    {
        Pattern::classDecoder = classDecoder;
    }

    /**
     * @return number of pattern
     */
    size_t size() const { return this->data.size();}

    bool has(const PatternPointer &pattern) const { return this->data.find(pattern) != this->data.end();}

    /**
     * add patternpointer into model, which will cast to pattern
     * @param patternpointer
     * @param ref
     */
    void add(const PatternPointer &patternpointer, const IndexReference &ref);

    NerModelStore::iterator erase(NerModelStore ::iterator &it);
    NerModelStore::iterator begin(){return this->data.begin();}
    NerModelStore::iterator end(){return this->data.end();}



    /**
     * prune function for ngram calculation
     * @param threshold
     * @param _n
     * @param category
     * @return
     */
    unsigned int prune(int threshold, int _n = 0, int category = 0);
    unsigned int prunebylength(int _n, int category = 0, int threshold = -1);
    unsigned int pruneskipgrams(int minskiptypes, int _n = 0);


    void post_train(const PatternModelOptions &options);

    size_t occurrencecount(const PatternPointer &patternPointer);

    void train_with_ner(const std::string &filename,
                        PatternModelOptions options,
                        NerCorpus &nerCorpus,
                        ClassDecoder *classDecoder,
                        uint32_t firstsentence,
                        bool ignoreerrors);

    /**
     * train the model for skipgram
     * @param options
     * @param classDecoder
     */
    void trainskipgrams_with_ner(PatternModelOptions options, ClassDecoder *classDecoder= nullptr);


    int computeskipgrams_with_ner(const PatternPointer &pattern, PatternModelOptions &options,
                                  const IndexReference *singleref, const IndexedData *multiplerefs,
                                  ClassDecoder *classDecoder= nullptr);

    int computeflexgrams_fromskipgrams_with_ners();

    void to_csv(std::ostream &out, ClassDecoder &decoder);


};

#endif //COCO_NERMODEL_H

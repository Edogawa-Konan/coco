//
// Created by mayuan on 2020/3/19.
//


#include "bowmodel.h"


void
BowModel::compute_bow_from_ngram(const std::string &filename, PatternModelOptions options, ClassDecoder *classDecoder) {
    auto *in = new std::ifstream(filename, std::ios::binary);

    if (options.MINTOKENS == -1) options.MINTOKENS = 2;
    if (options.MINTOKENS == 0) options.MINTOKENS = 1;

    uint32_t sentence = 0;
    const unsigned char version = getdataversion(in);

//    bool iter_unigramsonly = false; //only needed for counting uni-grams when we need them but they would be discarded
//    bool skipunigrams = false; //will be set to true later only when MINTOKENS=1, MINLENGTH=1 to prevent double counting of unigrams

    if (!options.QUIET) {
        std::cerr << "compute bag of words from n-gram";
        std::cerr << ", occurrence threshold: " << options.MINTOKENS;
        if (version < 2) std::cerr << ", class encoding version: " << (int) version;
        std::cerr << std::endl;
    }

    std::vector<std::pair<PatternPointer, int> > ngrams;
    std::vector<PatternPointer> subngrams;

    bool found;
    IndexReference ref;
    int backoffn = 0;
    Pattern *linepattern = nullptr;

    unsigned int prevsize = 0;


    for (int n = 1; n <= options.MAXLENGTH; n++) {
        unsigned int foundngrams = 0;
        if(!options.QUIET)
        {
            std::cerr<<"n = "<<n<<" ... \n";
        }
        in->clear();
        if (version >= 2) {
            in->seekg(2);
        } else {
            in->seekg(0);
        }

        sentence = 0; //reset
        bool singlepass = false;
        const unsigned int sentences = (reverseindex != nullptr) ? reverseindex->sentences() : 0;
        if ((options.DEBUG) && (reverseindex != nullptr))
            std::cerr << "Reverse index sentence count: " << sentences << std::endl;
        while (((reverseindex != nullptr) && (sentence < sentences)) ||
               ((reverseindex == nullptr) && (!in->eof()))) {
            sentence++;
            delete linepattern;

            if (reverseindex == nullptr) {
                linepattern = new Pattern(in, false, version, nullptr, false);
            }
            PatternPointer line = (reverseindex != nullptr) ?
                                  reverseindex->getsentence(sentence) : PatternPointer(linepattern);

            const unsigned int linesize = line.n();

            if (options.DEBUG)
                std::cerr << "Processing line " << sentence << ", size (tokens) " << linesize << " (bytes) "
                          << line.bytesize() << ", n=" << n << std::endl;
            if (linesize == 0) {
                //skip empty lines
                continue;
            }

            ngrams.clear();
            ngrams.reserve(linesize);// 分配vector的空间
            if (options.DEBUG) std::cerr << "   (container ready)" << std::endl;

            if (options.MINTOKENS > 1) {
                line.ngrams(ngrams, n); //
            } else {
                singlepass = true;
                int minlength = options.MINLENGTH;
                line.subngrams(ngrams, minlength,
                                  options.MAXLENGTH); //extract ALL ngrams if MINTOKENS == 1, no need to look back anyway, only one iteration over corpus
            }

            if (options.DEBUG) std::cerr << "\t" << ngrams.size() << " ngrams in line" << std::endl;

            // *** ITERATION OVER ALL NGRAMS OF CURRENT ORDER (n) IN THE LINE/SENTENCE ***
            for (std::vector<std::pair<PatternPointer, int>>::iterator iter = ngrams.begin();
                 iter != ngrams.end(); iter++) {
                found = true;
                //normal behaviour: ngram (n-1) look back
                if ((n > 1) && (options.MINTOKENS > 1)) {
                    //check if sub-parts were counted
                    subngrams.clear();
                    backoffn = n - 1;
                    if (backoffn > options.MAXBACKOFFLENGTH)
                        backoffn = options.MAXBACKOFFLENGTH;
                    iter->first.ngrams(subngrams, backoffn);
                    for (std::vector<PatternPointer>::iterator iter2 = subngrams.begin();
                         iter2 != subngrams.end(); iter2++) {
                        if (!this->has(*iter2)) {
                            found = false;
                            break;
                        }
                    }
                }
                ref = IndexReference(sentence,
                                     iter->second); //this is one token, we add the tokens as we find them, one by one
                if (found) {
                    if (options.DEBUG) {
                        std::cerr << "\t\tAdding @" << ref.sentence << ":" << ref.token << " n="
                                   << " category=" << (int) iter->first.category();
                        if (classDecoder != nullptr)
                            std::cerr << "   " << iter->first.bow2string(*classDecoder);
                        std::cerr << "\n";
                    }
                    add(iter->first, ref);
                } else {
                    if (options.DEBUG) {
                        std::cerr << "\t\tSkiping @" << ref.sentence << ":" << ref.token << " n="
                                   << " category=" << (int) iter->first.category();
                        if (classDecoder != nullptr)
                            std::cerr << "   " << iter->first.bow2string(*classDecoder);
                        std::cerr << "\n";
                    }
                }
            }
        }
        foundngrams = this->size() - prevsize;
        if (!options.QUIET) std::cerr << " Found " << foundngrams << " ngrams...";
        unsigned int pruned = 0;
        if (singlepass) {
            if (options.DOSKIPGRAMS) {
                pruned = this->prune(options.MINTOKENS, 0,
                                     NGRAM); //prune regardless of size, but only n-grams (if we have a constraint model with skipgrams with count 0, we need that later and can't prune it)
            } else {
                pruned = this->prune(options.MINTOKENS, 0); //prune regardless of size
            }
        } else {
            pruned = this->prune(options.MINTOKENS, n); //prune only in size-class
            if ((!options.DOSKIPGRAMS) && (!options.DOSKIPGRAMS_EXHAUSTIVE) && (n - 1 >= 1) &&
                ((n - 1) < options.MINLENGTH) && (n - 1 != options.MAXBACKOFFLENGTH) &&
                !((n - 1 == 1) && (options.MINTOKENS_UNIGRAMS >
                                   options.MINTOKENS)) //don't delete unigrams if we're gonna need them
                    ) {
                //we don't need n-1 anymore now we're done with n, it
                //is below our threshold, prune it all (== -1)
                this->prune(-1, n - 1);
                if (!options.QUIET) std::cerr << " (pruned last iteration due to minimum length) ";
            }
        }
        if (!options.QUIET) std::cerr << "pruned " << pruned;
        if (!options.QUIET)
            std::cerr << "...total kept: " << foundngrams  - pruned << std::endl;
        prevsize = this->size();
        if (options.MINTOKENS == 1)
            break; //no need for further n iterations, we did all in one pass since there's no point in looking back
    }
    delete linepattern;
}

bool BowModel::has(const PatternPointer &patternPointer) const {
    const Pattern pattern = Pattern(patternPointer);
    BagOfWords bagOfWords = pattern.get_bag_of_words();
    if(this->data.find(bagOfWords) != this->data.end())
    {
        if(this->data.at(bagOfWords).count(pattern)==1)
            return true;
    }
    return false;
}

void BowModel::add(PatternPointer &patternPointer, IndexReference &indexReference) {
    const Pattern pattern = Pattern(patternPointer);
    BagOfWords bagOfWords = pattern.get_bag_of_words();
    this->data[bagOfWords][pattern].insert(indexReference);
}

unsigned int BowModel::prune(int threshold, int _n, int category) {
    //prune all patterns under the specified threshold (set -1 for
    //all) and of the specified length (set _n==0 for all)
    unsigned int pruned = 0;
    auto iter = this->data.begin();
    while (iter != this->data.end()) {
        auto &bagofword = iter->first;
        if (((_n == 0) || (bagofword.size() == (unsigned int) _n)) &&
            //            ((category == 0) || (pattern.category() == category)) &&
            ((threshold == -1) || (occurrencecount(bagofword) < (unsigned int) threshold))) {
//            std::cerr << "(prune: " << iter->first<<")";
            iter = this->remove(iter);
//            std::cerr << "postprune:" << this->size() << std::endl;
            pruned++;
        } else {
            iter++;
        }
    }
    return pruned;
}

unsigned int BowModel::occurrencecount(const Pattern &pattern) const {
//    if (this->pattern2index.find(pattern) != this->pattern2index.end())
//        return this->pattern2index.size();
//    else
        return 0;
}

size_t BowModel::size() const
{
    unsigned int count = 0;
    for(auto & pair: this->data)
    {
        count += this->occurrencecount(pair.first);
    }
    return count;
}

unsigned int BowModel::occurrencecount(const BagOfWords &bagOfWords) const {
    auto it = this->data.find(bagOfWords);
    unsigned int count = 0;
    for (auto &pattern2index: it->second) {
        count += pattern2index.second.size();
    }
    return count;
}

size_t BowModel::remove(const BagOfWords &bagOfWords) {
    unsigned int delete_pattern = this->occurrencecount(bagOfWords);
    this->data.erase(bagOfWords);
    return delete_pattern;
}

BagOfWordsStore::iterator BowModel::remove(BagOfWordsStore::iterator it)
{
    return this->data.erase(it);
}


void BowModel::compute_bow_from_skipgram(PatternModelOptions &options) {
    if (!options.QUIET) std::cerr << "Finding bow skip-grams on the basis of extracted bow n-grams..." << std::endl;
    for (int n = 3; n <= options.MAXLENGTH; n++) {
        if (this->gapmasks[n].empty()) this->gapmasks[n] = compute_skip_configurations(n, options.MAXSKIPS);
        if (!options.QUIET) std::cerr << "Counting " << n << "-skipgrams" << std::endl;
        unsigned int foundskipgrams = 0;
        for(auto & pair: this->data)
        {
            for(auto & pattern2index: pair.second)
            {
                const PatternPointer pattern = PatternPointer(pattern2index.first);
                IndexedData& multirefs = pattern2index.second;
                if ((pattern.n() == n) && (pattern.category() == NGRAM))
                    foundskipgrams += this->computeskipgrams(pattern, options, multirefs);
            }
        }
        if (!foundskipgrams) {
            std::cerr << " None found" << std::endl;
            break;
        }
        if (!options.QUIET) std::cerr << " Found " << foundskipgrams << " skipgrams...";
        unsigned int pruned = this->prune(options.MINTOKENS, n);
        if (!options.QUIET) std::cerr << "pruned " << pruned<<"\n";
//        unsigned int prunedextra = this->pruneskipgrams(options.MINTOKENS_SKIPGRAMS, options.MINSKIPTYPES, n);
//        if (prunedextra && !options.QUIET) std::cerr << " plus " << prunedextra << " extra skipgrams..";
//        if (!options.QUIET)
//            std::cerr << "...total kept: " << foundskipgrams - pruned - prunedextra << std::endl;
    }
}

int BowModel::computeskipgrams(const PatternPointer &pattern, PatternModelOptions &options, IndexedData &multiplerefs) {
    if (options.MINTOKENS_SKIPGRAMS < options.MINTOKENS) options.MINTOKENS_SKIPGRAMS = options.MINTOKENS;
    int mintokens = options.MINTOKENS_SKIPGRAMS;
    //internal function for computing skipgrams for a single pattern
    int foundskipgrams = 0;
    const int n = pattern.n();
    std::vector<PatternPointer> subngrams;

    if (options.DEBUG) std::cerr << "Computing skipgrams (" << gapmasks[n].size() << " permutations)" << std::endl;

    //loop over all possible gap configurations
    int gapconf_i = 0;
    for (auto iter = gapmasks[n].begin();
         iter != gapmasks[n].end(); iter++, gapconf_i++) {
        PatternPointer skipgram = pattern;
        skipgram.mask = *iter;
        if (options.DEBUG) {
            std::cerr << "Checking for skipgram: " << std::endl;
            skipgram.out();
            if ((int) skipgram.n() != n) {
                std::cerr << "Generated invalid skipgram, n=" << skipgram.n() << ", expected " << n
                          << std::endl;
                throw InternalError();
            }
        }
        bool skipgram_valid = true;  // whether current skipgram is valid. default true.
        if (mintokens != 1) {
            bool check_extra = false;
            //check if sub-parts were counted
            subngrams.clear();
            skipgram.ngrams(subngrams, n - 1); //this also works for and returns skipgrams, despite the name
            for (const auto& subpattern : subngrams) { //only two patterns
                if (!subpattern.isgap(0) && !subpattern.isgap(subpattern.n() - 1u)) {
                    //this subpattern is a valid
                    //skipgram or ngram (no beginning or ending
                    //gaps) that should occur
                    if (options.DEBUG) {
                        std::cerr << "Subpattern: " << std::endl;
                        subpattern.out();
                    }
                    if (!this->has(subpattern)) {
                        if (options.DEBUG) std::cerr << "  discarded" << std::endl;
                        skipgram_valid = false;
                        break;
                    }
                } else {
                    //this check isn't enough, subpattern
                    //starts or ends with gap
                    //do additional checks
                    check_extra = true;
                    break;
                }
            }
            if (!skipgram_valid) continue;
            if (check_extra) {
                //check whether the gaps with single token context (X * Y) occur in model,
                //otherwise skipgram can't occur
                const std::vector<std::pair<int, int>> gapconfiguration = mask2vector(skipgram.mask, n);
                for (std::vector<std::pair<int, int>>::const_iterator iter3 = gapconfiguration.begin();
                     iter3 != gapconfiguration.end(); iter3++) {
                    if (!((iter3->first - 1 == 0) &&
                          (iter3->first + iter3->second + 1 == n))) { //entire skipgram is already X * Y format
                        const PatternPointer subskipgram = PatternPointer(skipgram, iter3->first - 1,
                                                                          iter3->second + 2, nullptr, false, true);
                        if (options.DEBUG) {
                            std::cerr << "Subskipgram: " << std::endl;
                            subskipgram.out();
                        }
                        if (!this->has(subskipgram)) {
                            if (options.DEBUG) std::cerr << "  discarded" << std::endl;
                            skipgram_valid = false;
                            break;
                        }
                    }
                }
            }
        }
        if (skipgram_valid) {
            if (options.DEBUG) std::cerr << "  counted!" << std::endl;
            //put in model
            if (!has(skipgram)) foundskipgrams++;
            for (auto ref : multiplerefs) {
                add(skipgram, ref); //counts the actual skipgram, will add it to the model
            }
        }
    }
    return foundskipgrams;
}

unsigned int BowModel::pruneskipgrams(int minskiptypes, int _n) {
//    unsigned int pruned = 0;
//    if (minskiptypes <= 1) return pruned; //nothing to do
//    auto iter = this->pattern2index.begin();
//    while (iter != this->pattern2index.end()) {
//        auto pattern = iter->first;
//        if (((_n == 0) || ((int) pattern.n() == _n)) && (pattern.category() == SKIPGRAM)) {
//            t_relationmap skipcontent = getskipcontent(pattern);
//            //std::cerr << " Pattern " << pattern.hash() << " occurs: " << this->occurrencecount(pattern) << " skipcontent=" << skipcontent.size() << std::endl;
//            if ((int) skipcontent.size() <
//                minskiptypes) { //will take care of token threshold too, patterns not meeting the token threshold are not included
//                //std::cerr << "..pruning" << std::endl;
//                iter = this->erase(iter);
//                pruned++;
//                continue;
//            }
//        }
//        iter++;
//    }
//    return pruned;
    return 0;
}

void BowModel::to_csv(std::ostream &out, ClassDecoder &decoder) {
    std::cerr<<"Total bag of words set :"<<this->size()<<"\n";
    std::cerr<<"writing to file...\n";

    out << "PATTERN\tCOUNT\tCATEGORY\tREFERENCES" << std::endl;
    for(auto & pair: this->data)
    {
        auto count = this->occurrencecount(pair.first);
        int i = 0;
        out<<pair.first.tostring(decoder)<<"\t"<<count<<"\t"<<BOW<<"\t";
        for (const auto & pattern2index :pair.second) {
            for (const auto & index : pattern2index.second)
            {
                i++;
                out << index.tostring();
                if (i < count)
                    out << " ";
            }
        }
        out << "\n";
    }
    std::cerr<<"write done ...\n";
}





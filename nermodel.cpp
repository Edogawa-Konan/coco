//
// Created by mayuan on 2020/3/3.
//

#include "nermodel.h"

void NerModel::train_with_ner(const std::string &filename, PatternModelOptions options, NerCorpus &nerCorpus,
                              ClassDecoder *classDecoder, uint32_t firstsentence, bool ignoreerrors) {
    std::ifstream In(filename, std::ios::binary);
    std::ifstream *in = &In;
    if (!In) {
        std::cerr << "failed to open file <" << filename << "> " << std::endl;
        return;
    }

    if (options.MINTOKENS == -1) options.MINTOKENS = 2;
    if (options.MINTOKENS == 0) options.MINTOKENS = 1;
    if (options.MINTOKENS_SKIPGRAMS < options.MINTOKENS) options.MINTOKENS_SKIPGRAMS = options.MINTOKENS;

    uint32_t sentence;
    const unsigned char version = (in != nullptr) ? getdataversion(in) : 2;

    bool iter_unigramsonly = false; //only needed for counting unigrams when we need them but they would be discarded
    bool skipunigrams = false; //will be set to true later only when MINTOKENS=1,MINLENGTH=1 to prevent double counting of unigrams

    if (((options.MINLENGTH > 1) || (options.MINTOKENS == 1)) && (options.MINTOKENS_UNIGRAMS > options.MINTOKENS)) {
        iter_unigramsonly = true;
    }
    if (!options.QUIET) {
        std::cerr << "Training patternmodel";
        std::cerr << ", occurrence threshold: " << options.MINTOKENS;
        if (iter_unigramsonly) std::cerr << ", secondary word occurrence threshold: " << options.MINTOKENS_UNIGRAMS;
        if (version < 2) std::cerr << ", class encoding version: " << (int) version;
        std::cerr << std::endl;


    }

    if (options.DOSKIPGRAMS && options.DOSKIPGRAMS_EXHAUSTIVE) {
        std::cerr
                << "ERROR: Both DOSKIPGRAMS as well as DOSKIPGRAMS_EXHAUSTIVE are set, this shouldn't happen, choose one."
                << std::endl;
        if (!ignoreerrors) throw InternalError();
        options.DOSKIPGRAMS = false;
    }

    std::vector<std::pair<PatternPointer, int> > ngrams;
    std::vector<PatternPointer> subngrams;
    bool found;
    IndexReference ref;
    int prevsize = this->size();  // pattern map's size
    int backoffn = 0;
    Pattern *linepattern = nullptr;

//    if (!this->data.empty()) {
//        this->computestats();
//    }

    for (int n = 1; n <= options.MAXLENGTH; n++) {
        bool skipgramsonly = false; //only used when continued==true, prevent double counting of n-grams whilst allowing skipgrams to be counted later
        int foundngrams = 0;
        int foundskipgrams = 0;
        if (in != nullptr) {
            in->clear();
            if (version >= 2) {
                in->seekg(2);
            } else {
                in->seekg(0);
            }
        }
        if (!options.QUIET) {
            if (iter_unigramsonly) {
                std::cerr << "Counting unigrams using secondary word occurrence threshold ("
                          << options.MINTOKENS_UNIGRAMS << ")" << std::endl;
            } else if (options.DOPATTERNPERLINE) {
                std::cerr << "Counting patterns from list, one per line" << std::endl;
            } else if (options.MINTOKENS > 1) {
                std::cerr << "Counting " << n << "-grams" << std::endl;
                if (skipgramsonly)
                    std::cerr << "(only counting skipgrams actually, n-grams already counted earlier)" << std::endl;
            } else {
                std::cerr << "Counting *all* n-grams (occurrence threshold=1)" << std::endl;
            }
        }

        sentence = firstsentence - 1; //reset
        bool singlepass = false;
        const unsigned int sentences = (reverseindex != NULL) ? reverseindex->sentences() : 0;
        if ((options.DEBUG) && (reverseindex != NULL))
            std::cerr << "Reverse index sentence count: " << sentences << std::endl;

        /*  iterate entire corpus ------------------------------------------------- */

        while (((reverseindex != NULL) && (sentence < sentences)) ||
               ((reverseindex == NULL) && (in != nullptr) && (!in->eof()))) {
            sentence++;
            //read line
            delete linepattern;
            if (reverseindex == NULL) {
                linepattern = new Pattern(in, false, version, nullptr, false);
            }
            PatternPointer line = (reverseindex != NULL) ? reverseindex->getsentence(sentence) : PatternPointer(
                    linepattern);

            const unsigned int linesize = line.n();
            if (options.DEBUG)
                std::cerr << "Processing line " << sentence << ", size (tokens) " << linesize << " (bytes) "
                          << line.bytesize() << ", n=" << n << std::endl;
            if (linesize == 0) {
                //skip empty lines
                continue;
            }
            //count total tokens
            if (n == 1) totaltokens += linesize;

            ngrams.clear();
            ngrams.reserve(linesize);// 分配vector的空间
            if (options.DEBUG) std::cerr << "   (container ready)" << std::endl;
            if (options.DOPATTERNPERLINE) {
                if (linesize > (unsigned int) options.MAXLENGTH) continue;
                ngrams.emplace_back(line, 0);
            } else {
                // ----
                if (iter_unigramsonly) {
                    line.ngrams(ngrams, n, nerCorpus[sentence - 1]);
                } else if (options.MINTOKENS > 1) {
                    line.ngrams(ngrams, n, nerCorpus[sentence - 1]); //
                } else {
                    singlepass = true;
                    int minlength = options.MINLENGTH;
                    line.subngrams(ngrams, minlength,
                                   options.MAXLENGTH, nerCorpus[sentence -
                                                                1]); //extract ALL ngrams if MINTOKENS == 1 or a constraint model is set, no need to look back anyway, only one iteration over corpus
                }
            }
            if (options.DEBUG) std::cerr << "\t" << ngrams.size() << " ngrams in line" << std::endl;

//            PatternPointer *tmp = nullptr;

            // *** ITERATION OVER ALL NGRAMS OF CURRENT ORDER (n) IN THE LINE/SENTENCE ***
            for (std::vector<std::pair<PatternPointer, int>>::iterator iter = ngrams.begin();
                 iter != ngrams.end(); iter++) {
                try {
                    if ((singlepass) && (options.MINLENGTH == 1) && (skipunigrams) && (iter->first.n() == 1)) {
                        //prevent double counting of unigrams after a iter_unigramsonly run with mintokens==1
                        continue;
                    }
                    if (!skipgramsonly) {

                        found = true; //are the submatches in order? (default to true, attempt to falsify, needed for mintokens==1)

                        //unigram check, special scenario, not usually processed!! (normal lookback suffices for most uses)
                        if ((!iter_unigramsonly) && (options.MINTOKENS_UNIGRAMS > options.MINTOKENS) &&
                            ((n > 1) || (singlepass))) {
                            subngrams.clear();
                            iter->first.ngrams(subngrams, 1); //get all unigrams
                            for (std::vector<PatternPointer>::iterator iter2 = subngrams.begin();
                                 iter2 != subngrams.end(); iter2++) {
                                //check if unigram reaches threshold
                                if (this->occurrencecount(*iter2) < (unsigned int) options.MINTOKENS_UNIGRAMS) {
                                    found = false;
                                    break;
                                }
                            }
                        }

                        //normal behaviour: ngram (n-1) look back
                        if ((found) && (n > 1) && (options.MINTOKENS > 1) && (!options.DOPATTERNPERLINE)) {
                            //check if sub-parts were counted
                            subngrams.clear();
                            backoffn = n - 1;
                            if (backoffn > options.MAXBACKOFFLENGTH) backoffn = options.MAXBACKOFFLENGTH;
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

//                        if(iter->first.tostring(*classDecoder)=="all emails deleted between")
//                        {
//                            if(!tmp)
//                                tmp = &(iter->first);
//                            else
//                                std::cerr<<(*tmp==iter->first)<<"\n";
//                        }
                        if ((found) && (!skipgramsonly)) {
                            if (options.DEBUG) {
                                std::cerr << "\t\tAdding @" << ref.sentence << ":" << ref.token << " n="
                                          << iter->first.n() << " category=" << (int) iter->first.category()
                                          << "   " << iter->first.tostring(*classDecoder) << "  ||  ";
                                for (const Ner &ner : iter->first.ners)
                                    std::cerr << ner << " ";
                                std::cerr << std::endl;
                            }
                            add(iter->first, ref);
                        } else {
                            if (options.DEBUG) {
                                std::cerr << "\t\tSkiping @" << ref.sentence << ":" << ref.token << " n="
                                          << iter->first.n() << " category=" << (int) iter->first.category()
                                          << "   " << iter->first.tostring(*classDecoder) << "  ||  ";
                                for (const Ner &ner : iter->first.ners)
                                    std::cerr << ner << " ";
                                std::cerr << std::endl;
                            }

                        }
                    }
                } catch (std::exception &e) {
                    std::cerr << "ERROR: An internal error has occured during training!!!" << std::endl;
                    if (ignoreerrors) continue;
                    throw InternalError();
                }
            }
        }
        if (!iter_unigramsonly) {
            foundngrams = this->size() - foundskipgrams - prevsize;
            if(foundngrams==0)
            {
                if (!options.QUIET) std::cerr << "None found" << std::endl;
                break;
            }
            if (!options.QUIET) std::cerr << " Found " << foundngrams << " ngrams...";
            if (options.DOSKIPGRAMS_EXHAUSTIVE && !options.QUIET)
                std::cerr << foundskipgrams << " skipgram occurrences...";

            if ((options.MINTOKENS > 1) && (n == 1)) {
                totaltypes = this->size(); //total unigrams, also those not in model
            }

            unsigned int pruned;
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

//            if (foundskipgrams) {
//                unsigned int prunedextra;
//                if (options.MINTOKENS == 1) {
//                    prunedextra = this->pruneskipgrams(options.MINTOKENS_SKIPGRAMS, options.MINSKIPTYPES, 0);
//                } else {
//                    prunedextra = this->pruneskipgrams(options.MINTOKENS_SKIPGRAMS, options.MINSKIPTYPES, n);
//                }
//                if (prunedextra && !options.QUIET) std::cerr << " plus " << prunedextra << " extra skipgrams..";
//                pruned += prunedextra;
//            }
            if (!options.QUIET)
                std::cerr << "...total kept: " << (foundngrams + foundskipgrams) - pruned << std::endl;
            if (options.MINTOKENS == 1)
                break; //no need for further n iterations, we did all in one pass since there's no point in looking back
        } else { //iter_unigramsonly
            if (!options.QUIET) {
                std::cerr << "found " << this->size() << std::endl;
            }


            if (!options.QUIET) std::cerr << " computing total word types prior to pruning...";
            totaltypes = this->size();
            if (!options.QUIET) std::cerr << totaltypes << "...";

            //prune the unigrams based on the word occurrence threshold
            this->prune(options.MINTOKENS_UNIGRAMS, 1);
            //normal behaviour next round
            iter_unigramsonly = false;
            if ((n == 1) && (options.MINLENGTH == 1)) skipunigrams = true; //prevent double counting of unigrams
            //decrease n so it will be the same (always 1) next (and by definition last) iteration
            n--;
        }
        prevsize = this->size();
    }

    if (options.DOSKIPGRAMS && !options.DOSKIPGRAMS_EXHAUSTIVE) {
        this->trainskipgrams_with_ner(options, nullptr);
    }
    if (options.MAXBACKOFFLENGTH < options.MINLENGTH) {
        this->prune(-1, options.MAXBACKOFFLENGTH);
    }
    if ((options.MINLENGTH > 1) && (options.MINTOKENS_UNIGRAMS > options.MINTOKENS)) {
        //prune the unigrams again
        this->prune(-1, 1);
    }
    if ((options.MINLENGTH > 1) && (options.DOSKIPGRAMS || options.DOSKIPGRAMS_EXHAUSTIVE)) {
        unsigned int pruned = this->prunebylength(options.MINLENGTH - 1);
        if (!options.QUIET)
            std::cerr << " pruned " << pruned << " patterns below minimum length (" << options.MINLENGTH << ")"
                      << std::endl;
    }
    this->post_train(options);
    delete linepattern;
}

size_t NerModel::occurrencecount(const PatternPointer &patternPointer) {
    Pattern pattern(patternPointer);
    if(this->data.find(pattern) == this->data.end())
    {
        return 0;
    } else
        return this->data[pattern].size();
}

void NerModel::add(const PatternPointer &patternpointer, const IndexReference &ref) {
    Pattern pattern(patternpointer);
    this->data[pattern].insert(ref);
}

unsigned int NerModel::prune(int threshold, int _n, int category) {
    //prune all patterns under the specified threshold (set -1 for
    //all) and of the specified length (set _n==0 for all)
    unsigned int pruned = 0;
    auto iter = this->data.begin();
    while (iter != this->data.end()) {
        Pattern pattern = iter->first;
        if (((_n == 0) || (pattern.n() == (unsigned int) _n)) &&
            ((category == 0) || (pattern.category() == category)) &&
            ((threshold == -1) || (occurrencecount(pattern) < (unsigned int) threshold))) {
            iter = this->erase(iter);
            pruned++;
        } else {
            iter++;
        }
    }
    return pruned;
}

NerModelStore::iterator NerModel::erase(NerModelStore ::iterator &it) {
    return this->data.erase(it);
}

void NerModel::post_train(const PatternModelOptions &options) {
    if (!options.QUIET) std::cerr << "Sorting all indices..." << std::endl;
    for (auto iter = this->begin();
         iter != this->end(); iter++) {
        iter->second.sort();
    }
}

void NerModel::to_csv(std::ostream &out, ClassDecoder &decoder) {
    std::cerr << "Total pattern " << this->size() << std::endl;
    int number = 0;
//        std::stringstream buffer;
//        out << "PATTERN\tCOUNT\tCATEGORY\tREFERENCES" << std::endl;
    out << "PATTERN\tCOUNT\tCATEGORY\tREFERENCES" << std::endl;

    for (auto &pattern2index: this->data) {
        unsigned int count = pattern2index.second.size();
        int i = 0;
        double category = pattern2index.first.category();
        if (!pattern2index.first.ners.empty()) {
            category += 0.5;
        }
        out << pattern2index.first.tostring(decoder) << "\t" << count << "\t" << category << "\t";
        for (const auto &index :pattern2index.second) {
            i++;
            out << index.tostring();
            if (i < count)
                out << " ";
        }
        out << "\n";
        number++;
//        if (number % 10000 == 0)
//            std::cerr << "have write  " << number << " patterns\n";
//        if(pattern2index.first.tostring(decoder)=="received {**} jane #")
//            std::cerr<<pattern2index.first.tostring(decoder)<<"\n";

    }
}

void NerModel::trainskipgrams_with_ner(PatternModelOptions options, ClassDecoder *classDecoder) {
    if (options.MINTOKENS == -1) options.MINTOKENS = 2;

    //normal behaviour: extract skipgrams from n-grams
    if (!options.QUIET) std::cerr << "Finding skipgrams on the basis of extracted n-grams..." << std::endl;

    for (int n = 3; n <= options.MAXLENGTH; n++) {
        if (this->gapmasks[n].empty()) this->gapmasks[n] = compute_skip_configurations(n, options.MAXSKIPS);
        if (!options.QUIET) std::cerr << "Counting " << n << "-skipgrams" << std::endl;
        unsigned int foundskipgrams = 0;
        for (auto iter = this->begin(); iter != this->end(); iter++) {
            const PatternPointer pattern = PatternPointer(&(iter->first));
            const IndexedData multirefs = iter->second;
            if ((pattern.n() == n) && (pattern.category() == NGRAM))
                foundskipgrams += this->computeskipgrams_with_ner(pattern, options, nullptr, &multirefs, classDecoder);
        }
        if (!foundskipgrams) {
            std::cerr << " None found" << std::endl;
            break;
        }
        if (!options.QUIET) std::cerr << " Found " << foundskipgrams << " skipgrams...";
        unsigned int pruned = this->prune(options.MINTOKENS, n);
        if (!options.QUIET) std::cerr << "pruned " << pruned;
        unsigned int prunedextra = this->pruneskipgrams(options.MINSKIPTYPES, n);
        if (prunedextra && !options.QUIET) std::cerr << " plus " << prunedextra << " extra skipgrams..";
        if (!options.QUIET)
            std::cerr << "...total kept: " << foundskipgrams - pruned - prunedextra << std::endl;
    }
}

int NerModel::computeskipgrams_with_ner(const PatternPointer &pattern, PatternModelOptions &options,
                                        const IndexReference *singleref, const IndexedData *multiplerefs,
                                        ClassDecoder *classDecoder) {
    if (options.MINTOKENS_SKIPGRAMS < options.MINTOKENS) options.MINTOKENS_SKIPGRAMS = options.MINTOKENS;
    int mintokens = options.MINTOKENS_SKIPGRAMS;
    if (mintokens == -1) mintokens = 2;
    if (mintokens <= 1) {
        mintokens = 1;
    }
    //internal function for computing skipgrams for a single pattern
    int foundskipgrams = 0;
    const int n = pattern.n();
    std::vector<PatternPointer> subngrams;

    if (gapmasks[n].empty()) gapmasks[n] = compute_skip_configurations(n, options.MAXSKIPS);

    if (options.DEBUG) std::cerr << "Computing skipgrams (" << gapmasks[n].size() << " permutations)" << std::endl;

    //loop over all possible gap configurations
    int gapconf_i = 0;
    for (std::vector<uint32_t>::iterator iter = gapmasks[n].begin();
         iter != gapmasks[n].end(); iter++, gapconf_i++) {
        if (*iter == 0) continue;
        try {
            PatternPointer skipgram = pattern;
            skipgram.mask = *iter;
            if (options.DEBUG) {
                std::cerr << "Checking for skipgram: " << std::endl;
                skipgram.out();
            }
            if (options.DEBUG) {
                if (skipgram.n() != n) {
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
                for (std::vector<PatternPointer>::iterator iter2 = subngrams.begin();
                     iter2 != subngrams.end(); iter2++) { //only two patterns
                    const PatternPointer subpattern = *iter2;
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
                                                                              iter3->second + 2, nullptr, false,
                                                                              true);
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
//                if(skipgram ==)
                if (singleref != nullptr) {
                    add(skipgram, *singleref); //counts the actual skipgram, will add it to the model
                } else if (multiplerefs != nullptr) {
                    for (IndexedData::const_iterator refiter = multiplerefs->begin();
                         refiter != multiplerefs->end(); refiter++) {
                        const IndexReference ref = *refiter;
                        add(skipgram, ref); //counts the actual skipgram, will add it to the model
                    }
                } else {
                    std::cerr
                            << "ERROR: computeskipgrams() called with no singleref, no multiplerefs"
                            << std::endl;
                    throw InternalError();
                }
            }
        } catch (InternalError &e) {
            std::cerr << "IGNORING ERROR and continuing with next skipgram" << std::endl;
        }
    }
    return foundskipgrams;
    return 0;
}

unsigned int NerModel::pruneskipgrams(int minskiptypes, int _n) {
    int pruned = 0;
//    if (minskiptypes <= 1) return pruned; //nothing to do
//
//    auto iter = this->begin();
//    while (iter != this->end()) {
//        Pattern pattern = iter->first;
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
    return pruned;
}

int NerModel::computeflexgrams_fromskipgrams_with_ners() {
    int count = 0;
    for (auto iter = this->begin();
         iter != this->end(); iter++) {
        Pattern pattern = iter->first;
        if (pattern.category() == SKIPGRAM) {
            Pattern flexgram = pattern.toflexgram_with_ner();
            if (!this->has(flexgram)) count++;
            //copy data from pattern
            IndexedData indexedData = this->data[pattern];
            for (IndexReference & ref: indexedData) {
                this->data[flexgram].insert(ref);
            }
        }
    }
    return count;
}

unsigned int NerModel::prunebylength(int _n, int category, int threshold) {
    //prune all patterns under the specified threshold (set -1 for
    //all) and of the specified length (set _n==0 for all)
    unsigned int pruned = 0;
    auto iter = this->begin();
    while (iter != this->end()) {
        Pattern pattern = iter->first;
        if ((pattern.n() <= (unsigned int) _n) && ((category == 0) || (pattern.category() == category)) &&
            ((threshold == -1) || (occurrencecount(pattern) < (unsigned int) threshold))) {
            /*std::cerr << "preprune:" << this->size() << std::endl;
                std::cerr << "DEBUG: pruning " << (int) pattern.category() << ",n=" << pattern.n() << ",skipcount=" << pattern.skipcount() << ",hash=" << pattern.hash() << std::endl;
                std::cerr << occurrencecount(pattern) << std::endl;*/
            iter = this->erase(iter);
            //std::cerr << "postprune:" << this->size() << std::endl;
            pruned++;
        } else {
            iter++;
        }
    }
    return pruned;
}

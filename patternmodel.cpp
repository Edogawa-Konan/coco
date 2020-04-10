#include "patternmodel.h"


int get_ngrams_with_ner(PatternPointer &line, std::vector<std::pair<PatternPointer, int>> &container, int n,
                        const std::vector<Ner> *ners_, ClassEncoder *classEncoder,
                        ClassDecoder *classDecoder)
{
    const int _n = line.n();
    if (n > _n) return 0;
    int found = 0;
    size_t byteoffset = 0;
    size_t pattern_start = 0;   // Current pattern start, from 1
    for (int i = 0; i < (_n - n) + 1; i++) {
        PatternPointer no_ners(line, 0, n, &byteoffset, true);
        pattern_start++;
        auto it = std::lower_bound(ners_->begin(), ners_->end(), pattern_start,
                                   [](const Ner &obj, const unsigned int &start) { return obj.start < start; });
        std::vector<Ner> pattern_has_ners;
        for (; it != ners_->cend() && it->start >= pattern_start &&
               (it->start + it->len) <= (pattern_start + n); it++) {
            pattern_has_ners.push_back(*it);
            pattern_has_ners.back().start -= pattern_start;
        }
        if (!pattern_has_ners.empty()) {
            std::vector<PatternPointer> res = PatternPointer::bruteforce(pattern_has_ners, no_ners);
            found += res.size();
            for (const PatternPointer &pp: res) {
                std::string patternstring = pp.tostring(*classDecoder);
                for (auto & key: pp.ners)
                {//
                    if(!classEncoder->contains(key.raw_string))
                        std::cerr<<key.raw_string<<" not in encoderclass\n";
                }
                PatternPointer new_pattern = classEncoder->buildpatternpointer(patternstring);
                assert(patternstring == new_pattern.tostring(*classDecoder));
                container.emplace_back(new_pattern, i);
            }
        } else {
            container.emplace_back(no_ners, i);
            found++;
        }
    }
    return found;
}



int getmodeltype(const std::string &filename) {
    unsigned char null;
    unsigned char model_type;
    unsigned char model_version;
    std::ifstream *f = new std::ifstream(filename.c_str());
    f->read((char *) &null, sizeof(char));
    f->read((char *) &model_type, sizeof(char));
    f->read((char *) &model_version, sizeof(char));
    f->close();
    delete f;
    if (null != 0) return -1;
    return model_type;
}

/**
 * Computation of log likelihood  for a single pattern, accross corpora.
 *
 * Computation is as in Rayson and Garside (2000), Comparing corpora using frequency profiling. In proceedings of the workshop on Comparing Corpora, held in conjunction with the 38th annual meeting of the Association for Computational Linguistics (ACL 2000). 1-8 October 2000, Hong Kong, pp. 1 - 6: http://www.comp.lancs.ac.uk/~paul/publications/rg_acl2000.pdf
 */
double comparemodels_loglikelihood(const Pattern pattern, std::vector<PatternModel<uint32_t> *> &models) {
    if (models.size() < 2) {
        std::cerr << "compare_models_loglikelihood requires at least two models!" << std::endl;
        throw InternalError();
    }

    int n = 0; //total
    int o = 0; //observed
    double e = 0; //expected
    double ll = 0; //Log Likelihood
    double e_log = 0;

    unsigned long long int n_sum = 0;
    unsigned long long int o_sum = 0;

    std::vector<int> observed;
    std::vector<int> total;
    std::vector<double> expected;


    for (unsigned int i = 0; i < models.size(); i++) {
        o = models[i]->occurrencecount(pattern);
        //n = models[i]->totaloccurrencesingroup(category,patternsize);
        n = models[i]->tokens();
        total.push_back(n);
        n_sum += n;
        observed.push_back(o);
        o_sum += o;
    }


    for (unsigned int i = 0; i < total.size(); i++) {
        e_log = (log(total[i]) + log(o_sum)) -
                log(n_sum); // do computation in logarithms to prevent overflows! e = (total[i] * o_sum) / n_sum
        e = exp(e_log);
        expected.push_back(e);
    }

    ll = 0;
    for (unsigned int i = 0; i < models.size(); i++) {
        if (observed[i] > 0)
            ll += observed[i] * log(observed[i] / expected[i]);
    }
    ll = ll * 2;
    if (std::isnan(ll)) ll = 0; //value too low, set to 0

    return ll;
}


/**
 * Computation of log likelihood between patterns in corpora
 *
 * Computation is as in Rayson and Garside (2000), Comparing corpora using frequency profiling. In proceedings of the workshop on Comparing Corpora, held in conjunction with the 38th annual meeting of the Association for Computational Linguistics (ACL 2000). 1-8 October 2000, Hong Kong, pp. 1 - 6: http://www.comp.lancs.ac.uk/~paul/publications/rg_acl2000.pdf
 */
void comparemodels_loglikelihood(std::vector<PatternModel<uint32_t> *> &models, PatternMap<double> *resultmap,
                                 bool conjunctiononly, std::ostream *output, ClassDecoder *classdecoder) {
    if (models.size() < 2) {
        std::cerr << "compare_models_loglikelihood requires at least two models!" << std::endl;
        throw InternalError();
    }


    int n = 0; //total
    int o = 0; //observed
    double e = 0; //expected
    double ll = 0; //Log Likelihood
    double e_log = 0;

    unsigned long long int n_sum = 0;
    unsigned long long int o_sum = 0;

    std::vector<int> observed;
    std::vector<int> total;
    std::vector<double> expected;

    if (output != NULL) {
        *output << "PATTERN\tLOGLIKELIHOOD";
        for (unsigned int i = 0; i < models.size(); i++) {
            *output << "\tOCC_" << i << "\tFREQ_" << i;
        }
    }

    int startmodel = 0;
    int endmodel = models.size();

    if (conjunctiononly) {
        startmodel = 1;
        endmodel = 2;
    }


    for (int m = startmodel; m < endmodel; m++) {
        for (PatternModel<uint32_t>::iterator iter = models[m]->begin(); iter != models[m]->end(); iter++) {
            const Pattern pattern = iter->first;
            if ((!conjunctiononly) && (resultmap->has(pattern))) continue; //already done


            observed.clear();
            total.clear();
            expected.clear();

            n_sum = 0;
            o_sum = 0;


            bool abort = false;
            for (unsigned int i = 0; i < models.size(); i++) {
                o = models[i]->occurrencecount(pattern);
                if ((conjunctiononly) && (o == 0)) {
                    abort = true;
                    break;
                }
                //n = models[i]->totaloccurrencesingroup(category,patternsize);
                n = models[i]->tokens();
                total.push_back(n);
                n_sum += n;
                observed.push_back(o);
                o_sum += o;
            }

            if (abort) continue;

            for (unsigned int i = 0; i < total.size(); i++) {
                e_log = (log(total[i]) + log(o_sum)) -
                        log(n_sum); // do computation in logarithms to prevent overflows! e = (total[i] * o_sum) / n_sum
                e = exp(e_log);
                //std::cerr << "DEBUG: e = " << e << " = (" << total[i] << " * " << o_sum << ") / " << n_sum << std::endl;
                expected.push_back(e);
            }


            ll = 0;
            for (unsigned int i = 0; i < models.size(); i++) {
                if (observed[i] > 0) {
                    //std::cerr << "DEBUG: observed[" << i << "] = " << observed[i] << "  expected[" << i << "] = " << expected[i] << " total[" << i << "] = " << total[i] << std::endl;
                    ll = ll + (observed[i] * log(observed[i] / expected[i]));
                }
            }
            ll = ll * 2;
            if (std::isnan(ll)) ll = 0; //value too low, set to 0
            //std::cerr << "DEBUG: ll = " << ll << std::endl;

            if (resultmap != NULL)
                (*resultmap)[pattern] = ll;

            if ((output != NULL) && (classdecoder != NULL)) {
                *output << pattern.tostring(*classdecoder) << "\t" << ll;
                for (unsigned int i = 0; i < models.size(); i++) {
                    *output << "\t" << observed[i] << "\t";
                    if (total[i] > 0) {
                        *output << observed[i] / total[i];
                    } else {
                        *output << 0;
                    }
                }
                *output << std::endl;
            }
        }
    }

}



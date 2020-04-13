#include <iostream>
#include <string>
#include "classencoder.h"
#include "classdecoder.h"
#include "patternmodel.h"


int main() {
//    std::cout << VERSION << std::endl;


    auto filename = R"(/Users/prime/CLionProjects/DP-colibri-core/data/corpus.txt)";
    auto encoding_file = R"(/Users/prime/CLionProjects/DP-colibri-core/data/corpus.dat)";
    auto class_file = R"(/Users/prime/CLionProjects/DP-colibri-core/data/corpus.cls)";

    ClassEncoder classEncoder = ClassEncoder();
    classEncoder.build(filename, 0);
    classEncoder.save(class_file);
    classEncoder.encodefile(filename, encoding_file, false);

    ClassDecoder classDecoder = ClassDecoder(class_file);

    auto options = PatternModelOptions();
    options.MINTOKENS = 2;
    options.MAXLENGTH = 8;
//    options.DEBUG = true;
    options.QUIET = false;
//    options.DOSKIPGRAMS = true;

    auto indexedCorpus = IndexedCorpus(encoding_file, true);

    auto model = IndexedPatternModel(&indexedCorpus);
    model.train(encoding_file, options,  nullptr, nullptr, false, 1, false, &classDecoder);
//    model.computeflexgrams_fromskipgrams();
    model.computeflexgrams_fromcooc(1);
    std::cout << "Total patterns: " << model.size() << std::endl;
    for (auto &pattern2index : model) {
        std::cout << pattern2index.first.tostring(classDecoder) << " >>>>>> ";
        for (const auto &index :pattern2index.second) {
            std::cout << index.tostring() << "  ";
        }
        std::cout << std::endl;
    }


    return 0;
}
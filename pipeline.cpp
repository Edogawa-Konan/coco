#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include "classencoder.h"
#include "classdecoder.h"
#include "patternmodel.h"
//#include <filesystem>
#include "cxxopts.hpp"
#include <chrono>
#include "nermodel.h"
#include "pattern.h"



using std::cout;
using std::cerr;
using std::endl;


int main(int argc, char** argv)
{
//    std::filesystem::path current_path = std::filesystem::current_path();

    auto start = std::chrono::high_resolution_clock::now();

    std::string filename;
    std::string classfile;
    std::string encodingfile;
    std::string ner_path;
    std::string output;
    auto modeloptions = PatternModelOptions();


    bool debug;
    bool do_skipgram, do_flexgram;

    cxxopts::Options options("coco", "Colibri Core tools");

    options.add_options()
            ("d,debug", "Param bar", cxxopts::value<bool >()->default_value("false"))
            ("ner_path", "Location of ner file (absolute path)", cxxopts::value<std::string>()->default_value(""))
            ("f,file", "Location of corpus file (absolute path)", cxxopts::value<std::string>()->default_value(""))
            ("h,help", "Print usage")
            ("skipgram", "whether extract skipgram? default false", cxxopts::value<bool >()->default_value("false"))
            ("flexgram", "whether extract flexgram? flexgram base on skipgram, if set true, ignore skipgram option! default false", cxxopts::value<bool >()->default_value("false"))
            ("o,output", "Location to save result (absolute path)", cxxopts::value<std::string >()->default_value(""))
            ("min_tokens", "minimum amount of occurrences for a pattern to be included in a model, default=2", cxxopts::value<int>()->default_value("2"))
            ("n,max_length", "The maximum length of patterns to be loaded/extracted, inclusive (in words/tokens), default=8", cxxopts::value<int>()->default_value("8"))
            ;


    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    try {
        filename = result["file"].as<std::string>();
        ner_path = result["ner_path"].as<std::string>();
        debug = result["debug"].as<bool>();
        do_skipgram = result["skipgram"].as<bool>();
        do_flexgram = result["flexgram"].as<bool>();
        output = result["output"].as<std::string>();

        modeloptions.MINTOKENS = result["min_tokens"].as<int>();
        modeloptions.MAXLENGTH = result["max_length"].as<int>();
        if(do_flexgram)
            do_skipgram = true;
        modeloptions.DOSKIPGRAMS = do_skipgram;
        modeloptions.DEBUG = debug;


    }catch (cxxopts::OptionParseException& e )
    {
        cerr<<e.what()<<endl;
        exit(0);
    }

    std::string target_path(filename);
//    encodingfile = target_path.replace_extension("dat");
//    classfile = target_path.replace_extension("cls");
//
//    if(output.empty())
//        output = target_path.replace_filename("result.csv");

    if(target_path.size() > 4 && *(target_path.end() - 4) == '.')
    {
        encodingfile = target_path.replace(target_path.end()-4, target_path.end(), ".dat");
        classfile = target_path.replace(target_path.end()-4, target_path.end(), ".cls");
    } else{
        encodingfile = target_path + ".dat";
        classfile = target_path +".cls";
    }

    cerr<<"input file name: "<<filename<<"\n";
    cerr<<"ner path: "<<ner_path<<"\n";
    cerr<<"output file name: "<<output<<"\n";


    /* encoding file */
    ClassEncoder classEncoder;
    ClassDecoder classDecoder;
    NerCorpus Nc;


    classEncoder.build(filename);
//    classEncoder.buildNer(Nc);
//    classEncoder.save(classfile);
//    classEncoder.encodefile(filename, encodingfile, false);
//    classDecoder.load(classfile);

    Pattern::classDecoder = &classDecoder;


    auto indexedCorpus = IndexedCorpus(encodingfile);


    if(!ner_path.empty())
    {
        Nc.load(ner_path);
//        classEncoder.buildNer(Nc);
        classEncoder.save(classfile);
        classEncoder.encodefile(filename, encodingfile, false);

        classDecoder.load(classfile);

        auto model = NerModel(&indexedCorpus, &classDecoder);
        model.train_with_ner(encodingfile, modeloptions, Nc, &classDecoder, 1, false);
        if(do_flexgram)
            model.computeflexgrams_fromskipgrams_with_ners();

//        auto model = IndexedPatternModel(&indexedCorpus);
//        model.train_with_ner(encodingfile, modeloptions, Nc, &classEncoder, &classDecoder);
//        if(do_flexgram)
//            model.computeflexgrams_fromskipgrams();


        std::ofstream out(output, std::ios_base::out);
        model.to_csv(out, classDecoder);
        out.close();
    } else{
        classEncoder.save(classfile);
        classEncoder.encodefile(filename, encodingfile, false);
        classDecoder.load(classfile);
        auto model = IndexedPatternModel(&indexedCorpus);
        model.train(encodingfile, modeloptions);
        if(do_flexgram)
            model.computeflexgrams_fromskipgrams();
        std::ofstream out(output, std::ios_base::out);
        model.to_csv(out, classDecoder);
        out.close();
    }

    auto over = std::chrono::high_resolution_clock::now();

    auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(over - start);
    auto duration_minutes = std::chrono::duration_cast<std::chrono::minutes>(over - start);
    cerr << "Total time  "<<duration_seconds.count()<<" seconds  ==  "<<duration_minutes.count()<<" minutes\n";

    return 0;
}


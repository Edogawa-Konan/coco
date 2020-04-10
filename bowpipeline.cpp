//
// Created by mayuan on 2020/3/20.
//

#include "bowmodel.h"
#include "classdecoder.h"
#include "classencoder.h"
#include "cxxopts.hpp"
#include <chrono>



int main(int argc, char** argv)
{

    std::string filename;
    std::string class_file;
    std::string encoding_file;
    std::string output;
    auto modeloptions = PatternModelOptions();

    auto start = std::chrono::high_resolution_clock::now();

    bool debug;
    bool do_skipgram;

    cxxopts::Options options("coco", "Colibri Core tools");

    options.add_options()
            ("d,debug", "Param bar", cxxopts::value<bool >()->default_value("false"))
//            ("ner_path", "Location of ner file (absolute path)", cxxopts::value<std::string>()->default_value(""))
            ("f,file", "Location of corpus file (absolute path)", cxxopts::value<std::string>()->default_value(""))
            ("h,help", "Print usage")
            ("skipgram", "whether extract skipgram? default false", cxxopts::value<bool >()->default_value("false"))
//            ("flexgram", "whether extract flexgram? flexgram base on skipgram, if set true, ignore skipgram option! default false", cxxopts::value<bool >()->default_value("false"))
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
        debug = result["debug"].as<bool>();
        do_skipgram = result["skipgram"].as<bool>();
        output = result["output"].as<std::string>();

        modeloptions.MINTOKENS = result["min_tokens"].as<int>();
        modeloptions.MAXLENGTH = result["max_length"].as<int>();

        modeloptions.DOSKIPGRAMS = do_skipgram;
        modeloptions.DEBUG = debug;

    }catch (cxxopts::OptionParseException& e )
    {
        std::cerr<<e.what()<<std::endl;
        exit(0);
    }

    std::string target_path(filename);

    if(target_path.size() > 4 && *(target_path.end() - 4) == '.')
    {
        encoding_file = target_path.replace(target_path.end()-4, target_path.end(), ".dat");
        class_file = target_path.replace(target_path.end()-4, target_path.end(), ".cls");
    } else{
        encoding_file = target_path + ".dat";
        class_file = target_path +".cls";
    }


    std::cerr<<"input file name: "<<filename<<"\n";
    std::cerr<<"encoding file name: "<<encoding_file<<"\n";
    std::cerr<<"class file name: "<<class_file<<"\n";
    std::cerr<<"output file name: "<<output<<"\n";


    ClassEncoder classEncoder = ClassEncoder();
    classEncoder.build(filename, 0);
    classEncoder.save(class_file);
    classEncoder.encodefile(filename, encoding_file, true);

    ClassDecoder classDecoder = ClassDecoder(class_file);


    auto indexedCorpus = IndexedCorpus(encoding_file, true);

    auto model = BowModel(&indexedCorpus);

    model.compute_bow_from_ngram(encoding_file, modeloptions, &classDecoder);

    model.compute_bow_from_skipgram(modeloptions);

    std::ofstream out(output, std::ios_base::out);
    model.to_csv(out, classDecoder);
    out.close();

    auto over = std::chrono::high_resolution_clock::now();

    auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(over - start);
    auto duration_minutes = std::chrono::duration_cast<std::chrono::minutes>(over - start);
    std::cerr << "Total time  "<<duration_seconds.count()<<" seconds  ==  "<<duration_minutes.count()<<" minutes\n";

    return 0;
}

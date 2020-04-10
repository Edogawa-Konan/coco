//
// Created by mayuan on 2020/2/26.
//

#include "ner.h"
#include <fstream>
#include <cctype>
#include <string>
#include <algorithm>
#include <iostream>

void NerCorpus::load(const std::string &filename, bool debug) {
    std::ifstream In(filename, std::ios::in);
    std::string line;
    int cur = 0;
    unsigned int line_num = 0;
    unsigned int total_line_num = 0;
    std::string ner_token;
    while (getline(In, line)) {
        std::vector<Ner> ners;
        cur = 0;
        try {
            for (int i = 0; i < line.length(); i++) {
                if (line[i] == ' ' || i == line.length() - 1)  // whitespace
                {
                    std::string word;
                    if (line[i] == ' ')
                        word = std::string(line.begin() + cur, line.begin() + i);
                    else
                        word = std::string(line.begin() + cur, line.begin() + i + 1);
                    cur = i + 1;
                    if (word.length() > 0) {
                        if (word[0]=='*') {// this is ner token
                            ner_token = word;
                        } else if (std::isdigit(word[0])) {//this is token:len pair
                            auto index = word.find(':');
                            auto token = std::stoi(std::string(word.begin(), word.begin() + index));
                            auto len = std::stoi(std::string(word.begin() + index + 1, word.end()));
                            ners.emplace_back(ner_token, token, len);
                        }
                    }
                }
            }
        }catch (std::exception &e)
        {
            if(debug)
                std::cerr<<"can't recognize line :  "<<line<<std::endl;
        }
        std::sort(ners.begin(), ners.end());
        this->data.push_back(ners);
        if(!ners.empty())
            line_num++;
        total_line_num++;
    }
    std::cerr<<"Total read "<<total_line_num<<" lines, "<<line_num<<" lines are not empty."<<std::endl;
}

const std::vector<Ner>* NerCorpus::operator[] (unsigned int index)
{// get vector of Ner at index sentence
    if(index >= this->data.size()|| index <0)
    {
        std::cerr<< "Bad index in Nercorpus"<< std::endl;
        return nullptr;
    }
    return &this->data[index];
}


std::ostream & operator <<(std::ostream & os, const Ner& obj)
{
    os<<"["<<obj.raw_string<<" "<<obj.start<<":"<<obj.len<<"]";
    return os;
}
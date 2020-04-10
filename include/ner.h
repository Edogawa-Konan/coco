//
// Created by mayuan on 2020/2/26.
//

#ifndef COCO_NER_H
#define COCO_NER_H

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>



class Ner {
public:
    friend std::ostream & operator <<(std::ostream & os, const Ner& obj);
    std::string raw_string;  // 原始的NER token
    unsigned int start{};  //pattern start token
    unsigned int len{};  // number of tokens

    Ner() = default;

    Ner(std::string raw_string, unsigned int start, unsigned int len): raw_string(std::move(raw_string)), start(start), len(len) {}

    bool operator == (const Ner & other) const{
        return this->start==other.start&&this->len==other.len&&this->raw_string == other.raw_string;
    }
    bool operator != (const Ner & other) const {
        return !(*this==other);
    }
    Ner & operator = (const Ner & other) = default;

    bool operator < (const Ner & other) const {
        if(this->start != other.start)
            return this->start < other.start;
        else if(this->len != other.len)
            return this->len < other.len;
        else
            return this->raw_string < other.raw_string;
    }

};





class NerCorpus {
    public:
    std::vector<std::vector<Ner>> data;

    void load(const std::string& filename, bool debug= false);

    const std::vector<Ner>* operator[] (unsigned int index);


};

#endif //COCO_NER_H

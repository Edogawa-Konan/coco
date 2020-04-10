//
// Created by mayuan on 2020/3/17.
//

#ifndef COCO_BOW_H
#define COCO_BOW_H

#include <vector>
#include <string>
#include <utility>
#include <bitset>
#include <map>
#include <iostream>
#include <functional>
#include "classdecoder.h"

class BagOfWords
{
    public:
        std::vector<unsigned int> bow;
        std::vector<std::string> ners;

        explicit BagOfWords(std::vector<unsigned int> &bow): bow(std::move(bow)){};
        explicit BagOfWords(std::vector<std::string> & ners): ners(std::move(ners)) {};

        BagOfWords() = default;

        bool operator == (const BagOfWords& obj) const;
        bool operator < (const BagOfWords& obj) const;
        bool operator != (const BagOfWords& obj) const;

        bool operator <= (const BagOfWords& obj) const ;

        size_t size() const ;

        BagOfWords &operator = (const BagOfWords& obj);

        void clear();

        friend std::ostream & operator << (std::ostream& os, const BagOfWords & bagOfWords);

        std::string tostring(ClassDecoder &classDecoder) const;
        
};











#endif //COCO_BOW_H

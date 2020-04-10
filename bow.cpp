#include "bow.h"


bool BagOfWords::operator==(const BagOfWords &obj) const {

    return this->bow == obj.bow && this->ners == obj.ners;
}

bool BagOfWords::operator<(const BagOfWords &obj) const {
    if(this->bow != obj.bow)
        return this->bow < obj.bow;
    else
        return this->ners < obj.ners;
}

bool BagOfWords::operator!=(const BagOfWords &obj) const {
    return !(*this == obj);
}

void BagOfWords::clear() {
    this->bow.clear();
    this->ners.clear();
}

BagOfWords &BagOfWords::operator= (const BagOfWords &obj) {
    this->bow = obj.bow;
    this->ners = obj.ners;
    return *this;
}

size_t BagOfWords::size() const {
    return this->ners.size() + this->bow.size();
}

bool BagOfWords::operator<=(const BagOfWords &obj) const {
    return *this < obj || *this == obj;
}

std::ostream & operator << (std::ostream& os, const BagOfWords & bagOfWords )
{
    for(auto v: bagOfWords.bow)
        os<<v<<" ";
    os<<"|";
    for (auto& c: bagOfWords.ners)
        os<<c<<" ";
    return os;
}

std::string BagOfWords::tostring(ClassDecoder &classDecoder) const
{
    std::string repr;
    for (auto &cls: this->bow) {
        repr += (" " + classDecoder[cls]);
    }
//    repr += "]";
    return repr;

}







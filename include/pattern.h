#ifndef COLIBRIPATTERN_H
#define COLIBRIPATTERN_H

/*****************************
* Colibri Core
*   by Maarten van Gompel
*   Centre for Language Studies
*   Radboud University Nijmegen
*
*   http://proycon.github.io/colibri-core
*
*   Licensed under GPLv3
*****************************/

#include <string>
#include <iostream>
#include <ostream>
#include <istream>
#include <unordered_map>
#include <vector>
#include <set>
#include <map>
#include <array>
#include <unordered_set>
#include <iomanip> // contains setprecision()
#include <exception>
#include <algorithm>
#include <limits>
#include "common.h"
#include "classdecoder.h"
#include "ner.h"
#include "bow.h"
//#include "classencoder.h"


/**
 * @file pattern.h
 * \brief Contains the Pattern class that is ubiquitous throughout Colibri Core.
 *
 * @author Maarten van Gompel (proycon) <proycon@anaproy.nl>
 *
 * @section LICENSE
 * Licensed under GPLv3
 *
 * @section DESCRIPTION
 * Contains the Pattern class that is ubiquitous throughout Colibri Core
 *
 */

const int MAINPATTERNBUFFERSIZE = 40960;

/**
 * Pattern categories
 */
enum PatternCategory {
    UNKNOWNPATTERN = 0, /**< not used */
    NGRAM = 1, /**< For ngrams, i.e. patterns without gaps */
    SKIPGRAM = 2, /**< For skipgrams, i.e. patterns with fixed-width gaps */
    FLEXGRAM = 3, /**< For flexgrams, i.e. patterns with dynamic-width gaps */
    SKIPGRAMORFLEXGRAM = 4, /**< Only used as a parameter in querying */
    BOW = 5, /* Bag of words from n-gram*/
    SKIPBOW = 6,

};

enum PatternType {
    PATTERN = 0,
    PATTERNPOINTER = 1,
};

void readanddiscardpattern(std::istream *in, bool pointerformat = false);

int reader_passmarker(const unsigned char c, std::istream *in);


/**
 * \brief Reference to a position in the corpus.
 */
class IndexReference {
public:
    uint32_t sentence;  // start from 1
    uint16_t token;  //start from 0

    IndexReference() {
        sentence = 0;
        token = 0;
    }

    /**
     * Constructor for a reference to a position in the corpus, sentences (or whatever other unit delimits your data) start at 1, tokens start at 0
     */
    IndexReference(uint32_t sentence, uint16_t token) {
        this->sentence = sentence;
        this->token = token;
    }

    IndexReference(std::istream *in) {
        in->read((char *) &sentence, sizeof(uint32_t));
        in->read((char *) &token, sizeof(uint16_t));
    }

    IndexReference(const IndexReference &other) { //copy constructor
        sentence = other.sentence;
        token = other.token;
    };

    void write(std::ostream *out) const {
        out->write((char *) &sentence, sizeof(uint32_t));
        out->write((char *) &token, sizeof(uint16_t));
    }

    bool operator<(const IndexReference &other) const {
        if (sentence < other.sentence) {
            return true;
        } else if (sentence == other.sentence) {
            return (token < other.token);
        } else {
            return false;
        }
    }

    bool operator>(const IndexReference &other) const {
        return other < *this;
    }

    bool operator==(const IndexReference &other) const {
        return ((sentence == other.sentence) && (token == other.token));
    };

    bool operator!=(const IndexReference &other) const {
        return ((sentence != other.sentence) || (token != other.token));
    };

    IndexReference operator+(const int other) const { return IndexReference(sentence, token + other); };

    std::string tostring() const {
        return std::to_string((long long unsigned int) sentence) + ":" + std::to_string((long long unsigned int) token);
    }

    friend std::ostream &operator<<(std::ostream &out, const IndexReference &iref) {
        out << iref.tostring();
        return out;
    }
};

/**
 * \brief Collection of references to position in the corpus (IndexReference).
 * Used by Indexed Pattern models.
 */
class IndexedData {
public:
    std::vector<IndexReference> data;

    IndexedData() {};
//    IndexedData(std::istream * in);
//    void write(std::ostream * out) const;

    bool has(const IndexReference &ref, bool sorted = false) const {
        if (sorted) {
            return std::binary_search(this->begin(), this->end(), ref);
        } else {
            return std::find(this->begin(), this->end(), ref) != this->end();
        }
    }

    /**
     * Returns the number of indices in the collection, i.e. the occurrence count.
     */
    unsigned int count() const { return data.size(); }

    void insert(const IndexReference &ref) { data.push_back(ref); }

    size_t size() const { return data.size(); }

    typedef std::vector<IndexReference>::iterator iterator;
    typedef std::vector<IndexReference>::const_iterator const_iterator;

    iterator begin() { return data.begin(); }

    const_iterator begin() const { return data.begin(); }

    iterator end() { return data.end(); }

    const_iterator end() const { return data.end(); }

    iterator find(const IndexReference &ref) { return std::find(this->begin(), this->end(), ref); }

    const_iterator find(const IndexReference &ref) const { return std::find(this->begin(), this->end(), ref); }

    /**
     * Returns a set of all unique sentences covered by this collection of references.
     */
    std::set<int> sentences() const {
        std::set<int> sentences;
        for (const_iterator iter = this->begin(); iter != this->end(); iter++) {
            const IndexReference ref = *iter;
            sentences.insert(ref.sentence);
        }
        return sentences;
    }

    /**
     * Conversion to std::set<IndexReference>
     */
    std::set<IndexReference> set() const {
        return std::set<IndexReference>(this->begin(), this->end());
    }

    /**
     * Sort the indices, in-place, in proper order of occurence
     */
    void sort() {
        std::sort(this->begin(), this->end());
    }

    void reserve(size_t size) {
        data.reserve(size);
    }

    void shrink_to_fit() {
        data.shrink_to_fit();
    }

};

class BasePattern {
public:


    unsigned char *data{}; /**< This array holds the variable-width byte representation, it is always terminated by \0 (ENDMARKER). Though public, you usually do not want to access it directly */

    /* Store bag of words of this pattern */
    BagOfWords bow;

    /**
     * store Ner object occur in this pattern.
     * start mean the begin of the entity, start from 0, which different from NerCorpus object!!!
     * */
    std::vector<Ner> ners;


    virtual std::string bow2string(ClassDecoder &classDecoder) const;

//    size_t bag_of_word_size() const { return this->bow.size(); }


};


class PatternPointer;



/**
 * \brief Pattern class, represents a pattern (ngram, skipgram or flexgram).
 * Encoded in a memory-saving fashion. Allows numerous operations.
 */
class Pattern : public BasePattern {
public:

    static ClassDecoder * classDecoder;

    const static int patterntype = PATTERN;

    /**
     * Default/empty Pattern constructor. Creates an empty pattern. Special case, allocates no extra data.
     */
    Pattern() { data = nullptr; }

    /**
     * Low-level pattern constructor from character array. The size is in
     * bytes and never includes the end-marker.
     * @param dataref Reference data, must be properly class-encoded
     * @param size The size (without \0 end marker!) to copy from dataref
     */
    Pattern(const unsigned char *dataref, int size);

    /**
     * Slice constructor for Pattern
     * @param ref Reference pattern
     * @param begin Index of the first token to copy (0-indexed)
     * @param length Number of tokens to copy
     */
    Pattern(const Pattern &ref, size_t begin, size_t length, size_t *byteoffset = nullptr,
            bool byteoffset_shiftone = false);

    Pattern(const PatternPointer &ref, size_t begin, size_t length, size_t *byteoffset = nullptr,
            bool byteoffset_shiftone = false);

    /**
     * Copy constructor for Pattern
     * @param ref Reference pattern
     */
    Pattern(const Pattern &ref);

    Pattern(const PatternPointer &ref);

    /**
     * Read Pattern from input stream (in binary form)
     * @param in The input stream
     * @param ignoreeol Ignore end of line markers and read on until the end of the file, storing corpus data in one pattern
     * @param version Version of file format (default: 2)
     * @param corpusoffset not used
     */
    Pattern(std::istream *in, bool ignoreeol = false, const unsigned char version = 2,
            const unsigned char *corpusstart = nullptr, bool debug = false);
    //Pattern(std::istream * in, unsigned char * buffer, int maxbuffersize, bool ignoreeol = false, const unsigned char version = 2, bool debug = false);


    ~Pattern();


    /**
     * Pattern constructor consisting of only a fixed-size gap
     * @param size The size of the gap
     */
    Pattern(int size) {
        //pattern consisting only of fixed skips
        data = new unsigned char[size + 1];
        for (int i = 0; i < size; i++) data[i] = ClassDecoder::skipclass;
        data[size] = ClassDecoder::delimiterclass;
    }

    /**
     * Write Pattern to output stream (in binary form)
     * @param out The output stream
     */
    void write(std::ostream *out, const unsigned char *corpusstart = nullptr) const;

    /**
     * return the size of the pattern in tokens (will count flex gaps gaps as size 1)
     */
    size_t n() const;


    /**
     * return the size of the pattern (in bytes), this does not include the
     * final \0 end-marker.
     */
    size_t bytesize() const;

    /**
     * return the size of the pattern in tokens (will count flex gaps gaps as size 1)
     */
    size_t size() const { return n(); }


    /**
     * return the number of skips in this pattern
     */
    unsigned int skipcount() const;

    /**
     * Returns the category of this pattern (value from enum PatternCategory)
     */
    PatternCategory category() const;


    bool isskipgram() const { return category() == SKIPGRAM; }

    bool isflexgram() const { return category() == FLEXGRAM; }

    bool unknown() const;

    /**
     * Return a single token (not a byte!). index < size().
     */
    Pattern operator[](int index) const { return Pattern(*this, index, 1); }

    /**
     * Compute a hash value for this pattern
     */
    size_t hash() const;

    /**
     * Converts this pattern back into its string representation, using a
     * classdecoder
     */
    std::string tostring(const ClassDecoder &classdecoder) const; //pattern to string (decode)

    std::string torawstring(const ClassDecoder &classdecoder) const;

    /**
     * alias for tostring()
     */
    std::string decode(const ClassDecoder &classdecoder) const { return tostring(classdecoder); } //alias

    /**
     * Debug function outputting the classes in this pattern to stderr
     */
    bool out() const;

    /**
     * Convert the pattern to a vector of integers, where the integers
     * correspond to the token classes.
     */
    std::vector<unsigned int> tovector() const;

    /*Convert the pattern to a vector of integers, where ner indicate as 5*/
    std::vector<unsigned int> tovector_with_ner() const;

    BagOfWords get_bag_of_words() const;


    /*Pattern eaqual without ner information*/
    bool equals(const Pattern &other) const;

//    bool equals(const PatternPointer &other) const;

    bool operator==(const Pattern &other) const;

    bool operator!=(const Pattern &other) const;

    bool operator==(const PatternPointer &other) const;

    bool operator!=(const PatternPointer &other) const;

    /**
     * Assignment operator
     */
    void operator=(const Pattern &other);


    /**
     * Patterns can be sorted, note however that the sorting is based on the
     * frequencies of the tokens and is not alphanumerical!
     */
    bool operator<(const Pattern &other) const;

    bool operator>(const Pattern &other) const;

    Pattern operator+(const Pattern &) const;

    PatternPointer getpointer() const;

    /**
     * Finds the specified subpattern in the this pattern. Returns the index
     * at which it is found, or -1 if it is not found at all.
     */
    int find(const Pattern &subpattern) const;

    /**
     * Test whether the pattern contains the specified subpattern.
     */
    bool contains(const Pattern &subpattern) const; //does the pattern contain the smaller pattern?

    /**
     * Tests whether the pattern is an instantiation of the specified skipgram
     */
    bool instanceof(const PatternPointer &skipgram) const;


    /**
     * Adds all patterns (not just ngrams) of size n that are contained within the pattern to
     * container. Does not extract skipgrams that are not directly present in
     * the pattern.
     */
    int ngrams(std::vector<Pattern> &container, const int n) const;

    int ngrams(std::vector<PatternPointer> &container, const int n) const;

    /**
     * Adds all patterns (not just ngrams) of all sizes that are contained within the pattern to
     * container. Does not extract skipgrams that are not directly present in
     * the pattern. Also returns the full ngram itself by default. Set maxn and minn to constrain.
     */
    int subngrams(std::vector<Pattern> &container, int minn = 1,
                  int maxn = 99) const; //return all subsumed ngrams (variable n)
    int subngrams(std::vector<PatternPointer> &container, int minn = 1,
                  int maxn = 99) const; //return all subsumed ngrams (variable n)

    /**
     * Adds all pairs of all patterns (not just ngrams) of size n that are
     * contained within the pattern, with the token offset at which they were
     * found,  to container.  Does not extract skipgrams that are not directly
     * present in the pattern.
     */
    int ngrams(std::vector<std::pair<Pattern, int>> &container, const int n) const; //return multiple ngrams
    int ngrams(std::vector<std::pair<PatternPointer, int>> &container, const int n) const; //return multiple ngrams

    /**
     * Adds all pairs of all patterns (not just ngrams) that are
     * contained within the pattern, with the token offset at which they were
     * found,  to container.  Does not extract skipgrams that are not directly
     * present in the pattern.
     */
    int subngrams(std::vector<std::pair<Pattern, int>> &container, int minn = 1,
                  int maxn = 9) const; //return all subsumed ngrams (variable n)
    int subngrams(std::vector<std::pair<PatternPointer, int>> &container, int minn = 1,
                  int maxn = 9) const; //return all subsumed ngrams (variable n)

    /**
     * Finds all the parts of a skipgram, parts are the portions that are not skips and adds them to container... Thus 'to be {*} not {*} be' has three parts
     */
    int parts(std::vector<Pattern> &container) const;

    int parts(std::vector<PatternPointer> &container) const;

    /**
     * Finds all the parts of a skipgram, parts are the portions that are not skips and adds them to container as begin,length pairs... Thus 'to be {*} not {*} be' has three parts
     */
    int parts(std::vector<std::pair<int, int> > &container) const;

    /**
     * Finds all the gaps of a skipgram or flexgram., parts are the portions that are not skips and adds them to container as begin,length pairs... Thus 'to be {*} not {*} be' has three parts. The gap-length of a flexgram will always be its minimum length one.
     */
    int gaps(std::vector<std::pair<int, int> > &container) const;


    /**
     * Given a skipgram and an ngram instantation of it (i.e, both of the same length), extract a pattern from the instance that would fill the gaps. Raise an exception if the instance can not be matched with the skipgram
     */
    Pattern extractskipcontent(const Pattern &instance) const;

    /**
     * Replace the tokens from begin (0-indexed), up to the specified length,
     * with a replacement pattern (of any length)
     */
    Pattern replace(int begin, int length, const Pattern &replacement) const;

    /**
     * Replaces a series of tokens with a skip/gap of a particular size.
     * Effectively turns a pattern into a skipgram.
     * @param gap The position and size of the skip/gap: a pair consisting of a begin index (0-indexed) and a length, i.e. the size of the skip
     */
    Pattern addskip(const std::pair<int, int> &gap) const;

    /**
     * Replaces multiple series of tokens with skips/gaps of particular sizes.  Effectively turns a pattern into a skipgram.
     * @param gaps The positions and sizes of the gaps: a vector of pairs, each pair consisting of a begin index (0-indexed) and a length, indicating where to place the gap
     * @return A skipgram
     */
    Pattern addskips(const std::vector<std::pair<int, int> > &gaps) const;

    /**
     * Replaces multiple series of tokens with skips/gaps of undefined variable size.  Effectively turns a pattern into a flexgram.
     * @param gaps The positions and sizes of the gaps: a vector of pairs, each pair consisting of a begin index (0-indexed) and a length, indicating where to place the gap
     * @return A flexgram
     */
    Pattern addflexgaps(const std::vector<std::pair<int, int> > &gaps) const;

    /**
     * Returns a pattern with the tokens in reverse order
     */
    Pattern reverse() const;

    /**
     * converts a skipgram into a flexgram (ngrams just come out unchanged)
     */
    Pattern toflexgram() const;

    /**
     * converts a skipgram into a flexgram (ngrams just come out unchanged), considering ner information
     */
    Pattern toflexgram_with_ner();


    std::vector<bool> ner_mask() const;

    /**
     * Is the word at the specified index (0 indexed) a gap?
     */
    bool isgap(int i) const;


//    //NOT IMPLEMENTED YET:
//
//    Pattern addcontext(const Pattern &leftcontext, const Pattern &rightcontext) const;
//
//    void
//    mask(std::vector<bool> &container) const; //returns a boolean mask of the skipgram (0 = gap(encapsulation) , 1 = skipgram coverage)

    uint32_t getmask() const;

    //sets an entirely new value
    void set(const unsigned char *dataref, const int size);
};



Pattern patternfromfile(const std::string &filename); //helper function to read pattern from file, mostly for Cython

typedef uint64_t PPSizeType;
const size_t PP_MAX_SIZE = std::numeric_limits<PPSizeType>::max();

class PatternPointer : public BasePattern {
public:

    const static int patterntype = PATTERNPOINTER;

    PPSizeType bytes; //number of bytes
    uint32_t mask; //0 == NGRAM
    //first bit high = flexgram, right-aligned, 0 = gap
    //first bit low = skipgram, right-aligned, 0 = gap , max skipgram length 31 tokens


    PatternPointer() {
        data = nullptr;
        bytes = 0;
        mask = 0;
    }

    PatternPointer(unsigned char *dataref, const bsize_t bytesize) {
        data = dataref;
        if (bytesize > PP_MAX_SIZE) {
            std::cerr << "ERROR: Pattern too long for pattern pointer [" << bytesize << " bytes,explicit]" << std::endl;
            throw InternalError();
        }
        bytes = bytesize;
        mask = computemask();
    }

    PatternPointer(const Pattern &ref) {
        data = ref.data;
        const size_t b = ref.bytesize();
        if (b > PP_MAX_SIZE) {
            std::cerr << "ERROR: Pattern too long for pattern pointer [" << b << " bytes,implicit]" << std::endl;
            throw InternalError();
        }
        bytes = b;
        mask = computemask();
        this->ners = ref.ners; // deep copy
    }

    PatternPointer(const Pattern *ref) {
        data = ref->data;
        const size_t b = ref->bytesize();
        if (b > PP_MAX_SIZE) {
            std::cerr << "ERROR: Pattern too long for pattern pointer [" << b << " bytes,implicit]" << std::endl;
            throw InternalError();
        }
        bytes = b;
        mask = computemask();
        this->ners = ref->ners; // deep copy
    }

    PatternPointer(const PatternPointer &ref) {
        data = ref.data;
        bytes = ref.bytes;
        mask = ref.mask;
        this->ners = ref.ners; // deep copy
//        this->bow = ref.bow;
//        std::cerr<<"调用了\n";
    }

    PatternPointer(const PatternPointer *ref) {
        data = ref->data;
        bytes = ref->bytes;
        mask = ref->mask;
        this->ners = ref->ners; // deep copy
    }

    PatternPointer &operator=(const PatternPointer &other) {
        data = other.data;
        bytes = other.bytes;
        mask = other.mask;
        this->ners = other.ners; // deep copy
//        this->bow = other.bow;
        // by convention, always return *this (for chaining)
        return *this;
    }

    PatternPointer(std::istream *in, bool ignoreeol = false, const unsigned char version = 2,
                   unsigned char *corpusstart = nullptr, bool debug = false);

    /**
     * Write Pattern to output stream (in binary form)
     * @param out The output stream
     */
    void write(std::ostream *out, const unsigned char *corpusstart = nullptr) const;

    //slice construtors:
    PatternPointer(unsigned char *, size_t, size_t, size_t *byteoffset = nullptr, bool byteoffset_shiftone = false);

    PatternPointer(const PatternPointer &, size_t, size_t, size_t *byteoffset = nullptr,
                   bool byteoffset_shiftone = false, bool enable_ner = false);

    PatternPointer(const Pattern &, size_t, size_t, size_t *byteoffset = nullptr, bool byteoffset_shiftone = false);


    uint32_t computemask() const;  // compute skipgram/flexgram mask

    uint32_t getmask() const { return mask; }  // get skipgram mask


    size_t n() const; /*pattern tokens*/

    size_t bytesize() const { return bytes; }

    size_t size() const { return n(); }

    /*compute token cls from patternpoint, no delimeter*/
    std::vector<unsigned int> tovector(unsigned int n) const;

    /* compute bag of words */
    void compute_bow(unsigned int n);

    /**
     * Compute a hash value for this pattern
     */
    size_t hash() const;

    PatternCategory category() const;

    bool isskipgram() const { return category() == SKIPGRAM; }

    bool isflexgram() const { return category() == FLEXGRAM; }

    bool unknown() const;

    std::string tostringraw(const ClassDecoder &classdecoder) const; // patternpointer to string withour ner

    std::string tostring(const ClassDecoder &classdecoder) const; //pattern to string (decode)

    std::string decode(const ClassDecoder &classdecoder) const {
        return tostring(classdecoder);
    } //pattern to string (decode)

    bool out() const;

    bool operator==(const PatternPointer &other) const;

    bool operator!=(const PatternPointer &other) const { return !(*this == other); }

    bool operator==(const Pattern &other) const;

    bool equals(const Pattern &other) const;  // check equals without NER

    bool equals(const PatternPointer &other) const;  // check equals without NER

    bool operator!=(const Pattern &other) const { return !(*this == other); }

    /**
     * Return a single token (not a byte!). index < size().
     */
    PatternPointer operator[](int index) const { return PatternPointer(*this, index, 1); }

    PatternPointer toflexgram() const;

    PatternPointer toflexgram_with_ner() const;

    bool isgap(unsigned int i) const;


    /**
     * Return a new patternpointer one token to the right, maintaining the same token length and same skip configuration (if any).
     * Note that this will cause segmentation faults if the new PatternPointer exceeds the original data!!!
     * It's up to the caller to check this!
     */
    PatternPointer &operator++();

    bool operator<(const PatternPointer &other) const {
        if (data == other.data) {
            if (bytes == other.bytes) {
                return mask < other.mask;
            } else {
                return bytes < other.bytes;
            }
        } else {
            return data < other.data;
        }
    }

    int ngrams(std::vector<PatternPointer> &container, int n) const;

    int subngrams(std::vector<PatternPointer> &container, int minn = 1,
                  int maxn = 9) const; //return all subsumed ngrams (variable n)
    int ngrams(std::vector<std::pair<PatternPointer, int>> &container, int n) const; //return multiple ngrams


    int bow_ngrams(std::vector<PatternPointer> &container, int n) const;

    int bow_ngrams(std::vector<std::pair<PatternPointer, int>> &container, int n) const;  // compute bag of words ngram

    int ngrams(std::vector<std::pair<PatternPointer, int>> &container, int n, const std::vector<Ner> *ners_) const;

//    int ngrams_with_ner(std::vector<std::pair<PatternPointer, int>> &container, int n,
//                        const std::vector<Ner> *ners_, ClassEncoder *classEncoder,
//                        ClassDecoder *classDecoder) const;

    int ngrams(std::vector<PatternPointer> &container, int n, const std::vector<Ner> *ners_) const;


    static std::vector<PatternPointer>
    bruteforce(const std::vector<Ner> &pattern_has_ners, const PatternPointer &no_ner);


    int subngrams(std::vector<std::pair<PatternPointer, int>> &container, int minn = 1,
                  int maxn = 9) const; //return all subsumed ngrams (variable n)


    int bow_subgrams(std::vector<std::pair<PatternPointer, int>> &container, int minn = 1,
                     int maxn = 9) const;

    int subngrams(std::vector<std::pair<PatternPointer, int>> &container, int minn,
                  int maxn, const std::vector<Ner> *ners) const; //return all subsumed ngrams (variable n)

    /**
     * Finds all the parts of a skipgram, parts are the portions that are not skips and adds them to container... Thus 'to be {*} not {*} be' has three parts
     */
    int parts(std::vector<PatternPointer> &container) const;

    /**
     * Finds all the parts of a skipgram, parts are the portions that are not skips and adds them to container as begin,length pairs... Thus 'to be {*} not {*} be' has three parts
     */
    int parts(std::vector<std::pair<int, int> > &container) const;

    /**
     * Finds all the gaps of a skipgram or flexgram., parts are the portions that are not skips and adds them to container as begin,length pairs... Thus 'to be {*} not {*} be' has three parts. The gap-length of a flexgram will always be its minimum length one.
     */
    int gaps(std::vector<std::pair<int, int> > &container) const;

    /**
     * return the number of skips in this pattern
     */
    unsigned int skipcount() const;

    /**
     * Replaces a series of tokens with a skip/gap of a particular size.
     * Effectively turns a pattern into a skipgram.
     * @param gap The position and size of the skip/gap: a pair consisting of a begin index (0-indexed) and a length, i.e. the size of the skip
     */
    PatternPointer addskip(const std::pair<int, int> &gap) const;

    /**
     * Replaces multiple series of tokens with skips/gaps of particular sizes.  Effectively turns a pattern into a skipgram.
     * @param gaps The positions and sizes of the gaps: a vector of pairs, each pair consisting of a begin index (0-indexed) and a length, indicating where to place the gap
     * @return A skipgram
     */
    PatternPointer addskips(const std::vector<std::pair<int, int> > &gaps) const;

    /**
     * Low-level function for flexgrams, that returns a collapsed comparable representation of the flexgram in collapseddata (has to be pre-allocated). Return value is the number of bytes of the representation. In the collapsed representation adjacent flexgrams are removed.
     */
    int flexcollapse(unsigned char *collapseddata) const;

    bool instanceof(const PatternPointer &skipgram) const;

//     operator Pattern() { return Pattern(*this); } //cast overload
    Pattern pattern() const { return Pattern(*this); } //cast overload

    /**
     * Deep copy for this PatternPointer object, used in NER
     * @return
     */
    PatternPointer copy() const;
};


static const unsigned char *tmp_unk = (const unsigned char *) "\2";
static const unsigned char *tmp_boundarymarker = (const unsigned char *) "\1";
static const unsigned char *tmp_skipmarker = (const unsigned char *) "\3";
static const unsigned char *tmp_flexmarker = (const unsigned char *) "\4";
const Pattern BOUNDARYPATTERN = Pattern((const unsigned char *) tmp_boundarymarker, 1);
const Pattern SKIPPATTERN = Pattern((const unsigned char *) tmp_skipmarker, 1);
const Pattern FLEXPATTERN = Pattern((const unsigned char *) tmp_flexmarker, 1);
static const Pattern UNKPATTERN = Pattern((const unsigned char *) tmp_unk, 1);

namespace std {

    template<>
    struct hash<Pattern> {
    public:
        size_t operator()(const Pattern &pattern) const throw() {
            return pattern.hash();
        }
    };


    template<>
    struct hash<const Pattern *> {
    public:
        size_t operator()(const Pattern *pattern) const throw() {
            return pattern->hash();
        }
    };

    template<>
    struct hash<PatternPointer> {
    public:
        size_t operator()(const PatternPointer &pattern) const throw() {
            return pattern.hash();
        }
    };


    template<>
    struct hash<const PatternPointer *> {
    public:
        size_t operator()(const PatternPointer *pattern) const throw() {
            return pattern->hash();
        }
    };

}

#endif

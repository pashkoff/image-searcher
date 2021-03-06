#ifndef CC_INVERTEDFILE
#define CC_INVERTEDFILE


#include <exception>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <stdint.h>

using namespace std;


typedef unsigned int wordtype;
typedef vector<wordtype> wordvec;
typedef vector<wordvec> docvec;

//typedef unsigned int uint;
//typedef size_t uint;
//typedef unsigned int id_t;
typedef uint32_t uint;
typedef unsigned long ulong;

#define EPS 1e-10


//------------------------------------------------------------------------
//Inverted file Word Document entry i.e. entry for a given word containing 
//counts in different documents
class ivWordDoc
{
public:
	//number of time this word appears
	size_t count;
	//weighted value
	float val;
	//document id
	size_t doc;

	//constructor
	ivWordDoc() :
		count(0),
		val(0),
		doc(0)
	{
	}

	//less than operator for ordering
	bool operator< (const ivWordDoc& j) const
	{
		return (this->doc < j.doc);
	}

	//stream overloads
	friend ostream& operator<<(ostream& os, ivWordDoc const& ivworddoc);
	friend istream& operator>>(istream& is, ivWordDoc& ivworddoc);  
};

//list of ivWordDoc
typedef vector<ivWordDoc> ivWordDocList;
typedef vector<ivWordDoc>::iterator ivWordDocIt;
typedef vector<ivWordDoc>::const_iterator ivWordDocCIt;

//------------------------------------------------------------------------
// inverted file Word entry
class ivWord
{
public:
	//number of documents that have this word
	size_t ndocs;
	//number of times this word appears in the database
	size_t wf;
	//documents entries
	ivWordDocList docs;

	//constructor
	ivWord() :
		ndocs(0),
		wf(0)
	{
		docs.resize(0);
	}

	//destructor
	~ivWord()
	{
		docs.clear();
	}

	//stream overloads
	friend ostream& operator<<(ostream& os, ivWord const& ivword);
	friend istream& operator>>(istream& is, ivWord& ivword);  
};


//------------------------------------------------------------------------
typedef vector<uint> ivDocWordList;
typedef ivDocWordList::iterator ivDocWordIt;

//Inverted file Document entry having information about documents
class ivDoc
{
public:
	//number of unique words in the document
	float norml0;
	//number of words in this document
	float norml1;
	//square sum of words in the document
	float norml2;
	//number of tokens
	size_t ntokens;
	//number of unique words
	size_t nwords;

	//list of word indices
	ivDocWordList words;

	//constructor
	ivDoc() :
		norml0(0),
		norml1(0),
		norml2(0),
		ntokens(0),
		nwords(0)
	{
		words.resize(0);
	}

	//destructor
	~ivDoc()
	{
		words.clear();
	}

	//stream overloads
	friend ostream& operator<<(ostream& os, ivDoc& ivdoc);
	friend istream& operator>>(istream& is, ivDoc& ivdoc);
};

//------------------------------------------------------------------------
//class to hold and id and a value
class ivNode
{
public:
	float val;
	size_t id;

	//constructor
	ivNode() :
		val(0),
		id(0)
	{
	}

	//operator < for comparison by id
	bool operator< (const ivNode& d) const
	{ return (this->id < d.id);  }  

};
//list of ivDocScore
typedef vector<ivNode>        ivNodeList;
typedef ivNodeList::iterator  ivNodeIt;

//list of lists
typedef vector<ivNodeList>     ivNodeLists;
typedef ivNodeLists::iterator  ivNodeListIt;

//class to compare ivNode by val
class ivNodeCmpValAsc
{ 
public:
	bool operator() (const ivNode& i,const ivNode& j) { return (i.val<j.val);} 
}; 

class ivNodeCmpValDesc
{ 
public:
	bool operator() (const ivNode& i,const ivNode& j) { return (i.val>j.val);} 
}; 


//------------------------------------------------------------------------
//Inverted file structure

typedef vector<ivDoc> ivDocList;
typedef ivDocList::iterator ivDocIt;
typedef ivDocList::const_iterator ivDocCIt;

typedef vector<ivWord> ivWordList;
typedef ivWordList::iterator ivWordIt;

class ivFile
{
public:

	//enumerators for normalization
	enum Norm
	{
		NORM_NONE,  // no normalization
		NORM_L0,    // normalize by the number of words
		NORM_L1,    // normalize so that the histogram sum = 1
		NORM_L2,    // normalize so that histogram sum square = 1 
		NORM_LAST
	};

	//enumerators for distance measure
	enum Dist
	{
		DIST_L1,      // l1 distance
		DIST_L2,      // l2 distance
		DIST_HAM,     // hamming distance
		DIST_KL,      // 
		DIST_COS,     // cos distance (dot product)
		DIST_JAC,     // Jacquard distance
		DIST_HISTINT, // Histogram Intersection 
		DIST_LAST
	};

	//enumerators for weight to use
	enum Weight
	{
		WEIGHT_NONE,    // don't weight counts values 
		WEIGHT_BIN,     // binarize the counts 
		WEIGHT_TF,      // term frequency, divide each count by the number of words in the doc  
		WEIGHT_TFIDF,   // use tf-idf weighting
		WEIGHT_LAST
	};

	struct Params
	{
		Params(Norm norm = NORM_L1, Weight weight = WEIGHT_NONE) : 
			norm(norm),
			weight(weight)
		{
			check(); 
		}

		Norm norm;
		Weight weight;

		void check() throw(std::logic_error);

		//stream overloads
		friend istream& operator>>(istream& is, Params& pr);
		friend ostream& operator<<(ostream& os, Params const& pr);
	};

	//constructor
	ivFile(ivFile::Params params = ivFile::Params()) :
		nwords(0),
		ndocs(0),
		params(params)
	{ 
		params.check();
	}

	//desctructor
	~ivFile()
	{
		//clear memory
		this->clear();
	}

	//write to file
	void save(string const & filename) const;

	//load from file
	void load(string const & filename);

	void display(bool show_docs = true, bool show_words = false);

	//clears the memory
	void clear();

	//compute stats: document norms, weights, ... to prepare for search
	void computeStats();

	//fill the inverted file with input counts
	//
	// wlabel   - word labels for every token, 1->nwords
	// dlabel   - doc labels for every token. 1->ndocs
	// ntokens  - number of tokens (length of wlabel and dlabel)
	// nwords   - number of words
	// ndocs    - number of docs
	//
	void fill(wordtype* wlabel, wordtype* dlabel, size_t ntokens, 
		size_t nwords, size_t ndocs);

	//fill the inverted file with input counts
	//
	// data     - the input data, with one data vector per input consisting of 
	//            all the word labels for its tokens
	// nwords   - the total number of words
	// idshift  - a value to add to document all ids (useful for adding more data)
	void fill(docvec const & data, uint nwords, uint idshift=0);

	// search the inverted file for the closest document
	//
	// data     - the input data, with one vector per iput
	// weight   - weight to use for individual counts (binary, term-freq, ..)
	// norm     - normalization of weights (L1, L2, ...)
	// dist     - distance measure to use (L1, L2, ...)
	// overlapOnly - return only those documents with overlapping words
	// k        - no. of output required per document, if 0 then return everything
	// scorelists   - a list of ivNOdeList to hold the results
	//
	void search(docvec const & data, 
		ivFile::Dist dist,
		bool overlapOnly, uint k, ivNodeLists& scorelists, bool verbose) const;

private:
	// search the inverted file for the closest document
	//
	// wlabel   - word labels for every token, 1->nwords
	// ntokens  - number of tokens (length of wlabel)
	// weight   - weight to use for individual counts (binary, term-freq, ..)
	// norm     - normalization of weights (L1, L2, ...)
	// dist     - distance measure to use (L1, L2, ...)
	// overlapOnly - return only those documents with overlapping words
	/// k       - no. of outputs required, if 0 then return everything
	// scorelist - a list of ivNode to hold the results
	//
	void search(const wordtype* wlabel, uint ntokens, 
		ivFile::Weight weight, ivFile::Norm norm, ivFile::Dist dist,
		bool overlapOnly, uint k, ivNodeList& scorelist) const;


private:
		
	//weight a document value
	inline float weightVal(float val, ivWord const & word, ivDoc const& doc, ivFile::Weight wt) const;

	//normalize a document value
	inline float normVal(float val, ivDoc& doc, ivFile::Norm norm) const;

	//compute distance
	inline float dist(float val1, float val2, ivFile::Dist dist) const;

	//convert form a distance to a norm
	inline float dist2Norm(ivDoc const & doc, ivFile::Dist dist, ivFile::Norm norm) const;

private:
	//array of word entries
	size_t nwords;
	ivWordList words;

	//array of document entries
	size_t ndocs;
	ivDocList docs;

	Params params;

public:

	//stream overloads
	friend istream& operator>>(istream& is, ivFile& ivf);
	friend ostream& operator<<(ostream& os, ivFile const& ivf);
};


#endif

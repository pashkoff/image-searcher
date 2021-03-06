#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <fstream>
#include <string>
#include <iterator>

#include "ccInvertedFile.hpp"

#include "Util/util.hpp"

#include "vc_fix.hpp"


//------------------------------------------------------------------------
void ivFile::fill(wordtype* wlabel, wordtype* dlabel, size_t ntokens, 
	size_t nwords, size_t ndocs)
{
	//allocate vectors
	words.resize(nwords);
	docs.resize(ndocs);

	//now loop on the tokens and fill in the hdata
	for (size_t i = 0; i < ntokens; ++i)
	{
		//get the labels
		if (wlabel[i] == 0 || (size_t)wlabel[i]>nwords || 
			dlabel[i]==0 || (size_t)dlabel[i]>ndocs)
		{
			continue;
		}

		uint wl = (uint)wlabel[i]-1;
		uint dl = (uint)dlabel[i]-1;

		//get word
		ivWord* w = &words[wl];        

		//increment word count
		w->wf++;    

		//make a new wordDoc
		ivWordDoc newdoc;
		newdoc.doc = dl;

		//find position of this document id in the list
		ivWordDocIt wdit = lower_bound (w->docs.begin(), w->docs.end(), newdoc); 

		//check if not found, then add it in the correct place
		if (wdit == w->docs.end() || newdoc<(*wdit))
		{
			wdit = w->docs.insert(wdit, newdoc);
			w->ndocs++;
		}

		//increment word count for this word's doc
		wdit->count++;

		//increment document count of tokens
		docs[dl].ntokens++;
	}

	//put sizes
	this->nwords = nwords;
	this->ndocs = ndocs;
}

//fill the inverted file with input counts //
// data     - the input data, with one data vector per input consisting of 
//            all the word labels for its tokens
// nwords   - the total number of words
void ivFile::fill(docvec const& data, uint nwords, uint idshift)
{
	cout << __FUNCTION__ << endl;
	//number of documents
	size_t ndocs = data.size();

	//allocate vectors
	words.resize(nwords);
	docs.resize(ndocs);

	//loop on the documents and fill in
	for (size_t d = 0; d < ndocs; ++d)
	{
		//cout << "doc:" << d << endl;
		//get the document data
		const wordvec& doc = data[d];
		//document label
		uint dl = d + idshift;

		//update ntokens for this document
		docs[dl].ntokens += doc.size();

		//loop and update
		for (size_t t=0; t < doc.size(); ++t)
		{
			//word label
			if ((uint)doc[t] > nwords)
				continue;

			if (doc[t] == 0)
				continue;

			uint wl = (uint)(doc[t]-1);

			//get word
			ivWord* w = &words[wl];        

			//increment word count
			w->wf++;    

			//make a new wordDoc
			ivWordDoc newdoc;
			newdoc.doc = dl;

			//find position of this document id in the list
			//cout << "wordid " << wl << " size" <<  w->docs.size() << endl;
			ivWordDocIt wdit = lower_bound (w->docs.begin(), w->docs.end(), newdoc); 

			//check if not found, then add it in the correct place
			if (wdit == w->docs.end() || newdoc<(*wdit))
			{
				wdit = w->docs.insert(wdit, newdoc);
				w->ndocs++;
			}

			//increment word count for this word's doc
			wdit->count++;
		}
	}

	//put sizes
	this->nwords = nwords;
	this->ndocs = ndocs;  
}


//------------------------------------------------------------------------
void ivFile::computeStats()
{
	Weight wt = params.weight;
	Norm norm = params.norm;

	cout << __FUNCTION__ << "wt " << wt << " norm " << norm << endl;
	uint i;

	//reset documents
	for (i=0; i<this->ndocs; ++i)
	{
		this->docs[i].norml0 = 0;
		this->docs[i].norml1 = 0;
		this->docs[i].norml2 = 0;
		this->docs[i].nwords = 0;
		this->docs[i].words.clear();
	}

	//now loop and compute word frequency for every document and norms
	for (i=0; i<this->nwords; ++i)
	{
		//get the word
		ivWord* w = &this->words[i];

		//get number of documents for this word
		w->ndocs = w->docs.size();

		//loop on its documents
		ivWordDocIt wdit;
		for (wdit = w->docs.begin(); wdit != w->docs.end(); wdit++)
		{
			//get the word document id
			uint doc = wdit->doc;

			//get the value and weight it
			wdit->val = this->weightVal(wdit->count, *w, this->docs[doc], wt);

			//update norms
			this->docs[doc].norml0 += 1;
			this->docs[doc].norml1 += wdit->val;
			this->docs[doc].norml2 += (wdit->val * wdit->val);

			//add this word to list of words of this document
			this->docs[doc].words.push_back(i);
			this->docs[doc].nwords++;
		}
	}

	//now normalize vals
	for (i=0; i<this->nwords; ++i)
	{
		//get the word
		ivWord* w = &this->words[i];

		//loop on its documents
		for (ivWordDocIt wdit = w->docs.begin(), wditend = w->docs.end(); 
			wdit != wditend; wdit++)
			//normalize
			wdit->val = this->normVal(wdit->val, this->docs[wdit->doc], norm);
	}

	//cout << "computeStats done" << endl;
	//cout << "ndocs = " << ndocs << endl;
	//cout << "nwords = " << nwords << endl;
	//cout << "Words: " << endl;
	//for (i = 0; i < nwords; ++i)
	//{
	//	cout << words[i] << endl;
	//}

	//  for (i=0; i<this->docs[5].nwords; ++i)
	//  {
	//    ivWordDocIt wdit;
	//    for (wdit=this->words[(uint)this->docs[5].words[i]].docs.begin();
	//      wdit!=this->words[(uint)this->docs[5].words[i]].docs.end(); wdit++)
	//    {
	//      if (wdit->doc!=5)
	//        continue;
	//      
	//      cout << wdit->val << " ";
	//    }
	//  }
}

//------------------------------------------------------------------------
float ivFile::weightVal(float val, ivWord const& word, ivDoc const& doc, ivFile::Weight wt) const
{
	switch(wt)
	{
	case WEIGHT_BIN:
		val = val>0 ? 1 : 0;
		break;
	case WEIGHT_TF:
		val /= doc.ntokens ? doc.ntokens : 1;
		break;
	case WEIGHT_TFIDF:
		val /= doc.ntokens ? doc.ntokens : 1;
		val *= log2(this->ndocs / (word.ndocs + EPS));
		break;
	default:
		break;
	}
	return val;
}

//------------------------------------------------------------------------
float ivFile::normVal(float val, ivDoc& doc, ivFile::Norm norm) const
{
	switch(norm)
	{
	case NORM_L0:
		val /= (EPS + doc.norml0);
		break;
	case NORM_L1:
		val /= (EPS + doc.norml1);
		break;
	case NORM_L2:
		val /= (EPS + sqrt(doc.norml2));
		break;
	default:
		break;
	}
	return val;
}

//------------------------------------------------------------------------
float ivFile::dist(float val1, float val2, ivFile::Dist dist) const
{
	switch(dist)
	{
	case DIST_L1:
		val1 -= val2;
		val1 = val1>0 ? val1 : -val1;
		break;

	case DIST_L2:
		val1 -= val2;
		val1 *= val1;
		break;

	case DIST_HAM:
		val1 = (float) ((uint)val1 ^ (uint)val2);
		break;

	case DIST_KL:
		break;

	case DIST_COS:
		val1 *= val2;
		break;      

		//there is intersection, return 1 if neither is 0
	case DIST_JAC:
		val1 = (val1==0 || val2==0) ? 0 : 1;
		break;

		//histogram intersection: return the minimum
	case DIST_HISTINT:
		val1 = (val1 <= val2) ? val1 : val2;
		break;
	}
	return val1;
}

//------------------------------------------------------------------------
float ivFile::dist2Norm(ivDoc const & doc, ivFile::Dist dist, ivFile::Norm norm) const
{
	float ret = 0;

	//if not normalizing, then return the actual norm
	if (norm == NORM_NONE)
	{
		switch(dist)
		{
		case DIST_L1:
			ret = doc.norml1;
			break;
		case DIST_L2:
			ret = doc.norml2;
			break;
		case DIST_HAM:
			ret = doc.norml0;
			break;
		case DIST_KL:
			break;
		case DIST_COS:
			ret = 0;
			break;      
			//return 0: which is the value with no intersection
		case DIST_JAC:
			ret = 0;
			break;
			//return 0: which is the value with no intersection
		case DIST_HISTINT:
			ret = 0;
			break;
		}  
	}
	//otherwise the norm is 1
	else
	{
		switch (dist)
		{
		case DIST_COS:
			ret = 0;
			break;
			//return 0: which is the value with no intersection
		case DIST_JAC:
			ret = 0;
			break;
			//return 0: which is the value with no intersection
		case DIST_HISTINT:
			ret = 0;
			break;
		default:
			ret = 1;
			break;
		}
	}

	return ret;
}
//------------------------------------------------------------------------

void ivFile::search(const wordtype* wlabel, uint ntokens, 
	ivFile::Weight weight, ivFile::Norm norm, ivFile::Dist dist,
	bool overlapOnly, uint k, ivNodeList& scorelist) const
	//        float* scores, uint* docs, uint& nret)
{
	cerr << __FUNCTION__ << endl;

	//return if empty
	if (words.empty()) return;

	uint i;
	//allocate array for storing scores for all documents
	//  ivNodeList scorelist;

	//compute counts for words first
	ivNodeList wordcount;
	//  ivNodeIt ni;
	for (i=0; i<ntokens; ++i)
	{
		//skip word if invalid
		uint wr = (uint) wlabel[i];
		if (wr==0 || wr>nwords) continue;
		//get that word
		ivNode w; 
		w.id = wr - 1;
		ivNodeIt wt = lower_bound(wordcount.begin(), wordcount.end(), w);
		if (wt==wordcount.end() || w<(*wt))	wt = wordcount.insert(wt, w);

		//increment
		wt->val += 1;
	}
	//this doc stats: norms, weights, ...
	ivDoc doc;
	doc.ntokens = ntokens;
	doc.nwords = wordcount.size();  
	for (ivNodeIt ni=wordcount.begin(), niend = wordcount.end(); ni!=niend; ni++)
	{
		//weight the value
		ni->val = weightVal(ni->val, words[(uint)ni->id], doc, weight);
		//compute norms
		doc.norml0++;
		doc.norml1 += ni->val;
		doc.norml2 += (ni->val * ni->val);    
	}
	//  cout << "Doc: nwords=" << doc.nwords << " ntokens=" << doc.ntokens <<
	//          " norml0=" << doc.norml0 << " norml1=" << doc.norml1 << 
	//          " norml2=" << doc.norml2 << "\n";
	//  cout << "Doc6: nwords=" << docs[5].nwords << " ntokens=" << docs[5].ntokens <<
	//          " norml0=" << docs[5].norml0 << " norml1=" << docs[5].norml1 << 
	//          " norml2=" << docs[5].norml2 << "\n";

	//get this documents's norm
	float docNorm = dist2Norm(doc, dist, norm);

	//if computing distance to all documents, update score list with this word's
	// norm. The initial one has twice the norm of every document
	if (!overlapOnly && norm==ivFile::NORM_NONE)
	{
		//loop on the documents and add to score list
		for (ivNodeIt dsit = scorelist.begin(), dsend = scorelist.end(); dsit!=dsend; dsit++)
			//update norm to include this docs norm
			dsit->val = (dsit->val / 2) +  docNorm;      
	}

	//now loop on the document words and update scores  
	for (ivNodeIt wit=wordcount.begin(), witend = wordcount.end(); wit!=witend; wit++)
	{    
		//get the word    
		ivWord const& word = words[(uint)wit->id];
		//    cout << "\nword: " << wit->id << "\n";

		//normalize this doc val
		wit->val = normVal(wit->val, doc, norm);
		//    cout << words.size() << endl;

		//loop on the documents
		for (ivWordDocCIt wdit = word.docs.begin(), wditend = word.docs.end(); 
			wdit != wditend; wdit++)
		{
			//      cout << " doc: "<< wdit->doc+1;
			//get that document in the score list
			ivNodeIt dsit; 
			//if not overlap or scorelist has equal length to docs
			if (!overlapOnly || scorelist.size()==docs.size())
			{
				dsit = scorelist.begin() + wdit->doc;
			}
			//if overlap only, then search for that document in the score list and 
			//insert if not already there
			else
			{
				ivNode ds; 
				ds.id = wdit->doc;
				dsit = lower_bound(scorelist.begin(), scorelist.end(), ds);

				//check if to insert to the list of scores (new doc)
				if (dsit==scorelist.end() || ds<(*dsit))
				{
					//init with sum of norms
					ds.val = docNorm + dist2Norm(docs[ds.id], dist, norm);
					//insert
					dsit = scorelist.insert(dsit, ds);
				}
			}

			//      if (dsit->id == 5) //(wdit->doc == 5)
			//        cout << "1-> " << wit->val << " 2->" << wdit->val << "\n";               
			//compute distance
			dsit->val -= this->dist(wit->val,0,dist) + this->dist(wdit->val,0,dist);
			dsit->val += this->dist(wit->val, wdit->val, dist);
		}
	}  

	//further processing
	if (dist == ivFile::DIST_JAC)
	{
		//we already have the intersections, so update to compute the Jac distance
		// dist = 1 - intersection / union
		// where intersection = dsit->val
		// and union = nwords1 + nwords2 - intersection
		for (ivNodeIt dsit=scorelist.begin(), dsitEnd=scorelist.end(); dsit!=dsitEnd; dsit++)
			dsit->val = 1 - dsit->val / (docs[dsit->id].norml0 + doc.norml0 - dsit->val);
	}
	else if (dist == ivFile::DIST_COS)
	{
		//update cosine distance by changing the range from -1 -> 1 
		// (least similar -> most similar)
		// to 0 -> 2 (most similar -> least similar)
		// where newval = -oldval + 1
		for (ivNodeIt dsit=scorelist.begin(), dsitEnd=scorelist.end(); dsit!=dsitEnd; dsit++)
			dsit->val = 1 -dsit->val;
	}
	else if (dist == ivFile::DIST_HISTINT)
	{
		//update histogram intersection kernel by inverting and adding 1 
		//(assuming normalization)
		for (ivNodeIt dsit=scorelist.begin(), dsitEnd=scorelist.end(); dsit!=dsitEnd; dsit++)
			dsit->val = 1 - dsit->val;
	}

	//sort ascendingly
	ivNodeCmpValAsc ncv;
	sort(scorelist.begin(), scorelist.end(), ncv);

	//keep only k
	if (k>0 && k<scorelist.size())
		scorelist.resize(k);

	//clean memory
	wordcount.clear();  
}

//------------------------------------------------------------------------
void ivFile::search(docvec const & data, 
	ivFile::Dist dist,
	bool overlapOnly, uint k, ivNodeLists& scorelists, bool verbose=false) const
{
	Weight weight = params.weight;
	Norm norm     = params.norm;

	cerr << __FUNCTION__ << "weight " << weight << " norm " << norm << " dist " << dist << endl;
	//number of documents
	uint ndocs = data.size();

	//resize the scorelists  
	scorelists.resize(ndocs);
	//   cout << "we have " << ndocs << " docs" << endl;

	//initialize score list with full list of documents, and just copy it 
	//afterwords. Init it with twice the norm
	ivNodeList tscorelist;
	if (!overlapOnly)
	{
		tscorelist.resize(docs.size());
		//loop on the documents and add to score list
		ivDocCIt dit = docs.begin(), dend = docs.end();
		ivNodeIt dsit = tscorelist.begin();
		for (uint i=0; dit!=dend; dit++, dsit++, i++)
		{
			//put the document id
			dsit->id = i;
			//put the initial distance: sum of norms
			dsit->val = 2 * dist2Norm(*dit, dist, norm);
		}  
	}

	//now loop
	for (uint d=0; d<ndocs; d++)
	{
		if (verbose && (d==0/* ||  (d % (uint)round(0.1 * ndocs) == 0)*/) )
		{
			cout << " doc:" << d << " / " << ndocs << endl;
			cout.flush();
		}
		//get data for this document
		const wordvec& doc = data[d];

		//make a copy of tscorelist
		ivNodeList ttscorelist = tscorelist;
		//search for this document
		search(&(*doc.begin()), doc.size(), weight, norm, dist, overlapOnly, 
			k, ttscorelist);
		//put back into scorelists
		scorelists[d] = ttscorelist;
		//clear
		ttscorelist.clear();
	}
}

//------------------------------------------------------------------------
void ivFile::clear()
{
	//clears the docs array
	this->docs.clear();

	//clear words array
	this->words.clear();
}

//------------------------------------------------------------------------
//Stream overloads for ivWord
ostream& operator<<(ostream& os, ivWord const& ivw)
{
	os.write((char*)&ivw.ndocs, sizeof(ivw.ndocs));
	os.write((char*)&ivw.wf, sizeof(ivw.wf));
	return os;
}
istream& operator>>(istream& is, ivWord& ivw)
{
	is.read((char*)&ivw.ndocs, sizeof(ivw.ndocs));
	is.read((char*)&ivw.wf, sizeof(ivw.wf));
	return is;
}

//------------------------------------------------------------------------
//Stream overloads for ivWordDoc
ostream& operator<<(ostream& os, ivWordDoc const& ivwd)
{
	os.write((char*)&ivwd.count, sizeof(ivwd.count));
	os.write((char*)&ivwd.doc, sizeof(ivwd.doc));
	os.write((char*)&ivwd.val, sizeof(ivwd.val));
	return os;
}
istream& operator>>(istream& is, ivWordDoc& ivwd)
{
	is.read((char*)&ivwd.count, sizeof(ivwd.count));
	is.read((char*)&ivwd.doc, sizeof(ivwd.doc));
	is.read((char*)&ivwd.val, sizeof(ivwd.val));
	return is;
}

//------------------------------------------------------------------------
//Stream overloads for ivDoc
ostream& operator<<(ostream& os, ivDoc const& ivdoc)
{
	os.write((char*)&ivdoc.ntokens, sizeof(ivdoc.ntokens));
	//  os.write((char*)&ivdoc.nwords, sizeof(ivdoc.nwords));
	return os;
}
istream& operator>>(istream& is, ivDoc& ivdoc)
{
	is.read((char*)&ivdoc.ntokens, sizeof(ivdoc.ntokens));
	//  is.read((char*)&ivdoc.nwords, sizeof(ivdoc.nwords));
	return is;
}

//------------------------------------------------------------------------
//Stream overloads for ivFile
ostream& operator<<(ostream& os, ivFile const& ivf)
{
	uint i;

	os << ivf.params;

	//save the document array
	//
	//put the size
	os.write((const char*)&ivf.ndocs, sizeof(ivf.ndocs));
	//put the array
	for (i=0; i<ivf.ndocs; ++i)
		os << ivf.docs[i];

	//save the words array
	//
	//put the size
	os.write((const char*)&ivf.nwords, sizeof(ivf.nwords));
	//put the array
	for (i=0; i<ivf.nwords; ++i)
		os << ivf.words[i];

	//save the word docs arrays
	ivWordDocCIt wdit;
	for (i=0; i<ivf.nwords; ++i)
	{
		//write for this word
		for (wdit=ivf.words[i].docs.begin(); wdit != ivf.words[i].docs.end();
			wdit++)
			os << *wdit;
	}  
	return os;
}

istream& operator>>(istream& is, ivFile& ivf)
{
	uint i;

	is >> ivf.params;

	//read the document array
	//
	//read the size & allocate
	is.read((char*)&ivf.ndocs, sizeof(ivf.ndocs));
	ivf.docs.resize(ivf.ndocs);
	//put the array
	for (i=0; i<ivf.ndocs; ++i)
		is >> ivf.docs[i];

	//read the words array
	//
	//read the size & allocate
	is.read((char*)&ivf.nwords, sizeof(ivf.nwords));
	ivf.words.resize(ivf.nwords);
	//  cout << "Words: " << this->words.size() << "\n";
	//put the array
	for (i=0; i<ivf.nwords; ++i)
		is >> ivf.words[i];

	//read the word docs arrays
	ivWordDocIt wdit;
	for (i=0; i<ivf.nwords; ++i)
	{
		//get the size of this list
		uint s = ivf.words[i].ndocs;
		ivf.words[i].docs.resize(s);
		//read for this word
		for (wdit=ivf.words[i].docs.begin(); wdit != ivf.words[i].docs.end();
			wdit++)
			is >> *wdit;
	}
	return is;
}

istream& operator>>(istream& is, ivFile::Params& pr)
{
	uint a;

	READ(a); pr.norm   = static_cast<ivFile::Norm>(a);
	READ(a); pr.weight = static_cast<ivFile::Weight>(a);
	
	try 
	{
		pr.check();
	}
	catch(...)
	{
		is.setstate(is.rdstate() | std::ios::failbit);
		throw;
	}

	return is;
}

ostream& operator<<(ostream& os, ivFile::Params const& pr)
{
	uint a;
	a = pr.norm;   WRITE(a);
	a = pr.weight; WRITE(a);

	return os;
}

//------------------------------------------------------------------------
void ivFile::save(string const& filename) const
{
	//open the file for opening
	ofstream of;
	of.open(filename.c_str(), ios_base::binary);

	//save
	of << *this;

	//close file
	of.close();
}


//------------------------------------------------------------------------
void ivFile::load(string const & filename)
{
	//open the file for opening
	ifstream inf;
	inf.open(filename.c_str(), ofstream::binary);

	inf >> *this;

	//close file
	inf.close();
}


//------------------------------------------------------------------------
void ivFile::display( bool show_docs/* = true*/, bool show_words/* = false*/ )
{
	uint i;

	if (show_docs)
	{
		//display the document array
		//
		cout << "Documents:\n----------\n";
		for (i=0; i<this->docs.size(); ++i)
		{
			cout << "Doc " << i << ": ntokens=" << this->docs[i].ntokens <<  
				" nwords=" << this->docs[i].nwords <<  
				" norml0=" << this->docs[i].norml0 << 
				" norml1=" << this->docs[i].norml1 << 
				" norml2=" << this->docs[i].norml2 << "\n";
		}
	}

	if (show_words)
	{
		//display the words array
		cout << "\nWords:\n--------";
		for (i=0; i<this->words.size(); ++i)
		{
			cout << "\nWord " << i << ": (ndocs=" << this->words[i].ndocs <<
				", wf=" << this->words[i].wf << ")\n\t";

			ivWordDocIt wdit;
			for (wdit=this->words[i].docs.begin(); wdit != this->words[i].docs.end();
				wdit++)
			{
				cout << "<doc=" << wdit->doc << ", count=" << wdit->count << 
					", val=" << wdit->val << ">";
			}
		}
	}
}

//------------------------------------------------------------------------


void ivFile::Params::check() throw(std::logic_error)
{
#define CHECK(A,B) if (A >= B) throw std::logic_error(#A "cannot be " #B);

	CHECK(norm, NORM_LAST);
	CHECK(weight, WEIGHT_LAST);
#undef CHECK
}
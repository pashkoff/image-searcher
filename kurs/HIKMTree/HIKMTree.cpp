#include <cmath>
#include <exception>
#include <stdexcept>
#include <fstream>

#include "HIKMTree.hpp"
#include "Util/util.hpp"
#include "Image/Image.hpp"

HIKMTree::HIKMTree(int dims, int clusters, int leaves, VlIKMAlgorithms method):
	mTree(nullptr),
	mLeaves(leaves)
{
	Init(dims, clusters, method);
}

HIKMTree::HIKMTree(HIKMTree::Params& params):
	mTree(nullptr),
	mLeaves(params.leaves)
{
	Init(params.dims, params.clusters, params.method);
}

HIKMTree::HIKMTree(std::string const &fname) :
	mTree(nullptr),
	mLeaves(0)
{
	load(fname);
}

HIKMTree::~HIKMTree(void)
{
	vl_hikm_delete(mTree);
}

void HIKMTree::Init(int dims, int clusters, VlIKMAlgorithms method)
{
	mTree = vl_hikm_new(method);
	if (!mTree)
		throw std::bad_alloc();

	int depth = VL_MAX(1, ceil (log ((double)mLeaves * 1.0) / log((double)clusters)));

	vl_hikm_init(mTree, dims, clusters, depth);
}

void HIKMTree::train(std::vector<unsigned char> const & data)
{
	vl_hikm_train(mTree, &data.front(), data.size() / Dims());
}

void HIKMTree::push(SiftDescr const * data, std::vector<unsigned int> & word) const
{
	word.resize(Depth() * 1);
	vl_hikm_push(mTree, &word.front(), data, 1);
}

void HIKMTree::push(std::vector<SiftDescr> const & data, std::vector<unsigned int> & word)
{
	push(&data.front(), word);
}

void HIKMTree::push(SiftDescr const * data, unsigned int & word) const
{
	push(data, mWordBuf);

	word = 0;
	unsigned int p = 1;
	unsigned int k = Clusters();
	for (auto it = mWordBuf.begin(); it != mWordBuf.end(); ++it)
	{
		word += (*it) * p;
		p *= k;
	}
}

void HIKMTree::push(std::vector<SiftDescr> const & data, unsigned int & word)
{
	push(&data.front(), word);
}

void HIKMTree::push(Image& img)
{
	TRACE;

	auto & iwords = img.getWords();
	auto    idscr = img.getDescr();
	auto   nidscr = img.getDescrCount();

	iwords.clear();
	iwords.resize(nidscr);
	for (size_t i = 0; i < nidscr; ++i)
	{
		push(&idscr[i * 128], iwords[i]);
	}
}

unsigned int HIKMTree::maxWord() const
{
	return (unsigned int)pow((double)Clusters(), Depth()) - 1;
}

void HIKMTree::save(std::string const & fname) const
{
	std::ofstream of;
	of.open(fname.c_str(), std::ofstream::binary);
	save(of);
	of.close();
}

void HIKMTree::save(std::ostream& os) const
{	
	os << *this;
}


void HIKMTree::load(std::string const & fname)
{
	std::ifstream ifs;
	ifs.open(fname.c_str(), std::ifstream::binary);
	load(ifs);
	ifs.close();
}

void HIKMTree::load(std::istream& is)
{
	is >> *this;
}

//////////////////////////////////////////////////////////////////////////


std::ostream& operator<<(std::ostream& os, VlIKMFilt const& filt)
{
	WRITE(filt.M);
	WRITE(filt.K);
	WRITE(filt.method);
	WRITE(filt.max_niters);
	WRITE(filt.verb);
	if (!filt.centers)
		throw std::runtime_error("centers in VlIKMFilt is nullptr. Cannot save it");

	os.write(reinterpret_cast<char const *>(filt.centers), 
		sizeof(filt.centers[0]) * filt.M * filt.K);

	if (filt.method == VL_IKM_ELKAN)
	{
		if (!filt.inter_dist)
			throw std::runtime_error("inter_dist in VlIKMFilt is nullptr. Cannot save it");

		os.write(reinterpret_cast<char const *>(filt.inter_dist), 
			sizeof(filt.inter_dist[0]) * filt.K * filt.K);
	}

	return os;
}

std::istream& operator>>(std::istream& is, VlIKMFilt & filt)
{
	READ(filt.M);
	READ(filt.K);
	READ(filt.method);
	READ(filt.max_niters);
	READ(filt.verb);

	size_t sctr = sizeof(filt.centers[0]) * filt.M * filt.K;
	filt.centers = static_cast<vl_ikm_acc*>(vl_malloc(sctr));
	is.read(reinterpret_cast<char*>(filt.centers), sctr);

	if (filt.method == VL_IKM_ELKAN)
	{
		size_t sidst = sizeof(filt.inter_dist[0]) * filt.K * filt.K;
		filt.inter_dist = static_cast<vl_ikm_acc*>(vl_malloc(sidst));
		is.read(reinterpret_cast<char*>(filt.inter_dist), sidst);
	}
	else
	{
		filt.inter_dist = nullptr;
	}

	return is;
}
std::ostream& operator<<(std::ostream& os, VlHIKMNode const& node)
{
	if (!node.filter)
		throw std::runtime_error("filter in VlHIKMNode is nullptr. Cannot save it");
	
	os << *node.filter;
	bool chld = (nullptr != node.children);
	WRITE(chld);
	if (chld)
	{
		for (int k = 0; k < node.filter->K; ++k)
		{
			if (!node.children[k])
				throw std::runtime_error("children[k] in VlHIKMNode is nullptr. Cannot save it");
			os << *node.children[k];
		}
	}
	
	return os;
}

std::istream& operator>>(std::istream& is, VlHIKMNode & node)
{
	node.filter = vl_ikm_new(0);
	is >> *node.filter;

	bool chld = false;
	READ(chld);
	if (chld)
	{
		node.children = static_cast<VlHIKMNode**>(vl_malloc(sizeof(*node.children) * node.filter->K));
		for (int k = 0; k < node.filter->K; ++k)
		{
			node.children[k] = static_cast<VlHIKMNode*>(vl_malloc(sizeof(*node.children[k])));
			is >> *node.children[k];
		}
	}
	else
	{
		node.children = nullptr;
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, VlHIKMTree const& tree)
{
	WRITE(tree.M);
	WRITE(tree.K);
	WRITE(tree.max_niters);
	WRITE(tree.method);
	WRITE(tree.verb);
	WRITE(tree.depth);
	if (!tree.root)
		throw std::runtime_error("root in VlHIKMTree is nullptr. Cannot save it");
	os << *tree.root;
	return os;
}

std::istream& operator>>(std::istream& is, VlHIKMTree & tree)
{
	READ(tree.M);
	READ(tree.K);
	READ(tree.max_niters);
	READ(tree.method);
	READ(tree.verb);
	READ(tree.depth);

	tree.root = static_cast<VlHIKMNode*>(vl_malloc(sizeof(*tree.root)));
	is >> *tree.root;
	return is;
}

std::ostream& operator<<(std::ostream& os, HIKMTree const & tree)
{
	WRITE(tree.mLeaves);
	if (!tree.mTree)
		throw std::runtime_error("mTree in HIKMTree is nullptr. Cannot save it");
	os << *tree.mTree;
	return os;
}

std::istream& operator>>(std::istream& is, HIKMTree & tree)
{
	READ(tree.mLeaves);

	vl_hikm_delete(tree.mTree);
	tree.mTree = vl_hikm_new(0);

	is >> *tree.mTree;
	return is;
}





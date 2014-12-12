#include "blockchainscan.h"
#include<iterator>

int main(int argc,char** argv)
{
	std::string dirloc("/home/steven/.bitcoin/blocks/");
	std::size_t incrementor=0;
	std::size_t total_transactions=0;
	for(const std::string& filename: bcs::get_block_filenames(dirloc))
	{
		std::ifstream blkin(filename.c_str(),std::ifstream::in|std::ifstream::binary);
		for(auto biterator=std::istream_iterator<bcs::block_t>(blkin);
		biterator!=std::istream_iterator<bcs::block_t>();
		++biterator)
		{
			if((incrementor++ & 0x3F)==0)
			{
				std::cerr << "The current position is " << filename << ": ~%" << ((100*blkin.tellg()) >> 27)  << "\n";
			}
			for(auto titerator=biterator->transactions.cbegin();titerator !=biterator->transactions.cend();++titerator)
			{
			
			}
		}
	}
	return 0;
}
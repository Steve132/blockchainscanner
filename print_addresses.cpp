#include "blockchainscan.h"
#include<iterator>
#include<sstream>
#include<iostream>
#include "BitcoinAddress.h"

/*char numtohex(int i)
{
	static const char hexchars[17]="0123456789ABCDEF";
	return hexchars[i & 0xF];
}

void printaddresshex(std::ostream& out,const std::vector<uint8_t>& v)
{
	for(size_t i=0;i<v.size();i++)
	{
		out << numtohex(v[i] >> 4) << numtohex(v[i]);
	}
	out << "\n";
}*/

struct OneShotReadBuf : public std::streambuf
{
    OneShotReadBuf(char* s, std::size_t n)
    {
        setg(s, s, s + n);
    }
};

int main(int argc,char** argv)
{
	std::string dirloc("/home/steven/.bitcoin/blocks/");
	std::size_t incrementor=0;
	std::size_t total_transactions=0;
	bcs::hash_t lastblockhash;
	std::vector<std::string> files=bcs::get_block_filenames(dirloc);
	size_t begi=0;
	size_t endi=files.size();
	if(argc > 1)
	{
		std::istringstream(argv[1]) >> begi;
	}
	if(argc > 2)
	{
		std::istringstream(argv[2]) >> endi;	
	}
	
//	std::vector<char> filedata_backing(1 << 28);

	for(size_t i=begi;i<endi;i++)
	{
		const std::string& filename=files[i];
		std::ifstream blkin(filename.c_str(),std::ifstream::in|std::ifstream::binary);
/*		std::cerr << "Reading file into memory" << std::endl;
		blkin.seekg (0, blkin.end);
		size_t length = blkin.tellg();
		blkin.seekg (0, blkin.beg);
		blkin.read(&filedata_backing[0],length);
		OneShotReadBuf osrb(&filedata_backing[0],length);
		std::istream istr(&osrb);
		std::cerr << "Reading file into memory complete" << std::endl;*/
		
		
		
		for(auto biterator=std::istream_iterator<bcs::block_t>(blkin);
		biterator!=std::istream_iterator<bcs::block_t>();
		++biterator)
		{
			if((incrementor++ & 0xFF)==0)
			{
				std::cerr << "The current position is " << filename << ": ~%" << ((100*blkin.tellg()) >> 27)  << "\n";
			}
			for(auto titerator=biterator->transactions.cbegin();titerator !=biterator->transactions.cend();++titerator)
			{
				for(auto outputiterator=titerator->outputs.cbegin();outputiterator!=titerator->outputs.cend();++outputiterator)
				{
					auto lst=outputiterator->script.get_addresses();
					
					for(const std::vector<uint8_t>& a: lst)
					{
						std::array<uint8_t,25> binaddress;
						std::array<char,37> ascii;
						if(a.size() > 63)
						{
							bitcoinPublicKeyToAddress(&a[0],&binaddress[0]);
						}
						else if(a.size()==20)
						{
							bitcoinRIPEMD160ToAddress(&a[0],&binaddress[0]);
						}
						bitcoinAddressToAscii(&binaddress[0],&ascii[0],37);
						std::cout << &ascii[0] << "\n";
					}
				}
			}
			lastblockhash=biterator->previous;
		}
	}
	std::cerr << lastblockhash << std::endl;
	return 0;
}
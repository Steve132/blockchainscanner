#include "blockchainscan.h"
#include<iterator>
#include<sstream>
#include<iostream>
#include<cstring>
#include<algorithm>
#include "BitcoinAddress.h"

typedef std::array<uint8_t,25> local_address_t;
bool operator<(const local_address_t& a,const local_address_t& b)
{
	return memcmp(reinterpret_cast<const char*>(&a),reinterpret_cast<const char*>(&b),25) < 0;
}

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
	
	std::vector<local_address_t > all_addresses;
	size_t memory=(1ULL << 34)/25;
	all_addresses.reserve(memory);
	

	for(size_t i=begi;i<endi;i++)
	{
		const std::string& filename=files[i];
		std::ifstream blkin(filename.c_str(),std::ifstream::in|std::ifstream::binary);
		
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
						local_address_t binaddress;
						std::array<char,37> ascii;
						if(a.size() > 60)
						{
							bitcoinPublicKeyToAddress(&a[0],&binaddress[0]);
						}
						else if(a.size() >= 20)
						{
							bitcoinRIPEMD160ToAddress(&a[0],&binaddress[0]);
						}
						all_addresses.push_back(binaddress);
					}
				}
				total_transactions++;
			}
			lastblockhash=biterator->previous;
		}
	}
	std::cerr << "Detected " << total_transactions << " transactions" << std::endl;
	std::cerr << "Sorting all addressess" << std::endl;
	std::sort(all_addresses.begin(),all_addresses.end());
	std::cerr << "Removing all addresses" << std::endl;
	auto newend=std::unique(all_addresses.begin(),all_addresses.end());
	std::cerr << "Printing all addresses" << std::endl;
	for(auto i=all_addresses.begin();i!=newend;++i)
	{
		std::array<char,37> ascii;
		bitcoinAddressToAscii(&(*i)[0],&ascii[0],37);
		std::cout << &ascii[0] << "\n";
	}
	return 0;
}

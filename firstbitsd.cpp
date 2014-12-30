#include<array>
#include<iostream>
#include<atomic>
#include<algorithm>
#include<fstream>
#include<sstream>
#include<memory>
#include<iterator>
#include "microhttpdpp.hpp"
#include "mhttpdfiles.h"
#include "microhttpd.h"
#include "blockchainscan.h"
#include "BitcoinAddress.h"
#include "firstbits.h"

/*
Requests it can recieve: 
	POST /block (authentication required...maybe only allowed from localhost!)
		goes through and processes adding the block to the mmapped file.  Calls msync when finished and then (last) updates the lastblockid number
		(block is not blockfile format (fuck parsing that noise) but is instead a JSON list of addresses and a blocknumber max.
	GET /block
		recieves the age of the last block added to the DB
		
	GET /firstbits/<bitstring>
		/looks up in the database and returns a list of all the addresses that match..  Needs at least version and 4 characters or it fails.
*/
	
#ifdef TESTMODE
int test()
{
	firstbits_t fb("out.db",{36*36*36,16,3});
	const std::string testaddresses[]={"1Applesaucemeister","1Pantywaste","1Appletini","1Appliancedirect","1Applesaucemeister"};
	fb.load_block(3,testaddresses,testaddresses+5);
	auto result=fb.get_firstbits("1Apple");
	
 	for(const address_t* i=result.first;i!=result.second;++i)
	{
		std::cout << &(i->data[0]) << std::endl;
	}
	return 0;
}
#endif

static int api_server(firstbits_t* fb,
			struct MHD_Connection * con,
			const std::string& url,
			const std::string& method,
			const std::string& version,
			const char* upload_data,
			size_t * upload_data_size,
			void ** ptr)
{
	static int dummy;			//Not reentrant?!
	if(&dummy != *ptr)
	{
		/* The first time only the headers are valid,
			do not respond in the first round... */
		*ptr = &dummy;
		return MHD_YES;
	}
	
	if(method=="GET")
	{
		if(url.substr(5,9)=="firstbits")
		{
			
			std::ostringstream oss;
			oss << "[";
			bool skip=false;
			std::string searchquery=url.substr(15);
			for(auto ch:searchquery)
			{
				if(!isalnum(ch))
				{
					skip=true;
				}
			}
			if(!skip)
			{
				auto result=fb->get_firstbits(searchquery);
				bool comma=false;
	
				for(const address_t* i=result.first;i!=result.second;++i)
				{
					if(comma)
					{
						oss << ",";
					}
					oss << "\"" << &(i->data[0]) << "\"";
					comma=true;
				}
			}
			
			oss << "]";
			return mdhpp_respond(con,oss.str());
		}
		else if(url.substr(5,5)=="block")
		{
			std::ostringstream oss;
			oss << *(fb->lastblockchainid) << "\n";
			return mdhpp_respond(con,oss.str());
		}
		return mdhpp_respond(con,"<html><body><h1>Api Call not recognized</h1></body></html>");
	}
	return MHD_NO;
}

static int serve_dispatch(filecache* fc,firstbits_t* fb,
			struct MHD_Connection * con,
			const std::string& url,
			const std::string& method,
			const std::string& version,
			const char* upload_data,
			size_t * upload_data_size,
			void ** ptr)
{
#ifndef NDEBUG
	std::cout << method << " " << url << std::endl;
#endif
	if(url.substr(0,4)=="/api")
	{
		return api_server(fb,con,url,method,version,upload_data,upload_data_size,ptr);
	}
	else
	{
		return fc->serve_cached_files(con,url,method,version,upload_data,upload_data_size,ptr);
	}
}

void import_blockchain(firstbits_t* fb,const std::string& dirloc,size_t begi=0,size_t endi=0)
{
///	std::atomic<std::size_t> incrementor;
	std::size_t total_transactions=0;
	bcs::hash_t lastblockhash;
	std::vector<std::string> files=bcs::get_block_filenames(dirloc);
	endi=endi ? endi : files.size();

//	#pragma omp parallel for
	for(size_t i=begi;i<endi;i++)
	{
		std::size_t incrementor=0;
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
						std::array<uint8_t,25> binaddress;
						address_t ascii;
						if(a.size() > 63)
						{
							bitcoinPublicKeyToAddress(&a[0],&binaddress[0]);
						}
						else if(a.size()==20)
						{
							bitcoinRIPEMD160ToAddress(&a[0],&binaddress[0]);
						}
						bitcoinAddressToAscii(&binaddress[0],&ascii.data[0],SIZE_ADDRESS);
						fb->insert_address(ascii);
					}
				}
			}
			lastblockhash=biterator->previous;
		}
	}
	std::cerr << lastblockhash << std::endl;
}

int main(int argc,char** argv)
{
	std::string httpdir=".";
	uint32_t httpport=8080;
	std::string dbfile="out.db";
	uint32_t num_characters_per_block=3;
	uint32_t num_addresses_per_block=16;
	uint32_t load_block=0;
	bool create_only=false;
	bool load_blockchain=false;
	
	
	std::vector<std::string> args(argv,argv+argc);
	for(uint i=1;i<argc;i++)
	{
		if(args[i]=="--httpdir")
		{
			httpdir=args[++i];
		}
		else if(args[i]=="--httpport")
		{
			std::istringstream(args[++i]) >> httpport;
		}
		else if(args[i]=="--dbfile")
		{
			dbfile=args[++i];
		}
		else if(args[i]=="--num_characters_per_block")
		{
			std::istringstream(args[++i]) >> num_characters_per_block;
		}
		else if(args[i]=="--num_addresses_per_block")
		{
			std::istringstream(args[++i]) >> num_addresses_per_block;
		}
		else if(args[i]=="--load_block")
		{
			std::istringstream(args[++i]) >> load_block;
		}
		else if(args[i]=="--create_only")
		{
			create_only=true;
		}
		else if(args[i]=="--load_blockchain")
		{
			load_blockchain=true;
		}
	}
	
	firstbits_t::metadata_t mdata;
	mdata.num_blocks=1;
	mdata.max_addresses_per_block=num_addresses_per_block;
	mdata.num_characters_per_block=num_characters_per_block;
	for(int i=0;i<mdata.num_characters_per_block;i++)
	{
		mdata.num_blocks*=36;
	}
	firstbits_t fb(dbfile,mdata);
	
	if(load_blockchain)
	{
		std::string dirloc("/home/steven/.bitcoin/blocks/");
		import_blockchain(&fb,dirloc,load_block);
	}
	else if(load_block==0)
	{
		if(create_only)
		{
			return 0;
		}
		filecache fc(httpdir);
		
		respondfunctype rfn=std::bind(serve_dispatch,&fc,&fb,
					std::placeholders::_1,
					std::placeholders::_2,
					std::placeholders::_3,
					std::placeholders::_4,
					std::placeholders::_5,
					std::placeholders::_6,
					std::placeholders::_7);
  
		struct MHD_Daemon* d = mhdpp_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		       httpport,
		       mdhpp_default_accept,
		       rfn,
		       MHD_OPTION_END);
		if (d == NULL)
			return 1;
		(void) getchar ();
		MHD_stop_daemon(d);
		return 0;

	}
	else
	{
		fb.load_block(load_block,(std::istream_iterator<std::string>(std::cin)),std::istream_iterator<std::string>());//load blocks from stdin
	}
	return 0;
}

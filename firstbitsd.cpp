#include<array>
#include<iostream>
#include<atomic>
#include<algorithm>
#include<fstream>
#include<memory>

#include<cstring>
#include<cstdint>
#include<cctype>

#include<sys/mman.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mhttpdfiles.h"
#include "microhttpd.h"
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
	

static const unsigned int SIZE_ADDRESS=36;
static const uint32_t WRITER_MODE=0xFFFFFFFF;

struct address_t
{
	std::array<char,SIZE_ADDRESS> data;
	
	address_t(const std::string& s="")
	{
		memset(&data[0],0,SIZE_ADDRESS);
		std::strncpy(&data[0],s.c_str(),std::min((size_t)SIZE_ADDRESS,s.size()));
	}
};

inline static void lowermod(address_t& add)
{
	for(int i=1;i<SIZE_ADDRESS;i++)		//don't mod leading character
	{
		add.data[i]=std::tolower(add.data[i]);
	}
}
bool operator<(address_t a,address_t b)
{
	lowermod(a);
	lowermod(b);
	return memcmp(reinterpret_cast<const char*>(&a),reinterpret_cast<const char*>(&b),SIZE_ADDRESS) < 0;
}
bool search_comparator(address_t a,address_t b)
{
	size_t na=strlen(reinterpret_cast<const char*>(&a));
	size_t n=strlen(reinterpret_cast<const char*>(&b));
	na=na < SIZE_ADDRESS ? na : SIZE_ADDRESS;
	n=n < na ? n : na;
	lowermod(a);
	lowermod(b);
	return memcmp(reinterpret_cast<const char*>(&a),reinterpret_cast<const char*>(&b),n) < 0;
}


struct block_t
{
	std::uint16_t num_addresses;
	address_t first;	//is a dynamic length array of addresses in contiguous memory.  Zero-padded
};

template<class IntType>
static inline IntType read_le(std::istream& in)
{
	IntType dout=0;
	for(unsigned int i=0;i<sizeof(IntType);i++)
	{
		dout|=static_cast<IntType>(in.get()) << (8*i);
	}
	return dout;
}
template<class IntType>
static inline void write_le(std::ostream& out,const IntType& it)
{
	for(unsigned int i=0;i<sizeof(IntType);i++)
	{
		out.put((it >> (8*i)) & 0xFF);
	}
}
inline int char2hashdex(char a)
{
	if(std::isalpha(a))
	{
		return std::tolower(a)-'a'+10;
	}
	else if(std::isdigit(a))
	{
		return a-'0';
	}
	return 0;
}

struct firstbits_t
{
private:
	char* filebeginning;	//memmaped file
	char* tablebeginning;   //memmapped table;
	
	struct metadata_t
	{
		std::uint64_t num_blocks;    //e.g. 36^num_hashsize;
		std::uint16_t max_addresses_per_block; //e.g. 256
		std::uint8_t num_characters_per_block;	  //e.g. 4
	};
	
	metadata_t mdata;
	uint32_t* lastblockchainid;
	
	mutable std::unique_ptr<std::atomic<std::uint32_t>[] > block_threadstate;
	size_t blocksize;
	size_t pagesize;
	int backendfd;
	
	std::uint64_t hashbucket(const address_t& bitsearch) const
	{
		uint64_t hashindex=0;
		for(uint_fast8_t i=0;i<mdata.num_characters_per_block;i++)
		{
			hashindex*=36;
			hashindex+=char2hashdex(bitsearch.data[1+i]);
		}
		return hashindex;
	}
	
	void create_file(const std::string& fn,const metadata_t& md)
	{
		std::ofstream outfile(fn.c_str(),std::ofstream::binary | std::ofstream::out);
		write_le(outfile,md.num_blocks);
		write_le(outfile,md.max_addresses_per_block);
		write_le(outfile,md.num_characters_per_block);
		write_le<std::uint32_t>(outfile,0);//set the last blockchainid
		size_t blocksize=md.max_addresses_per_block*SIZE_ADDRESS+sizeof(std::uint16_t);
		
		char* zbuf=(char*)malloc(blocksize);
		memset(zbuf,0,blocksize);
		
		for(size_t i=0;i<md.num_blocks;i++)
		{
			outfile.write(zbuf,blocksize);
		}
		outfile.close();
	}
	int safemsync(void* addr,size_t len,int flags)
	{
		size_t addrl=(size_t)addr;
		size_t addrp=addrl & ~(pagesize-1);
		len+=addrl-addrp;
		return msync((void*)addrp,len,flags);
	}
public:

	//loading constructor
	firstbits_t(const std::string& filename,const metadata_t& md={36*36*36*36,256,4})
	{
		std::ifstream indata(filename.c_str(),std::ifstream::binary | std::ifstream::in);
		if(!indata)
		{
			indata.close();
			create_file(filename,md);
			indata.open(filename.c_str(),std::ifstream::binary | std::ifstream::in);
		}

		mdata.num_blocks=read_le<std::uint64_t>(indata);
		mdata.max_addresses_per_block=read_le<std::uint16_t>(indata);
		mdata.num_characters_per_block=read_le<std::uint8_t>(indata);
		indata.close();
		block_threadstate.reset(new std::atomic<std::uint32_t>[mdata.num_blocks]);
		blocksize=mdata.max_addresses_per_block*SIZE_ADDRESS+sizeof(std::uint16_t);
		size_t len=sizeof(metadata_t)+sizeof(uint32_t)+mdata.num_blocks*blocksize;
		
		backendfd=open(filename.c_str(),O_RDWR | O_NOATIME);

		void* fb=mmap(NULL,len,PROT_READ | PROT_WRITE, MAP_SHARED,backendfd,0);
		pagesize=getpagesize();
		
		filebeginning=static_cast<char*>(fb);
		
		char* curpos=filebeginning;
		curpos+=sizeof(mdata);
		lastblockchainid=reinterpret_cast<uint32_t*>(curpos);
		curpos+=sizeof(uint32_t);
		
		tablebeginning=curpos;
	}

	std::pair<const address_t*,const address_t*> get_firstbits(const std::string& bitsearch) const
	{
		if(bitsearch.size() < 5 || bitsearch.size() > SIZE_ADDRESS)
		{
			return {NULL,NULL};
		}
		
		address_t searchbits(bitsearch);

		std::uint64_t hashindex=hashbucket(searchbits);
		std::atomic<std::uint32_t>* readerstate=&block_threadstate[hashindex];
				
		uint32_t prev=WRITER_MODE;
		while(readerstate->compare_exchange_strong(prev,WRITER_MODE)); //while it's writing, loop
		++(*readerstate);
		
		const block_t* thisblock=(const block_t*)(tablebeginning+hashindex*blocksize);
		
		const address_t* first=&thisblock->first;
		const address_t* last=first+thisblock->num_addresses;
		const address_t *outbegin=std::lower_bound(first,last,searchbits,search_comparator);
		const address_t *outend=std::upper_bound(first,last,searchbits,search_comparator);
		
		--(*readerstate);
		return {outbegin,outend};
	}
	
	void insert_address(const address_t& address)		
	{
		std::uint64_t hashindex=hashbucket(address);
		std::atomic<std::uint32_t>* readerstate=&block_threadstate[hashindex];		
		
		uint32_t prev=0;
		while(!readerstate->compare_exchange_strong(prev,WRITER_MODE)); //while it's reading or writing, loop

		block_t* thisblock=(block_t*)(tablebeginning+hashindex*blocksize);
		
		address_t* first=&thisblock->first;
		address_t* last=first+thisblock->num_addresses;
		address_t *position=std::lower_bound(first,last,address);
		
		if(strncmp(reinterpret_cast<const char*>(position),reinterpret_cast<const char*>(&address),SIZE_ADDRESS)!=0)//Don't insert duplicates!
		{
			*(last)=address;
			std::rotate(position,last,last+1);
			thisblock->num_addresses+=1;
		}
		
		safemsync(thisblock,blocksize,MS_ASYNC);
		
		*readerstate=0;
	}
	
	template<class StringIterator>
	void load_block(const std::uint32_t& blockid,StringIterator ab,StringIterator ae)
	{
		for(StringIterator ai=ab;ai!=ae;++ai)
		{
			insert_address(*ai);
		}
		if(blockid > *lastblockchainid)
		{
			*lastblockchainid=blockid;
		}
		safemsync(lastblockchainid,sizeof(uint32_t),MS_ASYNC);
	}
};


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



int main(int argc,char** argv)
{
	
}

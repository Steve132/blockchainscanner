#include "blockchainscan.h"
#include<sstream>
#include<iomanip>
#include<list>
#include<algorithm>
//http://codesuppository.blogspot.com/2014/01/how-to-parse-bitcoin-blockchain.html


namespace bcs
{
template<class inttype>
inttype read_le(std::istream& iss)
{
	inttype r=0;
	for(std::uint_fast8_t i=0;i<sizeof(inttype);i++)
	{
		inttype nc=iss.get();
		r |= nc << (i*8);
	}
	return r;
}
std::uint64_t read_le_vli(std::istream& iss)
{
	std::uint8_t fb=iss.get();
	switch(fb)
	{
	case 253:
		return read_le<std::uint16_t>(iss);
	case 254:
		return read_le<std::uint32_t>(iss);
	case 255:
		return read_le<std::uint64_t>(iss);
	default:
		return fb;
	};
}
std::istream& operator>>(std::istream& iss,script_t& sc)
{
	std::uint64_t scriptsize=read_le_vli(iss);
	sc.bytes.resize(scriptsize);
	return iss.read(reinterpret_cast<char*>(&sc.bytes[0]),scriptsize);
}
std::istream& operator>>(std::istream& iss,hash_t& hs)
{
	return iss.read(reinterpret_cast<char*>(&hs[0]),32);
}
std::istream& operator>>(std::istream& iss,input_t& in)
{
	iss >> in.transaction_hash;
	in.transaction_index=read_le<std::uint32_t>(iss);
	iss >> in.script;
	std::uint32_t sequence_number=read_le<std::uint32_t>(iss);
	return iss;
}
std::istream& operator>>(std::istream& iss,output_t& out)
{
	out.value=read_le<std::uint64_t>(iss);
	return iss >> out.script;
}
std::istream& operator>>(std::istream& iss,transaction_t& ts)
{
	std::uint32_t version=read_le<std::uint32_t>(iss);
	std::uint64_t inputs=read_le_vli(iss);
	ts.inputs.resize(inputs);
	for(size_t i=0;i<inputs;i++)
	{
		iss >> ts.inputs[i];
	}
	std::uint64_t outputs=read_le_vli(iss);
	ts.outputs.resize(outputs);
	for(size_t i=0;i<outputs;i++)
	{
		iss >> ts.outputs[i];
	}
	ts.locktime=read_le<uint32_t>(iss);
	return iss;
}
std::istream& operator>>(std::istream& iss,block_t& ts)
{
	
	for(std::uint32_t ch=read_le<std::uint32_t>(iss);iss && ch!=0xD9B4BEF9;ch=read_le<std::uint32_t>(iss));
	if(!iss) {return iss;}
	
	//std::size_t     begg=iss.tellg();
	//begg-=4;  I bet block length doesn't include magic bytes or block length
	std::uint32_t 	blocklength=read_le<std::uint32_t>(iss);
	std::size_t     begg=iss.tellg();
	std::uint32_t   versionnumber=read_le<std::uint32_t>(iss);
	iss >> ts.previous;
	iss >> ts.merkle_root;
	ts.timestamp=read_le<std::uint32_t>(iss);
	ts.difficulty=read_le<std::uint32_t>(iss);
	ts.nonce=read_le<std::uint32_t>(iss);
	std::uint64_t transactions=read_le_vli(iss);
	ts.transactions.resize(transactions);
	for(size_t i=0;i<transactions;i++)
	{
		iss >> ts.transactions[i];
	}
	std::size_t endg=iss.tellg();
	iss.ignore(blocklength-(endg-begg)); //re-add this once you are clear
	return iss;
}

std::string get_block_filename(const std::string& directory,const std::size_t& a)
{
	std::ostringstream out;
	out << "blk" << std::setw(5) << std::setfill('0') << a << ".dat";
	return directory+out.str();
}



std::vector<std::string> get_block_filenames(const std::string& directory)
{
	std::string blkfilename;
	std::vector<std::string> outnames;
	for(	std::size_t count=0;
		std::ifstream(
			(blkfilename=get_block_filename(directory,count)).c_str()
			,std::ifstream::in | std::ifstream::binary
			);
		count++)
	{
			outnames.push_back(blkfilename);
	}
	return outnames;
}

typedef std::vector<uint8_t> run;

std::list<run> script_t::get_addresses() const
{
	std::list<run> allruns;
	run currentrun;
	
	for(size_t i=0;i<bytes.size();i++)
	{
		size_t datapos=0;
		size_t datalen=0;
		
		uint8_t cmd=bytes[i++];
		if(cmd >= 1 && cmd <= 75)
		{
			datalen=cmd;
			datapos=i;
			i+=datalen;
		}
		else if(cmd >=76 && cmd <=78)
		{
			uint_fast8_t numbytes=1 << (cmd-76);
			for(uint_fast8_t sb=0;sb < numbytes;sb++)
			{
				datalen|=((uint32_t)bytes[i+sb]) << (8*sb);
			}
			i+=numbytes;
			datapos=i;
			i+=datalen;
		}
		
		if(datalen == 0 && currentrun.size())
		{
			allruns.push_back(currentrun);
			currentrun.clear();
		}
		else if(datalen != 0)
		{
			currentrun.reserve(currentrun.size()+datalen);
			for(size_t k=datapos;(k<(i+datalen)) && (k < bytes.size());k++)
			{
				currentrun.push_back(bytes[k]);
			}
		}
	}
	if(currentrun.size())
	{
		allruns.push_back(currentrun);
	}
	std::remove_if(allruns.begin(),allruns.end(),[](const run& r)
	{
		return !(r.size() >= 20);
	});
	return allruns;
}



/*
std::vector<script_command_t> script_t::parse() const
{
	static const uint8_t DATA=0xFF;
	std::vector<script_command_t> output;
	for(size_t i=0;i<bytes.size();i++)
	{
		uint8_t cmd=bytes[i];
		
		output.emplace_back(cmd,i);
		if(cmd >= 1 && cmd <= 75)
		{
			i+=cmd;
		}
		else if(cmd >=76 && cmd <=78)
		{
			uint32_t skipamount=0;
			uint_fast8_t numbytes=1 << (cmd-76);
			for(uint_fast8_t sb=0;sb < numbytes;sb++)
			{
				skipamount|=((uint32_t)bytes[i+1+sb]) << (8*sb);
			}
			i+=numbytes;
			i+=skipamount;
		}
	}
}*/



}

std::ostream& operator<<(std::ostream& oss,const bcs::hash_t& hs)
{
	std::ios::fmtflags f(oss.flags());
	oss << "0x";
	for(int i=0;i<32;i++)
	{
		oss << std::hex << (int)hs[31-i];
	}
	oss.flags(f);
	return oss;
}

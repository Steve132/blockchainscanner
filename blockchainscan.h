#ifndef BLOCKCHAINSCAN_H
#define BLOCKCHAINSCAN_H

#include<iostream>
#include<fstream>
#include<cstdint>
#include<array>
#include<vector>
#include<list>

namespace bcs
{
typedef std::array<std::uint8_t,32> hash_t;
/*struct script_command_t
{
	std::uint8_t opcode;
	std::size_t offset;
	script_command_t(const std::uint8_t& opc,const std::uint8_t& off):
		opcode(opc),offset(off)
	{}
};*/
struct script_t
{
	std::vector<uint8_t> bytes;
	std::list<std::vector<uint8_t> > get_addresses() const;
	
	//std::vector<script_command_t> parse() const;
};
struct input_t
{
	hash_t transaction_hash;
	std::uint32_t transaction_index;
	script_t script;
};
struct output_t
{
	std::uint64_t value;
	script_t script;
};
struct transaction_t
{
	std::vector<input_t> inputs;
	std::vector<output_t> outputs;
	std::uint32_t locktime;
};

struct block_t
{
	std::uint32_t total_length;
	std::uint32_t version;
	hash_t previous;
	hash_t merkle_root;
	std::uint32_t timestamp;
	std::uint32_t difficulty;
	std::uint32_t nonce;
	std::vector<transaction_t> transactions;
};
std::vector<std::string> get_block_filenames(const std::string& directory);
std::string get_block_filename(const std::string& directory,const std::size_t& a);


std::istream& operator>>(std::istream& iss,script_t& sc);
std::istream& operator>>(std::istream& iss,hash_t& hs);
std::istream& operator>>(std::istream& iss,input_t& in);
std::istream& operator>>(std::istream& iss,output_t& out);
std::istream& operator>>(std::istream& iss,transaction_t& ts);
std::istream& operator>>(std::istream& iss,block_t& ts);

}

std::ostream& operator<<(std::ostream& oss,const bcs::hash_t& hs);

#endif 

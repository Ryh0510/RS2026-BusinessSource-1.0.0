#pragma once

#include <string>
using std::string;
#include <fstream>
using std::ifstream;
using std::ofstream;


class StringToFile
{
public:
	StringToFile();
	~StringToFile();

	static void save_string(ofstream& ofile, string str);
	static void load_string(ifstream& ifile, string& str);
	static bool load_string(ifstream& ifile, string& str, unsigned int max_length_);

private:

};



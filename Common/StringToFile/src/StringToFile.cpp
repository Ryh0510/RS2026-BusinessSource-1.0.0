#include <StringToFile/StringToFile.h>
#include <cstring>

StringToFile::StringToFile()
{
}

StringToFile::~StringToFile()
{
}

void StringToFile::save_string(ofstream& ofile, string str)
{
	unsigned int str_size;
	str_size = str.size();
	ofile.write((char*)&str_size, sizeof(str_size));
	ofile.write((char*)str.c_str(), str_size);
}

void StringToFile::load_string(ifstream& ifile, string& str)
{
	char buff[100];
    std::memset(buff, 0, 100);
	unsigned int str_size;
	ifile.read((char*)&str_size, sizeof(str_size));
	ifile.read((char*)buff, str_size);
	str = buff;
}

bool StringToFile::load_string(ifstream& ifile, string& str, unsigned int max_length_)
{
	char buff[100];
	memset(buff, 0, 100);

	//	Load the string size
	unsigned int str_size;
	ifile.read((char*)&str_size, sizeof(str_size));

	//	Check the string size
	if (str_size > max_length_)
		return false;

	ifile.read((char*)buff, str_size);
	str = buff;
	return true;
}


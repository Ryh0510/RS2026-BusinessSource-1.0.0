#pragma once
#include <fstream>
using std::ofstream;
using std::streambuf;

#include <string>
using std::string;

#include <memory>


class LogSystem
{
private:
	ofstream		m_outfile;
	streambuf*		m_oldbuf;

public:
	LogSystem();
	~LogSystem();

	bool enable_logfile(string filename = "");
	bool disable_logfile();


	LogSystem& operator << ( const char* s );

	static std::shared_ptr<LogSystem> get_logsystem(void);
};

typedef std::shared_ptr<LogSystem> ilogsystem_spointer;

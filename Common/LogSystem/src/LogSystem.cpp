#include <LogSystem/LogSystem.h>

#include <iostream>
using std::cout;
using std::endl;

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>



LogSystem::LogSystem()
{
	enable_logfile();
}


LogSystem::~LogSystem()
{
	disable_logfile();
}

bool LogSystem::enable_logfile(string filename) {

    if(!boost::filesystem::exists("../log") || !boost::filesystem::is_directory("../log")){
        boost::filesystem::path parent_dir = boost::filesystem::current_path().parent_path();
        boost::filesystem::path log_dir = parent_dir / "log";
        boost::filesystem::create_directory(log_dir);
        cout << "Directory log doesn't exist and be created! " << endl;
    }

	if (filename == "") {
		string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
        filename = "../log/Simulator" + strTime + ".log";
	}

	m_outfile.open(filename);
	m_oldbuf = cout.rdbuf(m_outfile.rdbuf());		// Save the standard output
	return true;
}

bool LogSystem::disable_logfile(void) {
	if (m_oldbuf) {
		cout.rdbuf(m_oldbuf);					// Redirection the output to the file
		m_outfile.close();
	}
	return true;
}

LogSystem& LogSystem::operator<<( const char* s )
{
	// TODO: Insert a return statement here.
	cout << s << endl;
	return *this;
}

std::shared_ptr<LogSystem> LogSystem::get_logsystem(void) {
	return std::make_shared<LogSystem>();
}


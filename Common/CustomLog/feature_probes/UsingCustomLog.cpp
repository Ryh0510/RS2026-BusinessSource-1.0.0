#include <iostream>
#include <CustomLog/CustomLog.h>

int main(void){
    CustomLog::init({
        {"core",{true,true}},
        {"algo",{true,false}}
    }, "Robot", "logs");

    CustomLog::set_level(CustomLog::Level::info);

    LOG_TRACE("core") << "Testing trace Log!";
    LOG_DEBUG("core") << "Testing debug Log!";
    LOG_INFO("core") << "Testing info Log!";
    LOG_WARNING("core") << "Testing warning Log!";
    LOG_ERROR("core") << "Testing error Log!";
    LOG_FATAL("core") << "Testing error Log!";

    //std::cout << "CORE write: " << &boost::log::core::get() << std::endl;


    return 0;
}
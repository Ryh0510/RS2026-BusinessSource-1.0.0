#include "FileSystem/uuid_robot.h"

#include <boost/lexical_cast.hpp>



namespace RobotSimulator2020 {

    RobotSimulator2020::uuid_robot::uuid_robot()
    {
    }

    RobotSimulator2020::uuid_robot::~uuid_robot()
    {
    }

    boost::uuids::uuid RobotSimulator2020::uuid_robot::get_uuid()
    {
        return boost::uuids::random_generator()();
    }

    boost::uuids::uuid uuid_robot::get_uuid(const char* _uuid_str)
    {

        boost::uuids::string_generator sgen;
        return sgen(_uuid_str);
    }

    std::string uuid_robot::get_uuid_str()
    {
        boost::uuids::random_generator rgen;
        return boost::lexical_cast<std::string>(rgen());
    }


}
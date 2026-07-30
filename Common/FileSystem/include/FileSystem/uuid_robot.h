#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

using::boost::uuids::uuid;
using::boost::uuids::string_generator;




namespace RobotSimulator2020 {


	class uuid_robot
	{
	public:
		uuid_robot();
		~uuid_robot();

		static boost::uuids::uuid get_uuid();
		static boost::uuids::uuid get_uuid(const char* _uuid_str);
		static std::string get_uuid_str();

	private:

	};


}


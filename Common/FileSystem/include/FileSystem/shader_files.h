#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

using::boost::uuids::uuid;
using::boost::uuids::string_generator;

#include <string>
using std::string;
#include <map>
using std::map;


namespace RobotSimulator2020 {

	struct shader_pair
	{
		string name;
		string vert_string;
		string frag_string;
		string geom_string;
	};


	class shader_files
	{
	public:
		shader_files();
		~shader_files();

		shader_pair get_shader_pair(std::string _shader_name);
	private:
		map<string, shader_pair>		m_shader_programs;

		static const char* const mlmm_global_frag;
		static const char* const mlmm_global_vert;
	};

}


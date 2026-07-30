#include "FileSystem/shader_files.h"

#include <boost/lexical_cast.hpp>



namespace RobotSimulator2020 {




    RobotSimulator2020::shader_files::shader_files()
    {
        shader_pair shader01 = { "mlmm_global", shader_files::mlmm_global_vert, shader_files::mlmm_global_frag, "" };
        m_shader_programs.insert(map<std::string, shader_pair>::value_type(shader01.name, shader01));

    }

    RobotSimulator2020::shader_files::~shader_files()
    {
    }

    shader_pair shader_files::get_shader_pair(std::string _shader_name)
    {
        return m_shader_programs[_shader_name];
    }




}
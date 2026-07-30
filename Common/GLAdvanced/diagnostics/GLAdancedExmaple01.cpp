#include <iostream>
using std::cout;
using std::endl;

#include <GLAdvanced/GLAdvanced.hpp>


int main(void){

    GLAdvanced gl_init;
    bool gl_result = gl_init.init();


    std::cout << "GL Adanced init " << gl_result << std::endl;

    return 0;
}


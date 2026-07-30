#include <iostream>
#include <string>

#include <IDGenerator/IDGenerator.h>

int main()
{
    //IDGenerator id_generator;
    
#ifndef IDGEN_USE_UUID
    //  Generate a new id
    ID id = IDGenerator::generate();
    std::cout << "New Generated ID is: " << id << std::endl;
#endif

    ID id2 = IDGenerator::generate();
    std::cout << "New Generated ID is: " << IDGenerator::toString(id2) << std::endl;

    return 0;
}

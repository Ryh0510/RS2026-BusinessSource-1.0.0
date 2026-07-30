#include <Collision/CollisionWorld.h>

#include <fcl/fcl.h>

#include <iostream>
#include <memory>

int main()
{
    using namespace collision;

    auto box = std::make_shared<CollisionGeometry>(std::make_shared<fcl::Boxd>(1.0, 1.0, 1.0));

    auto objectA = std::make_shared<CollisionObject>(1, box);
    auto objectB = std::make_shared<CollisionObject>(2, box);

    Transform3 transformA = Transform3::Identity();
    Transform3 transformB = Transform3::Identity();
    transformB.translation() = Vec3(0.5, 0.0, 0.0);

    objectA->setTransform(transformA);
    objectB->setTransform(transformB);

    CollisionWorld world;
    world.addObject(objectA);
    world.addObject(objectB);
    world.update();

    CollisionResult result;
    world.checkCollision(result);

    std::cout << "Collision: " << (result.inCollision() ? "true" : "false") << "\n";
    return result.inCollision() ? 0 : 1;
}


#pragma once
#include "foundation/smart_ptr.hpp"
#include "app/details/app_driver.hpp"

namespace Engine
{
    class Application
    {
    public:
        void Init();

        void Shutdown();

        void Tick();

    private:
        UniquePtr<IAppDriver> Driver;
    };
}
#include <print>
#include "Matrix.h"

int main()
{
    Jidao::Matrix<2, 2> m = {
        {2, 0},
        {0, 2}
    };
    Jidao::Vector<2> v = {
        1,1
    };
    Jidao::Matrix<2, 1> r = m*v;
    auto [x,y] = r.get();
    std::println("{},{}", x, y);
    std::println("Hello Matrix");
}

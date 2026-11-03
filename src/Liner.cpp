#include "Liner.h"
#include "Matrix.h"

namespace Jidao
{

Vector<3> turn(const Matrix<3, 3>& turn_way, Vector<3> origin)
{
    return turn_way*origin;
}


Vector<2> get_pos(Vector<3> origin, float d)
{
    auto [x,y,z] = origin.get();
    return Vector<2>{x/(1-z/d), y/(1-z/d)};
}


Vector<2> map(Vector<2> pos, int w, int h)
{
    float side_length = w<h?w:h;
    Matrix<2, 2> change_op = {
        {side_length/2, 0},
        {0, side_length/4}
    };
    auto new_pos = change_op*pos;
    new_pos += {static_cast<float>(w)/2, static_cast<float>(h)/2};
    return new_pos;
}

// end of namespace Jidao
}

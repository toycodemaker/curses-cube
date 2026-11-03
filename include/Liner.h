#ifndef LINER_H_
#define LINER_H_

#include "Matrix.h"

namespace Jidao
{

Vector<3> turn(const Matrix<3, 3>& trun_way, Vector<3> origin);
Vector<2> get_pos(Vector<3> origin, float d);

Vector<2> map(Vector<2> pos, int w, int h);

// end of namespace Jidao
}

#endif

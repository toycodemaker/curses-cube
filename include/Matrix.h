#ifndef JIDAO_MATRIX_H_
#define JIDAO_MATRIX_H_

#include <array>
#include <cassert>
#include <initializer_list>
namespace Jidao 
{


template <int row, int col> requires (row>0&&col>0)
class Matrix
{
public:
    template <int r, int c> requires (r>0&&c>0)
    friend class Matrix;

    Matrix() {
        for (int i=0; i<row; i++)
            for (int j=0; j<col; j++)
                matrix[i][j] = 0;
    }
    Matrix(std::initializer_list<std::initializer_list<int>> rhv) requires(col!=1) {
        assert(rhv.size() == row);
        for (const auto& i:rhv) {
            assert(i.size() == col);
        }
        int i = 0;
        for (const auto& r : rhv) {
            assert(r.size() == col);
            int j = 0;
            for (int v : r) matrix[i][j++] = v;
            ++i;
        }
    }
    Matrix(std::initializer_list<int> rhv)requires(col==1) {
        assert(rhv.size() == row);
        auto b = rhv.begin();
        for (auto& i:matrix) {
            i[0] = *(b++);
        }
    }

    Matrix(const Matrix& rhv) {
        matrix = rhv.matrix;
    }
    Matrix& operator=(const Matrix& rhv) {
        matrix = rhv.matrix;
        return *this;
    }

    Matrix& operator+=(const Matrix& rhv) {
        for (int i=0; i<row; i++)
            for (int j=0; j<col; j++)
                matrix[i][j] += rhv.matrix[i][j];
        return *this;
    }
    Matrix& operator-=(const Matrix& rhv) {
        for (int i=0; i<row; i++)
            for (int j=0; j<col; j++)
                matrix[i][j] -= rhv.matrix[i][j];
        return *this;
    }
    Matrix& operator*=(int value) {
        for (int i=0; i<row; i++)
            for (int j=0; j<col; j++)
                matrix[i][j] *= value;
        return *this;
    }
    template <int r, int c> requires (r==col&&c>=0)
    Matrix<row, c> operator*(const Matrix<r,c>& rhv)const {
        Matrix<row, c> result;
        for (int i=0; i<row; i++)
            for (int j=0; j<c; j++) {
                for (int a=0; a<col; a++)
                    result.matrix[i][j] += matrix[i][a]*rhv.matrix[a][j];
            }
        return result;
    }

    int get(int x, int y) const {
        return matrix[x-1][y-1];
    }
    std::array<int, row> get() const requires(col==1) {
        std::array<int, row> vec;
        auto b = vec.begin();
        for (const auto& i:matrix)
            *(b++) = i[0];
        return vec;
    }
    
private:
    std::array<std::array<int, col>, row> matrix;
};

template <int d>
using Vector = Matrix<d, 1>;


// end of namespace Jidao
}

#endif

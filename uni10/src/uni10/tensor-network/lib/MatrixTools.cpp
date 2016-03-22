/****************************************************************************
*  @file CMakeLists.txt
*  @license
*    Universal Tensor Network Library
*    Copyright (c) 2013-2014
*    National Taiwan University
*    National Tsing-Hua University

*
*    This file is part of Uni10, the Universal Tensor Network Library.
*
*    Uni10 is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Lesser General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    Uni10 is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public License
*    along with Uni10.  If not, see <http://www.gnu.org/licenses/>.
*  @endlicense
*  @brief Implementation file for Matrix class
*  @author Ying-Jer Kao
*  @date 2014-05-06
*  @since 0.1.0
*
*****************************************************************************/
#include <uni10/tools/uni10_tools.h>
#include <uni10/numeric/uni10_lapack.h>
#include <uni10/tensor-network/Matrix.h>

typedef double Real;
typedef std::complex<double> Complex;

namespace uni10{

  void RtoC(Matrix& mat){
    try{
      if(mat.typeID() == 1){
        mat.r_flag = RNULL;
        mat.c_flag = CTYPE;
        mat.cm_elem = (Complex*)elemAlloc(mat.elemNum() * sizeof(Complex), mat.ongpu);
        elemCast(mat.cm_elem, mat.m_elem, mat.elemNum(), mat.ongpu, mat.ongpu);
        if(mat.m_elem != NULL)
          elemFree(mat.m_elem, mat.elemNum() * sizeof(Real), mat.ongpu);
        mat.m_elem = NULL;
      }
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function Block::RtoC(Matrix& ):");
    }
  }

  Matrix takeExp(double a, const Block& mat){
    try{
      return exph(a, mat);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function takeExp(double, uni10::Matrix&):");
      return Matrix();
    }
  }

  Matrix exph(double a, const Block& mat){
    try{
      if(mat.typeID() == 1)
        return  exph(RTYPE, a, mat);
      else if(mat.typeID() == 2)
        return  exph(CTYPE, a, mat);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function exph(double, uni10::Matrix&):");
    }
    return Matrix();
  }

  Matrix exph(rflag tp, double a, const Block& mat){
    try{
      throwTypeError(tp);
      std::vector<Matrix> rets = mat.eigh();
      Matrix UT(rets[1]);
      UT.transpose();
      vectorExp(a, rets[0].getElem(RTYPE), rets[0].row(), rets[0].isOngpu());
      return UT * (rets[0] * rets[1]);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function exph(uni10::rflag, double, uni10::Matrix&):");
      return Matrix();
    }
  }

  Matrix exph(cflag tp, double a, const Block& mat){
    try{
      throwTypeError(tp);
      std::vector<Matrix> rets = mat.eigh();
      Matrix UT(rets[1]);
      UT.cTranspose();
      vectorExp(a, rets[0].getElem(CTYPE), rets[0].row(), rets[0].isOngpu());
      return UT * (rets[0] * rets[1]);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function exph(uni10::cflag, double, uni10::Matrix&):");
    }
    return Matrix();
  }

  Matrix exp(double a, const Block& mat){
    try{
      std::vector<Matrix> rets = mat.eig();
      Matrix Uinv = rets[1].inverse();
      vectorExp(a, rets[0].getElem(CTYPE), rets[0].row(), rets[0].isOngpu());
      return Uinv * (rets[0] * rets[1]);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function exp(double, uni10::Matrix&):");
      return Matrix();
    }
  }

  Matrix exp(const std::complex<double>& a, const Block& mat){
    try{
      std::vector<Matrix> rets = mat.eig();
      Matrix Uinv = rets[1].inverse();
      vectorExp(a, rets[0].getElem(CTYPE), rets[0].row(), rets[0].isOngpu());
      return Uinv * (rets[0] * rets[1]);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In function exp(std::complex<double>, uni10::Matrix&):");
    }
    return Matrix();
  }

  Matrix exp(const Block& mat){
    return exp(1.0, mat);
  }

  Matrix exph(const Block& mat){
    return exph(1.0, mat);
  }

};	/* namespace uni10 */

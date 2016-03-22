/****************************************************************************
*  @file UniTensor.cpp
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
*  @brief Implementation of UniTensor Class
*  @author Yun-Da Hsieh, Ying-Jer Kao
*  @date 2015-03-06
*  @since 0.1.0
*
*****************************************************************************/
#include <uni10/tools/uni10_tools.h>
#include <uni10/numeric/uni10_lapack.h>
#include <uni10/data-structure/uni10_struct.h>
#include <uni10/data-structure/Bond.h>
#include <uni10/tensor-network/Matrix.h>
#include <uni10/tensor-network/UniTensor.h>
#include <uni10/hdf5io/uni10_hdf5io.h>

typedef double Real;
typedef std::complex<double> Complex;

namespace uni10{

int64_t UniTensor::ELEMNUM = 0;
int UniTensor::COUNTER = 0;
size_t UniTensor::MAXELEMNUM = 0;
size_t UniTensor::MAXELEMTEN = 0;

/*********************  OPERATOR **************************/

std::ostream& operator<< (std::ostream& os, const UniTensor& UniT){
  try{
    if(!(UniT.status & UniT.HAVEBOND)){
      if(UniT.ongpu){
        if(UniT.typeID() == 0)
          os<<"This Tensor is EMPTY";
        if(UniT.typeID() == 1)
          os<<"\nScalar: " << getElemAt(0, UniT.elem, UniT.ongpu);
        if(UniT.typeID() == 2)
          os<<"\nScalar: " << getElemAt(0, UniT.c_elem, UniT.ongpu);
        os<<", onGPU";
      }
      else{
        if(UniT.typeID() == 0)
          os<<"This Tensor is EMPTY";
        if(UniT.typeID() == 1)
          os<<"\nScalar: " << UniT.elem[0];
        if(UniT.typeID() == 2)
          os<<"\nScalar: " << UniT.c_elem[0];
      }
      os<<"\n\n";
      return os;
    }
    int row = 0;
    int col = 0;
    std::vector<Bond>bonds = UniT.bond();
    for(size_t i = 0; i < bonds.size(); i++)
      if(bonds[i].type() == BD_IN)
        row++;
      else
        col++;
    int layer = std::max(row, col);
    int nmlen = UniT.name.length() + 2;
    int star = 12 + (14 - nmlen) / 2;
    for(int s = 0; s < star; s++)
      os << "*";
    if(UniT.name.length() > 0)
      os << " " << UniT.name << " ";
    for(int s = 0; s < star; s++)
      os<<"*";
    os<<std::endl;
    if(UniT.typeID() == 0)
      os << "EMPTY" << std::endl;
    if(UniT.typeID() == 1)
      os << "REAL" << std::endl;
    if(UniT.typeID() == 2)
      os << "COMPLEX" << std::endl;
    if(UniT.ongpu)
      os<<"\n                 onGPU";
    os << "\n             ____________\n";
    os << "            |            |\n";
    int llab = 0;
    int rlab = 0;
    char buf[128];
    for(int l = 0; l < layer; l++){
      if(l < row && l < col){
        llab = UniT.labels[l];
        rlab = UniT.labels[row + l];
        sprintf(buf, "    %5d___|%-4d    %4d|___%-5d\n", llab, bonds[l].dim(), bonds[row + l].dim(), rlab);
        os<<buf;
      }
      else if(l < row){
        llab = UniT.labels[l];
        sprintf(buf, "    %5d___|%-4d    %4s|\n", llab, bonds[l].dim(), "");
        os<<buf;
      }
      else if(l < col){
        rlab = UniT.labels[row + l];
        sprintf(buf, "    %5s   |%4s    %4d|___%-5d\n", "", "", bonds[row + l].dim(), rlab);
        os << buf;
      }
      os << "            |            |   \n";
    }
    os << "            |____________|\n";

    os << "\n================BONDS===============\n";
    for(size_t b = 0; b < bonds.size(); b++){
      os << bonds[b];
    }
    os<<"\n===============BlockS===============\n";
    std::map<Qnum, Block> blocks = UniT.const_getBlocks();
    bool printElem = true;
    for (std::map<Qnum, Block>::const_iterator  it = blocks.begin() ; it != blocks.end(); it++ ){
      os << "--- " << it->first << ": ";// << Rnum << " x " << Cnum << " = " << Rnum * Cnum << " ---\n\n";
      if((UniT.status & UniT.HAVEELEM) && printElem)
        os<<it->second;
      else
        os<<it->second.row() << " x "<<it->second.col()<<": "<<it->second.elemNum()<<std::endl<<std::endl;
    }
    os << "Total elemNum: "<<UniT.m_elemNum<<std::endl;
    os << "***************** END ****************\n\n";
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function operator<<(std::ostream&, uni10::UniTensor&):");
  }
  return os;
}

UniTensor& UniTensor::operator*=(const UniTensor& uT){
  try{
    *this = *this * uT;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator*=(uni10::UniTensor&):");
  }
  return *this;
}

UniTensor& UniTensor::operator*= (double a){
  try{
    if(!(status & HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform scalar multiplication on a tensor before setting its elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    *this = *this * a;
//    vectorScal(a, elem, m_elemNum, ongpu);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator*=(double):");
  }
  return *this;
}

UniTensor& UniTensor::operator*= (Complex a){
  try{
    if(!(status & HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform scalar multiplication on a tensor before setting its elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    *this = *this * a;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator*=(std::complex<double>):");
  }
  return *this;
}

UniTensor operator*(const std::complex<double>& a, const UniTensor& Ta){
  try{
    if(a.imag() == 0)
      return a.real()*Ta;
    if(!(Ta.status & Ta.HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform scalar multiplication on a tensor before setting its elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    UniTensor Tb(Ta);
    if(Tb.typeID() == 1)
      RtoC(Tb);
    vectorScal(a, Tb.c_elem, Tb.m_elemNum, Tb.ongpu);
    return Tb;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function operator*(uni10::UniTensor&, complex<double>&):");
    return UniTensor();
  }
}

UniTensor operator*(const UniTensor& Ta, const std::complex<double>& a){return a * Ta;};

UniTensor operator*(const UniTensor& Ta, double a){
  try{
    if(!(Ta.status & Ta.HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform scalar multiplication on a tensor before setting its elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    UniTensor Tb(Ta);
    if(Tb.typeID() == 1)
      vectorScal(a, Tb.elem, Tb.m_elemNum, Tb.ongpu);
    else if(Tb.typeID() == 2)
      vectorScal(a, Tb.c_elem, Tb.m_elemNum, Tb.ongpu);
    return Tb;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function operator*(uni10::UniTensor&, double):");
    return UniTensor();
  }
}

UniTensor operator*(double a, const UniTensor& Ta){return Ta * a;};

UniTensor operator*(const UniTensor& Ta, const UniTensor& Tb){
  try{
    UniTensor cTa = Ta;
    UniTensor cTb = Tb;
    return contract(cTa, cTb, true);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function operator*(uni10::UniTensor&, uni10::UniTensor&):");
    return UniTensor();
  }
}

UniTensor& UniTensor::operator=(const UniTensor& UniT){ //GPU
  try{

    r_flag = UniT.r_flag;
    c_flag = UniT.c_flag;
    bonds = UniT.bonds;
    blocks = UniT.blocks;
    labels = UniT.labels;
    name = UniT.name;
    RBondNum = UniT.RBondNum;
    RQdim = UniT.RQdim;
    CQdim = UniT.CQdim;
    QidxEnc = UniT.QidxEnc;
    RQidx2Off = UniT.RQidx2Off;
    CQidx2Off = UniT.CQidx2Off;
    RQidx2Dim = UniT.RQidx2Dim;
    CQidx2Dim = UniT.CQidx2Dim;
    RQidx2Blk = UniT.RQidx2Blk;

    ELEMNUM -= m_elemNum;	//free original memory


    TelemFree();

    elem = NULL;
    c_elem = NULL;
    status = UniT.status;
    m_elemNum = UniT.m_elemNum;


    std::map<Qnum, Block>::const_iterator it2;
    std::map< const Block* , Block*> blkmap;

    if(typeID() == 0){
      for (std::map<Qnum, Block>::iterator it = blocks.begin() ; it != blocks.end(); it++ ){
        it2 = UniT.blocks.find(it->first);
        blkmap[(&(it2->second))] = &(it->second);
      }
    }else if(typeID() == 1){
      TelemAlloc(RTYPE);
      for (std::map<Qnum, Block>::iterator it = blocks.begin(); it != blocks.end(); it++ ){ // blocks here is UniT.blocks
        it->second.m_elem = &(elem[it->second.m_elem - UniT.elem]);

        it2 = UniT.blocks.find(it->first);
        blkmap[&(it2->second)] = &(it->second);
      }
    }else if(typeID() == 2){
      TelemAlloc(CTYPE);
      for (std::map<Qnum, Block>::iterator it = blocks.begin(); it != blocks.end(); it++ ){ // blocks here is UniT.blocks
        it->second.cm_elem = &(c_elem[it->second.cm_elem - UniT.c_elem]);

        it2 = UniT.blocks.find(it->first);
        blkmap[&(it2->second)] = &(it->second);
      }
    }

    if(UniT.status & HAVEBOND){
      for(std::map<int, Block*>::iterator it = RQidx2Blk.begin(); it != RQidx2Blk.end(); it++)
        it->second = blkmap[it->second];
    }

    ELEMNUM += m_elemNum;
    if(ELEMNUM > MAXELEMNUM)
      MAXELEMNUM = ELEMNUM;
    if(m_elemNum > MAXELEMTEN)
      MAXELEMTEN = m_elemNum;

    if(typeID() == 1)
      elemCopy(elem, UniT.elem, sizeof(Real) * UniT.m_elemNum, ongpu, UniT.ongpu);
    else if(typeID() == 2)
      elemCopy(c_elem, UniT.c_elem, sizeof(Complex) * UniT.m_elemNum, ongpu, UniT.ongpu);

  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator=(uni10::UniTensor&):");
  }
  return *this;
}

UniTensor operator+(const UniTensor& _Ta, const UniTensor& _Tb){
  try{

    UniTensor Ta(_Ta);
    UniTensor Tb(_Tb);
    if(Ta.typeID() == 1 && Tb.typeID() == 2)
      RtoC(Ta);
    if(Ta.typeID() == 2 && Tb.typeID() == 1)
      RtoC(Tb);
    if(!(Ta.status & Tb.status & Ta.HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform addition of tensors before setting their elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    if(!(Ta.bonds == Tb.bonds)){
      std::ostringstream err;
      err<<"Cannot perform addition of two tensors having diffirent bonds.";
      throw std::runtime_error(exception_msg(err.str()));
    }

    UniTensor Tc(Ta);
    Tc.typeID() == 1 ? vectorAdd(Tc.elem, Tb.elem, Tc.m_elemNum, Tc.ongpu, Tb.ongpu) :vectorAdd(Tc.c_elem, Tb.c_elem, Tc.m_elemNum, Tc.ongpu, Tb.ongpu);
    return Tc;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function operator+(uni10::UniTensor&, uni10::UniTensor&):");
    return UniTensor();
  }
}

UniTensor& UniTensor::operator+= (const UniTensor& _Tb){
  try{

    UniTensor Tb(_Tb);
    if(typeID() == 1 && Tb.typeID() == 2)
      RtoC(*this);
    if(typeID() == 2 && Tb.typeID() == 1)
      RtoC(Tb);

    if(!(status & Tb.status & HAVEELEM)){
      std::ostringstream err;
      err<<"Cannot perform addition of tensors before setting their elements.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    if(!(bonds == Tb.bonds)){
      std::ostringstream err;
      err<<"Cannot perform addition of two tensors having different bonds.";
      throw std::runtime_error(exception_msg(err.str()));
    }

    typeID() == 1 ? vectorAdd(elem, Tb.elem, m_elemNum, ongpu, Tb.ongpu) : vectorAdd(c_elem, Tb.c_elem, m_elemNum, ongpu, Tb.ongpu);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator+=(uni10::UniTensor&):");
  }
  return *this;
}

Matrix otimes(const Block& Ma, const Block& Mb){
  try{
    UniTensor Ta(Ma);
    UniTensor Tb(Mb);
    return otimes(Ta, Tb).getBlock();
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function otimes(uni10::Matrix&, uni10::Matrix&):");
    return Matrix();
  }
}

UniTensor::UniTensor(): status(0){
  try{
    initUniT(RTYPE);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor():");
  }
}


UniTensor::UniTensor(const std::vector<Bond>& _bonds, const std::string& _name): name(_name), status(0), bonds(_bonds){
  try{
    initUniT(RTYPE);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::vector<Bond>&, std::string& = \"\"):");
  }
}

UniTensor::UniTensor(const std::string stp, const std::vector<Bond>& _bonds, const std::string& _name): name(_name), status(0), bonds(_bonds){
  try{
    if(stp == "R")
      initUniT(RTYPE);
    else if(stp == "C")
      initUniT(CTYPE);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::vector<Bond>&, std::string& = \"\"):");
  }
}

UniTensor::UniTensor(const std::vector<Bond>& _bonds, std::vector<int>& _labels, const std::string& _name): name(_name), status(0), bonds(_bonds){
  try{
    initUniT(RTYPE);
    setLabel(_labels);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::vector<Bond>&, std::vector<int>&, std::string& = \"\"):");
  }
}

UniTensor::UniTensor(const std::vector<Bond>& _bonds, int* _labels, const std::string& _name): name(_name), status(0), bonds(_bonds){
  try{
    initUniT(RTYPE);
    setLabel(_labels);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::vector<Bond>&, int*, std::string& = \"\"):");
  }
}

UniTensor::UniTensor(const UniTensor& UniT): //GPU
  r_flag(UniT.r_flag), c_flag(UniT.c_flag),name(UniT.name), elem(NULL), c_elem(NULL), status(UniT.status), 
bonds(UniT.bonds), blocks(UniT.blocks), labels(UniT.labels), \
    RBondNum(UniT.RBondNum), RQdim(UniT.RQdim), CQdim(UniT.CQdim), m_elemNum(UniT.m_elemNum), RQidx2Blk(UniT.RQidx2Blk), QidxEnc(UniT.QidxEnc),  RQidx2Off(UniT.RQidx2Off), CQidx2Off(UniT.CQidx2Off), RQidx2Dim(UniT.RQidx2Dim), CQidx2Dim(UniT.CQidx2Dim){
    try{

      std::map<Qnum, Block>::const_iterator it2;
      std::map<const Block* , Block*> blkmap;

      if(typeID() == 0){
        for (std::map<Qnum, Block>::iterator it = blocks.begin() ; it != blocks.end(); it++ ){
          it2 = UniT.blocks.find(it->first);
          blkmap[(&(it2->second))] = &(it->second);
        }
      }

      if(typeID() == 1){
        TelemAlloc(RTYPE);
        for (std::map<Qnum, Block>::iterator it = blocks.begin() ; it != blocks.end(); it++ ){
          it->second.m_elem = &(elem[(it->second.m_elem - UniT.elem)]);
          it2 = UniT.blocks.find(it->first);
          blkmap[(&(it2->second))] = &(it->second);
        }
      }

      if(typeID() == 2){
        TelemAlloc(CTYPE);
        for (std::map<Qnum, Block>::iterator it = blocks.begin() ; it != blocks.end(); it++ ){
          it->second.cm_elem = &(c_elem[(it->second.cm_elem - UniT.c_elem)]);

          it2 = UniT.blocks.find(it->first);
          blkmap[(&(it2->second))] = &(it->second);
        }
      }

      if(UniT.status & HAVEBOND){
        for(std::map<int, Block*>::iterator it = RQidx2Blk.begin(); it != RQidx2Blk.end(); it++)
          it->second = blkmap[it->second];
      }

      ELEMNUM += m_elemNum;
      COUNTER++;
      if(ELEMNUM > MAXELEMNUM)
        MAXELEMNUM = ELEMNUM;
      if(m_elemNum > MAXELEMTEN)
        MAXELEMTEN = m_elemNum;

      if(typeID() == 1)
        elemCopy(elem, UniT.elem, sizeof(Real) * UniT.m_elemNum, ongpu, UniT.ongpu);
      if(typeID() == 2)
        elemCopy(c_elem, UniT.c_elem, sizeof(Complex) * UniT.m_elemNum, ongpu, UniT.ongpu);
    }
    catch(const std::exception& e){
      propogate_exception(e, "In copy constructor UniTensor::UniTensor(uni10::UniTensor&):");
    }
  }

UniTensor::UniTensor(const Block& blk): status(0){
  try{
    Bond bdi(BD_IN, blk.Rnum);
    Bond bdo(BD_OUT, blk.Cnum);
    bonds.push_back(bdi);
    bonds.push_back(bdo);
    initUniT(blk.typeID());
    this->putBlock(blk);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(uni10::Block&");
  }
}

UniTensor::~UniTensor(){
  TelemFree();
  ELEMNUM -= m_elemNum;
  COUNTER--;
}

UniTensor::UniTensor(const std::string& fname): status(0){ //GPU
  try{
    int namemax = 32;
    if(fname.size() > (size_t)namemax)
      name = fname.substr(fname.size() - namemax);
    else
      name = fname;
    FILE* fp = fopen(fname.c_str(), "r");
    if(!(fp != NULL)){
      std::ostringstream err;
      err<<"Error in opening file '" << fname <<"'.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    fread(&r_flag, 1, sizeof(r_flag), fp);
    fread(&c_flag, 1, sizeof(c_flag), fp);
    int st;
    fread(&st, 1, sizeof(int), fp);
    int bondNum;
    fread(&bondNum, 1, sizeof(bondNum), fp);  //OUT: bondNum(4 bytes)
    size_t qnum_sz;
    fread(&qnum_sz, 1, sizeof(size_t), fp);	//OUT: sizeof(Qnum)
    if(!(qnum_sz == sizeof(Qnum))){
      std::ostringstream err;
      err<<"Error in reading file '"<<fname<<"' in.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    for(int b = 0; b < bondNum; b++){
      int num_q;
      bondType tp;
      fread(&tp, 1, sizeof(bondType), fp);	//OUT: Number of Qnums in the bond(4 bytes)
      fread(&num_q, 1, sizeof(int), fp);	//OUT: Number of Qnums in the bond(4 bytes)
      Qnum q0;
      std::vector<Qnum> qnums(num_q, q0);
      fread(&(qnums[0]), num_q, qnum_sz, fp);
      std::vector<int> qdegs(num_q, 0);
      fread(&(qdegs[0]), num_q, sizeof(int), fp);
      std::vector<Qnum> tot_qnums;
      for(int q = 0; q < num_q; q++)
        for(int d = 0; d < qdegs[q]; d++)
          tot_qnums.push_back(qnums[q]);
      Bond bd(tp, tot_qnums);
      bonds.push_back(bd);
    }
    initUniT(typeID());
    int num_l;
    fread(&num_l, 1, sizeof(int), fp);	//OUT: Number of Labels in the Tensor(4 bytes)
    if(!(num_l == bonds.size())){
      std::ostringstream err;
      err<<"Error in reading file '"<<fname<<"' in.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    labels.assign(num_l, 0);
    fread(&(labels[0]), num_l, sizeof(int), fp);
    if(typeID() == 1){
      if(st & HAVEELEM){
        Real *tmp_elem = elem;
        size_t memsize = m_elemNum * sizeof(Real);
        if(ongpu)
          tmp_elem = (Real*)malloc(memsize);
        size_t num_el;
        fread(&num_el, 1, sizeof(m_elemNum), fp);	//OUT: Number of elements in the Tensor(4 bytes)
        fread(tmp_elem, m_elemNum, sizeof(Real), fp);
        if(ongpu){
          elemCopy(elem, tmp_elem, memsize, ongpu, false);
          free(tmp_elem);
        }
        status |= HAVEELEM;
      }
    }
    if(typeID() == 2){
      if(st & HAVEELEM){
        Complex *tmp_elem = c_elem;
        size_t memsize = m_elemNum * sizeof(Complex);
        if(ongpu)
          tmp_elem = (Complex*)malloc(memsize);
        size_t num_el;
        fread(&num_el, 1, sizeof(m_elemNum), fp);	//OUT: Number of elements in the Tensor(4 bytes)
        fread(tmp_elem, m_elemNum, sizeof(Complex), fp);
        if(ongpu){
          elemCopy(c_elem, tmp_elem, memsize, ongpu, false);
          free(tmp_elem);
        }
        status |= HAVEELEM;
      }
    }
    fclose(fp);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::string&):");
  }
}

UniTensor::UniTensor(const std::string& fname, const bool hdf5): status(0){ //GPU
  try{
    HDF5IO h5f(fname.c_str());
    h5f.loadFlag("Status", "flag", r_flag, c_flag);
    int st = h5f.loadInt("Status", "status");
    int bondNum = h5f.loadInt("Bonds", "bondNum");
    for(int b = 0; b < bondNum; b++){
      std::string gname = "Bonds/bond-";
      gname.append(std::to_string((unsigned long long)b));
      int num_q = h5f.loadInt(gname, "numQnums");
      bondType tp;
      h5f.loadBond(gname, "bondType", tp);
      std::vector<Qnum> qnums;
      for (size_t cnt_q = 0; cnt_q < num_q; cnt_q++) {
        std::string setname = "Qnum-";
        setname.append(std::to_string((unsigned long long)cnt_q));
        int m_U1;
        parityType m_prt;
        parityFType m_prtF;
        h5f.loadQnum(gname, setname, m_U1, m_prt, m_prtF);
        qnums.push_back(Qnum(m_prtF, m_U1, m_prt));
      }
      std::vector<int> qdegs;
      h5f.loadStdVector(gname, "degs", qdegs);
      std::vector<Qnum> tot_qnums;
      for(int q = 0; q < num_q; q++)
        for(int d = 0; d < qdegs[q]; d++)
          tot_qnums.push_back(qnums[q]);
      Bond bd(tp, tot_qnums);
      bonds.push_back(bd);
    }
    initUniT(typeID());// from rflag and cflag
    int num_l = h5f.loadInt("Bonds", "num_label");
    if(!(num_l == bonds.size())){
      std::ostringstream err;
      err<<"Error in reading file '"<<fname<<"' in.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    labels.assign(num_l, 0);
    h5f.loadStdVector("Bonds", "labels", labels);
    if(typeID() == 1){
      if(st & HAVEELEM){
        double* work;
        h5f.loadRawBuffer("Block", "m_elem", m_elemNum, work);
        setElem(work, ongpu);
        status |= HAVEELEM;
      }
    }
    if(typeID() == 2){
      if(st & HAVEELEM){
        Complex* work;
        h5f.loadRawBuffer("Block", "m_elem", m_elemNum, work);
        setElem(work, ongpu);
        status |= HAVEELEM;
      }
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::UniTensor(std::string&, bool):");
  }
}


int UniTensor::typeID()const{
  return r_flag + c_flag;
}

void UniTensor::setLabel(const int newLabel, const size_t idx){
  try{
    if(labels.size() <= idx){
      throw std::runtime_error(exception_msg("The bond index is out  of the range of vector(labels)."));
    }
    labels[idx] = newLabel;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::setLabel(std::vector<int>&):");
  }
}

void UniTensor::setLabel(const std::vector<int>& newLabels){
  try{
    std::set<int> labelS(newLabels.begin(), newLabels.end());
    if(!(bonds.size() == labelS.size())){
      throw std::runtime_error(exception_msg("The size of input vector(labels) does not match for the number of bonds."));
    }
    labels = newLabels;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::setLabel(std::vector<int>&):");
  }
}

void UniTensor::setLabel(int* newLabels){
  try{
    std::vector<int> labels(newLabels, newLabels + bonds.size());
    setLabel(labels);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::setLabel(int*):");
  }
}

std::vector<int> UniTensor::label()const{
  return labels;
}

int UniTensor::label(size_t idx)const{
  try{
    if(!(idx < labels.size())){
      std::ostringstream err;
      err<<"Index exceeds the number of the bonds("<<bonds.size()<<").";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::label(size_t):");
  }
  return labels[idx];
}

std::string UniTensor::getName() const{
  return name;
}

void UniTensor::setName(const std::string& _name){
  name = _name;
}

size_t UniTensor::bondNum()const{return bonds.size();}

size_t UniTensor::inBondNum()const{return RBondNum;}

std::vector<Bond> UniTensor::bond()const{
  return bonds;
}

Bond UniTensor::bond(size_t idx)const{
  try{
    if(!(idx < bonds.size())){
      std::ostringstream err;
      err<<"Index exceeds the number of the bonds("<<bonds.size()<<").";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::bond(size_t):");
  }
  return bonds[idx];
}

size_t UniTensor::elemNum()const{return m_elemNum;}

size_t UniTensor::blockNum()const{
	return blocks.size();
}

std::vector<Qnum> UniTensor::blockQnum()const{
  std::vector<Qnum> keys;
  for(std::map<Qnum, Block>::const_iterator it = blocks.begin(); it != blocks.end(); it++)
    keys.push_back(it->first);
  return keys;
}

Qnum UniTensor::blockQnum(size_t idx)const{
  try{
    if(!(idx < blocks.size())){
      std::ostringstream err;
      err<<"Index exceeds the number of the blocks("<<blocks.size()<<").";
      throw std::runtime_error(exception_msg(err.str()));
    }
    for(std::map<Qnum, Block>::const_iterator it = blocks.begin(); it != blocks.end(); it++){
      if(idx == 0)
        return it->first;
      idx--;
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::blockQnum(size_t):");
  }
  return Qnum(0);
}
const std::map<Qnum, Block>& UniTensor::const_getBlocks()const{
  return blocks;
}

const Block& UniTensor::const_getBlock()const{
  try{
    Qnum q0(0);
    return const_getBlock(q0);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getBlock(bool=false):");
    return blocks.end()->second;
  }
}

const Block& UniTensor::const_getBlock(const Qnum& qnum)const{
  try{
    std::map<Qnum, Block>::const_iterator it = blocks.find(qnum);
    if(it == blocks.end()){
      std::ostringstream err;
      err<<"There is no block with the given quantum number "<<qnum;
      throw std::runtime_error(exception_msg(err.str()));
    }
    return it->second;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getBlock(uni10::Qnum&):");
    return blocks.end()->second;
  }
}


std::map<Qnum, Matrix> UniTensor::getBlocks()const{
  try{
    if(typeID() == 1)
      return getBlocks(RTYPE);
    else if(typeID() == 2)
      return getBlocks(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"This tensor is EMPTY ";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getBlocks():");
  }
  return std::map<Qnum, Matrix>();
}

Matrix UniTensor::getBlock(bool diag)const{
  try{
    if(typeID() == 1)
      return getBlock(RTYPE, diag);
    else if(typeID() == 2)
      return getBlock(CTYPE, diag);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"This tensor is EMPTY ";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getBlock(bool=false):");
    return Matrix();
  }
  return Matrix();
}

Matrix UniTensor::getBlock(const Qnum& qnum, bool diag)const{
  try{
    if(typeID() == 1)
      return getBlock(RTYPE, qnum, diag);
    else if(typeID() == 2)
      return getBlock(CTYPE, qnum, diag);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"This tensor is EMPTY ";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getBlock(uni10::Qnum&):");
    return Matrix(0, 0);
  }
  return Matrix();
}

void UniTensor::set_zero(){
  try{
    if(typeID() == 1)
      set_zero(RTYPE);
    else if(typeID() == 2)
      set_zero(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set zeros in EMPTY tensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::set_zero():");
  }
}

void UniTensor::set_zero(const Qnum& qnum){
  try{
    if(typeID() == 1)
      set_zero(RTYPE, qnum);
    else if(typeID() == 2)
      set_zero(CTYPE, qnum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set zeros in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::set_zero(std::Qnum&):");
  }
}

void UniTensor::identity(){
  try{
    if(typeID() == 1)
      identity(RTYPE);
    else if(typeID() == 2)
      identity(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set identity in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::identity():");
  }
}

void UniTensor::identity(const Qnum& qnum){
  try{
    if(typeID() == 1)
      identity(RTYPE, qnum);
    else if(typeID() == 2)
      identity(CTYPE, qnum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set identity in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::identity(std::Qnum&):");
  }
}

void UniTensor::randomize(){
  try{
    if(typeID() == 1)
      randomize(RTYPE);
    else if(typeID() == 2)
      randomize(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set randomize in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::randomize():");
  }
}

void UniTensor::orthoRand(){
  try{
    if(typeID() == 1)
      orthoRand(RTYPE);
    else if(typeID() == 2)
      orthoRand(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set orthoRand in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::orthoRand():");
  }
}

void UniTensor::orthoRand(const Qnum& qnum){
  try{
    if(typeID() == 1)
      orthoRand(RTYPE, qnum);
    else if(typeID() == 2)
      orthoRand(CTYPE, qnum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set orthoRand in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::orthoRand(std::Qnum&):");
  }
}

Real UniTensor::norm() const{
  try{
    Real norm = 0;
    std::map<Qnum, Block>::const_iterator it = blocks.begin();
    if(blocks.size() == 0){
      std::ostringstream err;
      err<<"There is no block in this tensor ";
      throw std::runtime_error(exception_msg(err.str()));
    }
    for(; it != blocks.end(); it++)
      norm += pow(it->second.norm(),2);
    return sqrt(norm);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::norm():");
  }
  return 0.;
}

void UniTensor::normalize(){
  Real norm = this->norm();
  *this *= (1./norm);
}

void UniTensor::save(const std::string& fname){
  try{
    if((status & HAVEBOND) == 0){   //If not INIT, NO NEED to write out to file
      throw std::runtime_error(exception_msg("Saving a tensor without bonds(scalar) is not supported."));
    }
    FILE* fp = fopen(fname.c_str(), "w");
    if(!(fp != NULL)){
      std::ostringstream err;
      err<<"Error in writing to file '"<<fname<<"'.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    fwrite(&r_flag, 1, sizeof(r_flag), fp);
    fwrite(&c_flag, 1, sizeof(c_flag), fp);
    fwrite(&status, 1, sizeof(status), fp);	//OUT: status(4 bytes)
    int bondNum = bonds.size();
    fwrite(&bondNum, 1, sizeof(bondNum), fp);  //OUT: bondNum(4 bytes)
    size_t qnum_sz = sizeof(Qnum);
    fwrite(&qnum_sz, 1, sizeof(size_t), fp);	//OUT: sizeof(Qnum)
    for(int b = 0; b < bondNum; b++){
      int num_q = bonds[b].Qnums.size();
      fwrite(&(bonds[b].m_type), 1, sizeof(bondType), fp);	//OUT: Number of Qnums in the bond(4 bytes)
      fwrite(&num_q, 1, sizeof(int), fp);	//OUT: Number of Qnums in the bond(4 bytes)
      fwrite(&(bonds[b].Qnums[0]), num_q, qnum_sz, fp);
      fwrite(&(bonds[b].Qdegs[0]), num_q, sizeof(int), fp);
    }
    int num_l = labels.size();
    fwrite(&num_l, 1, sizeof(int), fp);	//OUT: Number of Labels in the Tensor(4 bytes)
    fwrite(&(labels[0]), num_l, sizeof(int), fp);
    if(typeID() == 1){
      if(status & HAVEELEM){
        fwrite(&m_elemNum, 1, sizeof(m_elemNum), fp);	//OUT: Number of elements in the Tensor(4 bytes)
        size_t memsize = m_elemNum * sizeof(Real);
        Real* tmp_elem = elem;
        if(ongpu){
          tmp_elem = (Real*)malloc(memsize);
          elemCopy(tmp_elem, elem, memsize, false, ongpu);
        }
        fwrite(tmp_elem, m_elemNum, sizeof(Real), fp);
        if(ongpu)
          free(tmp_elem);
      }
    }
    if(typeID() == 2){
      if(status & HAVEELEM){
        fwrite(&m_elemNum, 1, sizeof(m_elemNum), fp);	//OUT: Number of elements in the Tensor(4 bytes)
        size_t memsize = m_elemNum * sizeof(Complex);
        Complex* tmp_elem = c_elem;
        if(ongpu){
          tmp_elem = (Complex*)malloc(memsize);
          elemCopy(tmp_elem, c_elem, memsize, false, ongpu);
        }
        fwrite(tmp_elem, m_elemNum, sizeof(Complex), fp);
        if(ongpu)
          free(tmp_elem);
      }
    }
    fclose(fp);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::save(std::string&):");
  }
}

void UniTensor::h5save(const std::string& fname){
  try{
    if((status & HAVEBOND) == 0){   //If not INIT, NO NEED to write out to file
      throw std::runtime_error(exception_msg("Saving a tensor without bonds(scalar) is not supported."));
    }
    HDF5IO h5f(fname.c_str());
    h5f.saveFlag("Status", "flag", r_flag, c_flag);
    h5f.saveNumber("Status", "status", status);
    int bondNum = bonds.size();
    h5f.saveNumber("Bonds", "bondNum", bondNum);
    for(int b = 0; b < bondNum; b++){
      int num_q = bonds[b].Qnums.size();
      std::string gname = "Bonds/bond-";
      gname.append(std::to_string((unsigned long long)b));
      h5f.saveNumber(gname, "numQnums", num_q);
      h5f.saveBond(gname, "bondType", bonds[b].m_type);
      h5f.saveStdVector(gname, "degs", bonds[b].Qdegs);
      for (size_t cnt_q = 0; cnt_q < num_q; cnt_q++) {
          std::string setname = "Qnum-";
          setname.append(std::to_string((unsigned long long)cnt_q));
          h5f.saveQnum(gname, setname, bonds[b].Qnums[cnt_q].U1(), bonds[b].Qnums[cnt_q].prt(), bonds[b].Qnums[cnt_q].prtF());
      }
    }
    int num_l = labels.size();
    h5f.saveNumber("Bonds", "num_label", num_l);
    h5f.saveStdVector("Bonds", "labels", labels);
    if(typeID() == 1){
        if(status & HAVEELEM){
            h5f.saveRawBuffer("Block", "m_elem", m_elemNum, elem);
        }
    }
    if(typeID() == 2){
        if(status & HAVEELEM){
            h5f.saveRawBuffer("Block", "m_elem", m_elemNum, c_elem);
        }
    }
  } catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::save(std::string&):");
  }
}

UniTensor& UniTensor::transpose(){
  try{
    if(typeID() == 1)
      return transpose(RTYPE);
    else if(typeID() == 2)
      return transpose(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't set orthoRand in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::transpose():");
  }
  return *this;
}

UniTensor& UniTensor::permute(int rowBondNum){
  try{
    if(typeID() == 1)
      return permute(RTYPE, rowBondNum);
    else if(typeID() == 2)
      return permute(CTYPE, rowBondNum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't permute in EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::permute(int):");
  }
  return *this;
}


UniTensor& UniTensor::permute(int* newLabels, int rowBondNum){
  try{
    if(typeID() == 1)
      return permute(RTYPE, newLabels, rowBondNum);
    else if(typeID() == 2)
      return permute(CTYPE, newLabels, rowBondNum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't permute an EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::permute(int*, int):");
  }
	return *this;
}

UniTensor& UniTensor::permute(const std::vector<int>& newLabels, int rowBondNum){
  try{
    if(typeID() == 1)
      return permute(RTYPE, newLabels, rowBondNum);
    else if(typeID() == 2)
      return permute(CTYPE, newLabels, rowBondNum);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't permute an EMPTY block";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::permute(std::vector<int>&, int):");
  }
  return *this;
}

UniTensor contract(UniTensor& _Ta, UniTensor& _Tb, bool fast){
  try{
    if(_Ta.typeID() == 0 || _Tb.typeID() == 0){
      std::ostringstream err;
      err<<"This tensor is EMPTY ";
      throw std::runtime_error(exception_msg(err.str()));
    }else if(_Ta.typeID() == 1 && _Tb.typeID() == 1)
      return contract(RTYPE, _Ta, _Tb, fast);
    else if(_Ta.typeID() == 2 && _Tb.typeID() == 2)
      return contract(CTYPE, _Ta, _Tb, fast);
    else if(_Ta.typeID() == 1 && _Tb.typeID() == 2){
      UniTensor Ta(_Ta);
      RtoC(Ta);
      return contract(CTYPE, Ta, _Tb, fast);
    }else{
      UniTensor Tb(_Tb);
      RtoC(Tb);
      return contract(CTYPE, _Ta, Tb, fast);
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function contract(uni10::UniTensor&, uni10::UniTensor, bool):");
    return UniTensor();
  }
}

UniTensor otimes(const UniTensor & Ta, const UniTensor& Tb){
  try{
    UniTensor T1 = Ta;
    UniTensor T2 = Tb;
    std::vector<int> label1(T1.bondNum());
    std::vector<int> label2(T2.bondNum());
    for(size_t i = 0; i < T1.bondNum(); i++){
      if(i < T1.inBondNum())
        label1[i] = i;
      else
        label1[i] = T2.inBondNum() + i;
    }
    for(size_t i = 0; i < T2.bondNum(); i++){
      if(i < T2.inBondNum())
        label2[i] = i + T1.inBondNum();
      else
        label2[i] = i + T1.bondNum();
    }
    T1.setLabel(label1);
    T2.setLabel(label2);
    return contract(T1, T2, true);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function otimes(uni10::UniTensor&, uni10::UniTensor&):");
    return UniTensor();
  }
}

std::vector<UniTensor> UniTensor::hosvd(size_t modeNum, size_t fixedNum)const{
  try{
    std::vector<std::map<Qnum, Matrix> > symLs;
    return _hosvd(modeNum, fixedNum, symLs, false);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::hosvd(size_t, size_t = 0):");
    return std::vector<UniTensor>();
  }
}

std::vector<UniTensor> UniTensor::hosvd(size_t modeNum, size_t fixedNum, std::vector<std::map<Qnum, Matrix> >& Ls)const{
  try{
    return _hosvd(modeNum, fixedNum, Ls, true);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::hosvd(size_t, size_t, std::vector<std::map<uni10::Qnum, uni10::Matrix> >&):");
    return std::vector<UniTensor>();
  }
}

std::vector<UniTensor> UniTensor::hosvd(size_t modeNum, std::vector<std::map<Qnum, Matrix> >& Ls)const{
  try{
    return _hosvd(modeNum, 0, Ls, true);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::hosvd(size_t, std::vector<std::map<uni10::Qnum, uni10::Matrix> >&):");
    return std::vector<UniTensor>();
  }
}

std::vector<UniTensor> UniTensor::hosvd(size_t modeNum, size_t fixedNum, std::vector<Matrix>& Ls)const{
  try{
    bool withoutSymmetry = true;
    for(size_t b = 0; b < bonds.size(); b++){
      if(bonds[b].Qnums.size() != 1)
        withoutSymmetry = false;
    }
    if(!withoutSymmetry){
      std::ostringstream err;
      err<<"The tensor has symmetry quantum numbers. Cannot use non-symmetry version hosvd(size_t, std::vector<uni10::Matrix>&)";
      err<<"\n  Hint: Use UniTensor::hosvd(size_t, size_t, std::vector<std::map<uni10::Qnum, uni10::Matrix> >&)";
      throw std::runtime_error(exception_msg(err.str()));
    }
    std::vector<std::map<Qnum, Matrix> > symLs;
    const std::vector<UniTensor>& outs = _hosvd(modeNum, fixedNum, symLs, true);
    Ls.clear();
    Qnum q0(0);
    for(size_t i = 0; i < symLs.size(); i++)
      Ls.push_back(symLs[i][q0]);
    return outs;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::hosvd(size_t, size_t, std::vector<Matrix>&):");
    return std::vector<UniTensor>();
  }
}
std::vector<UniTensor> UniTensor::hosvd(size_t modeNum, std::vector<Matrix>& Ls)const{
  try{
    return hosvd(modeNum, 0, Ls);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::hosvd(size_t, std::vector<Matrix>&):");
    return std::vector<UniTensor>();
  }
}

UniTensor& UniTensor::combineBond(const std::vector<int>&cmbLabels){
  try{
    if(typeID() == 1)
      return combineBond(RTYPE, cmbLabels);
    else if(typeID() == 2)
      return combineBond(CTYPE, cmbLabels);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't perform combineBond on and EMPTY tensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::combineBond(std::vector<int>&):");
  }
  return *this;
}

std::string UniTensor::printRawElem(bool print)const{
  try{
    std::ostringstream os;
    if(status & HAVEBOND && status & HAVEELEM){
      int bondNum = bonds.size();
      std::vector<Bond> ins;
      std::vector<Bond> outs;
      for(std::vector<Bond>::const_iterator it = bonds.begin(); it != bonds.end(); ++it){
        if(it->type() == BD_IN)
          ins.push_back(*it);
        else
          outs.push_back(*it);
      }
      if(ins.size() == 0 || outs.size() == 0)
        os<<getRawElem();
      else{
        Bond rBond = combine(ins);
        Bond cBond = combine(outs);
        std::vector<Qnum> rowQ = rBond.Qlist();
        std::vector<Qnum> colQ = cBond.Qlist();
        size_t rowNum = rBond.dim();
        size_t colNum = cBond.dim();
        std::vector<size_t> idxs(bondNum, 0);

        os<< "     ";
        for(size_t q = 0; q < colQ.size(); q++)
          os<< "   " << std::setw(2) << colQ[q].U1() << "," << colQ[q].prt();
        os<< std::endl << std::setw(5) << "" << std::setw(colQ.size() * 7 + 2) <<std::setfill('-')<<"";
        os<<std::setfill(' ');
        int cnt = 0;
        int r = 0;
        int bend;
        while(1){
          if(cnt % colNum == 0){
            os<<"\n    |\n" << std::setw(2) << rowQ[r].U1() << "," << rowQ[r].prt() << "|";
            r++;
          }
          if(typeID() == 1)
            os<< std::setw(7) << std::fixed << std::setprecision(3) << at(idxs);
          else if(typeID() == 2)
            os<< std::setw(7) << std::fixed << std::setprecision(3) << at(CTYPE, idxs);
          for(bend = bondNum - 1; bend >= 0; bend--){
            idxs[bend]++;
            if(idxs[bend] < bonds[bend].dim())
              break;
            else
              idxs[bend] = 0;
          }
          cnt++;
          if(bend < 0)
            break;
        }
        os <<"\n    |\n";
      }
    }
    else if(status & HAVEELEM){
      if(typeID() == 1)
        os<<"\nScalar: " << elem[0]<<"\n\n";
      if(typeID() == 2)
        os<<"\nScalar: " << c_elem[0]<<"\n\n";
    }
    else{
      os<<"NO ELEMENT IN THE TENSOR!!!\n";
    }
    if(print){
      std::cout<<os.str();
      return "";
    }
    return os.str();
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::printRawElem():");
    return "";
  }
}

std::string UniTensor::profile(bool print){
  std::ostringstream os;
  os<<"\n===== Tensor profile =====\n";
	os<<"Existing Tensors: " << COUNTER << std::endl;
	os<<"Allocated Elements: " << ELEMNUM << std::endl;
	os<<"Max Allocated Elements: " << MAXELEMNUM << std::endl;
	os<<"Max Allocated Elements for a Tensor: " << MAXELEMTEN << std::endl;
  os<<"============================\n\n";
  if(print){
    std::cout<<os.str();
    return "";
  }
  return os.str();
}


void UniTensor::setRawElem(const Block& blk){
  try{
    if(blk.typeID() == 1)
      setRawElem(RTYPE, blk);
    else if(blk.typeID() == 2)
      setRawElem(CTYPE, blk);
    else if(blk.typeID() == 0){
      std::ostringstream err;
      err<<"Can't put an EMPTY　block in UniTensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::setRawElem(uni10::Block&):");
  }
}

void UniTensor::putBlock(const Block& mat){
  try{
    if(mat.typeID() == 1)
      putBlock(RTYPE, mat);
    else if(mat.typeID() == 2)
      putBlock(CTYPE, mat);
    else if(mat.typeID() == 0){
      std::ostringstream err;
      err<<"Can't put an EMPTY　block in UniTensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::putBlock(uni10::Block&):");
  }
}

void UniTensor::putBlock(const Qnum& qnum, const Block& mat){
  try{
    std::map<Qnum, Block>::iterator it;
    if(!((it = blocks.find(qnum)) != blocks.end())){
      std::ostringstream err;
      err<<"There is no block with the given quantum number "<<qnum;
      throw std::runtime_error(exception_msg(err.str()));
    }
    if(!(mat.row() == it->second.Rnum && mat.col() == it->second.Cnum)){
      std::ostringstream err;
      err<<"The dimension of input matrix does not match for the dimension of the block with quantum number "<<qnum<<std::endl;
      err<<"  Hint: Use Matrix::resize(int, int)";
      throw std::runtime_error(exception_msg(err.str()));
    }

    if(mat.typeID() == 1) 
      this->putBlock(RTYPE, qnum, mat);
    else if(mat.typeID() == 2) 
      this->putBlock(CTYPE, qnum, mat);

  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::putBlock(uni10::Qnum&, uni10::Block&):");
  }
}

std::vector<_Swap> UniTensor::exSwap(const UniTensor& Tb) const{
  std::vector<_Swap> swaps;
  try{
    if(status & Tb.status & HAVEBOND){
      int bondNumA = labels.size();
      int bondNumB = Tb.labels.size();
      std::vector<int> intersect;
      std::vector<int> left;
      for(int a = 0; a < bondNumA; a++){
        bool found = false;
        for(int b = 0; b < bondNumB; b++)
          if(labels[a] == Tb.labels[b])
            found = true;
        if(found)
          intersect.push_back(a);
        else
          left.push_back(a);
      }
      _Swap sp;
      for(size_t i = 0; i < intersect.size(); i++)
        for(size_t j = 0; j < left.size(); j++){
          sp.b1 = intersect[i];
          sp.b2 = left[j];
          swaps.push_back(sp);
        }
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::exSwap(uni10::UniTensor&):");
  }
  return swaps;
}
void UniTensor::addGate(const std::vector<_Swap>& swaps){
  try{
    if(typeID() == 1)
      addGate(RTYPE, swaps);
    else if(typeID() == 2)
      addGate(CTYPE, swaps);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't addGate on an EMPTY block in UniTensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::addGate(std::vector<_Swap>&):");
  }
}

Complex UniTensor::trace()const{
  try{
    if(typeID() == 1)
      return Complex(trace(RTYPE), 0);
    else if(typeID() == 2)
      return trace(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't perform trace on an EMPTY block in UniTensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::trace():");
    return 0;
  }
  return 0;
}

UniTensor& UniTensor::partialTrace(int la, int lb){
  try{
    if(typeID() == 1)
      return partialTrace(RTYPE, la, lb);
    else if(typeID() == 2)
      return partialTrace(CTYPE, la, lb);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"Can't perform partialTrace on an EMPTY block in UniTensor";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::partialTrace(int, int):");
  }
  return *this;
}

Real UniTensor::operator[](size_t idx)const{
  try{
    if(!(idx < m_elemNum)){
      std::ostringstream err;
      err<<"Index exceeds the number of elements("<<m_elemNum<<").";
      throw std::runtime_error(exception_msg(err.str()));
    }else if(typeID() == 0 || typeID() == 2){
      std::ostringstream err;
      err<<"This Tensor is EMPTY or COMPLEX. If it's COMPLEX, please use UniTensor::at(cflag, const vector<size_t>&) instead";
      throw std::runtime_error(exception_msg(err.str()));
    }
    if(typeID() == 1)
      return getElemAt(idx, elem, ongpu);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator[](size_t):");
  }
  return 0;
}

Complex UniTensor::operator()(size_t idx)const{
  try{
    if(!(idx < m_elemNum)){
      std::ostringstream err;
      err<<"Index exceeds the number of elements("<<m_elemNum<<").";
      throw std::runtime_error(exception_msg(err.str()));
    }else if(typeID() == 0 || typeID() == 1){
      std::ostringstream err;
      err<<"This Tensor is EMPTY or REAL. If it's REAL, please use UniTensor::operator[](size_t) instead";
      throw std::runtime_error(exception_msg(err.str()));
    }
    if(typeID() == 2)
      return getElemAt(idx, c_elem, ongpu);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::operator()(size_t):");
  }
  return 0;
}

Matrix UniTensor::getRawElem()const{
  try{
    if(typeID() == 1)
      return getRawElem(RTYPE);
    else if(typeID() == 2)
      return getRawElem(CTYPE);
    else if(typeID() == 0){
      std::ostringstream err;
      err<<"This Tensor is EMPTY.";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::getRawElem():");
    return Matrix();
  }
  return Matrix();
}

UniTensor& UniTensor::assign(const std::vector<Bond>& _bond){
  try{
    UniTensor T(_bond);
    *this = T;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::assign(std::vector<Bond>&):");
  }
  return *this;
}

bool UniTensor::CelemIsNULL(){
  return c_elem == NULL;
}
bool UniTensor::RelemIsNULL(){
  return elem == NULL;
}

bool UniTensor::similar(const UniTensor& Tb)const{
  try{
    if(bonds.size() != Tb.bonds.size())
      return false;
    for(size_t b = 0; b < bonds.size(); b++){
      if(bonds[b] == Tb.bonds[b]);
      else return false;
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::similar(uni10::UniTensor&):");
  }
  return true;
}

bool UniTensor::elemCmp(const UniTensor& _UniT)const{
  try{
    if(typeID() == 0 || _UniT.typeID() == 0){
      std::ostringstream err;
      err<<"This Tensor is EMPTY.";
      throw std::runtime_error(exception_msg(err.str()));
    }
    UniTensor Ta(*this);
    UniTensor UniT(_UniT);
    if(Ta.typeID() != UniT.typeID())
      Ta.typeID() == 1 ? RtoC(Ta) : RtoC(UniT);

    double diff;
    if(m_elemNum == UniT.m_elemNum){
      if(Ta.typeID() == 1 && UniT.typeID() == 1){
        for(size_t i = 0; i < m_elemNum; i++){
          diff = std::abs(elem[i] - UniT.elem[i]);
          if(diff > 1E-12)
            return false;
        }
      }else{
        for(size_t i = 0; i < m_elemNum; i++){
          diff = std::abs(c_elem[i] - UniT.c_elem[i]);
          if(diff > 1E-12)
            return false;
        }
      }
    }
    else
      return false;
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::elemCmp(uni10::UniTensor&):");
  }
  return true;
}

void UniTensor::clear(){
  status &= ~HAVEELEM;
}

Real UniTensor::at(size_t idx)const{
  try{
    return at(RTYPE, idx);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::at(size_t):");
  }
  return 0;
}

Real UniTensor::at(const std::vector<int>& idxs)const{
  try{
    std::vector<size_t> _idxs(idxs.size());
    for(size_t i = 0; i < idxs.size(); i++)
      _idxs[i] = idxs[i];
    return at(_idxs);
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::at(std::vector<int>&):");
  }
  return 0;
}

Real UniTensor::at(const std::vector<size_t>& idxs)const{
  try{
    if(typeID() == 1)
      return at(RTYPE, idxs);
    else if(typeID() == 0 || typeID() == 2){
      std::ostringstream err;
      err<<"This Tensor is EMPTY or COMPLEX. If it's COMPLEX, please use UniTensor::at(cflag, const vector<size_t>&) instead";
      throw std::runtime_error(exception_msg(err.str()));
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In function UniTensor::at(std::vector<size_t>&):");
  }
  return 0;
}

Real* UniTensor::getElem(){
  return elem;
}

/**************** Private Functions ***********************/

void UniTensor::initUniT(int typeID){
  if(typeID == 1)
    initUniT(RTYPE);
  else if(typeID == 2)
    initUniT(CTYPE);
}

void UniTensor::TelemFree(){
  if(elem != NULL)
    elemFree(elem, sizeof(Real) * m_elemNum, ongpu);
  if(c_elem != NULL)
    elemFree(c_elem, sizeof(Complex) * m_elemNum, ongpu);
}

std::vector<UniTensor> UniTensor::_hosvd(rflag _tp, size_t modeNum, size_t fixedNum, std::vector<std::map<Qnum, Matrix> >& Ls, bool returnL)const{
  if((status & HAVEBOND) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor without bonds(scalar).";
    throw std::runtime_error(exception_msg(err.str()));
  }
  if((status & HAVEELEM) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor before setting its elements.";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int bondNum = bonds.size();
  if((bondNum - fixedNum) % modeNum != 0){
    std::ostringstream err;
    err<<"Bond number cannot be divided by the input mode number("<<modeNum<<").";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int combNum = (bondNum - fixedNum) / modeNum;
  UniTensor T(*this);
  for(size_t t = 0; t < T.labels.size(); t++)
    T.labels[t] = t;
  std::vector<int>ori_labels = T.labels;
  std::vector<int>rsp_labels = T.labels;
  if(returnL)
    Ls.assign(modeNum, std::map<Qnum, Matrix>());
  std::vector<UniTensor> Us;
  UniTensor S(T);
  std::vector<int>out_labels(S.labels.begin(), S.labels.begin() + fixedNum + modeNum);
  for(size_t m = 0; m < modeNum; m++){
    for(size_t l = 0; l < rsp_labels.size(); l++){
      if(l < (size_t)combNum)
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l) % (bondNum - fixedNum))];
      else if(l < combNum + fixedNum)
        rsp_labels[l] = ori_labels[l - combNum];
      else
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l - fixedNum) % (bondNum - fixedNum))];
    }
    T.permute(RTYPE, rsp_labels, combNum);
    std::vector<Bond> bonds(T.bonds.begin(), T.bonds.begin() + combNum);
    bonds.push_back(combine(bonds).dummy_change(BD_OUT));
    Us.push_back(UniTensor(RTYPE, bonds));
    for(std::map<Qnum, Block>::iterator it = T.blocks.begin(); it != T.blocks.end(); it++){
      std::vector<Matrix> svd = it->second.svd(RTYPE);
      Us[m].putBlock(it->first, svd[0]);
      if(returnL)
        Ls[m][it->first] = svd[1];
    }
    for(int c = 0; c < combNum; c++)
      Us[m].labels[c] = fixedNum + m * combNum + c;
    Us[m].labels[combNum] = -m - 1;
    out_labels[fixedNum + m] = -m -1;
    UniTensor UT = Us[m];
    S *= UT.transpose(RTYPE);
  }
  S.permute(RTYPE, out_labels, fixedNum);
  Us.push_back(S);
  return Us;
}

std::vector<UniTensor> UniTensor::_hosvd(cflag tp, size_t modeNum, size_t fixedNum, std::vector<std::map<Qnum, Matrix> >& Ls, bool returnL)const{
  if((status & HAVEBOND) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor without bonds(scalar).";
    throw std::runtime_error(exception_msg(err.str()));
  }
  if((status & HAVEELEM) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor before setting its elements.";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int bondNum = bonds.size();
  if((bondNum - fixedNum) % modeNum != 0){
    std::ostringstream err;
    err<<"Bond number cannot be divided by the input mode number("<<modeNum<<").";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int combNum = (bondNum - fixedNum) / modeNum;
  UniTensor T(*this);
  for(size_t t = 0; t < T.labels.size(); t++)
    T.labels[t] = t;
  std::vector<int>ori_labels = T.labels;
  std::vector<int>rsp_labels = T.labels;
  if(returnL)
    Ls.assign(modeNum, std::map<Qnum, Matrix>());
  std::vector<UniTensor> Us;
  UniTensor S(T);
  std::vector<int>out_labels(S.labels.begin(), S.labels.begin() + fixedNum + modeNum);
  for(size_t m = 0; m < modeNum; m++){
    for(size_t l = 0; l < rsp_labels.size(); l++){
      if(l < (size_t)combNum)
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l) % (bondNum - fixedNum))];
      else if(l < combNum + fixedNum)
        rsp_labels[l] = ori_labels[l - combNum];
      else
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l - fixedNum) % (bondNum - fixedNum))];
    }
    T.permute(CTYPE, rsp_labels, combNum);
    std::vector<Bond> bonds(T.bonds.begin(), T.bonds.begin() + combNum);
    bonds.push_back(combine(bonds).dummy_change(BD_OUT));
    Us.push_back(UniTensor(CTYPE, bonds));
    for(std::map<Qnum, Block>::iterator it = T.blocks.begin(); it != T.blocks.end(); it++){
      std::vector<Matrix> svd = it->second.svd(CTYPE);
      Us[m].putBlock(it->first, svd[0]);
      if(returnL)
        Ls[m][it->first] = svd[1];
    }
    for(int c = 0; c < combNum; c++)
      Us[m].labels[c] = fixedNum + m * combNum + c;
    Us[m].labels[combNum] = -m - 1;
    out_labels[fixedNum + m] = -m -1;
    UniTensor UT = Us[m];
    S *= UT.transpose(CTYPE);
  }
  S.permute(CTYPE, out_labels, fixedNum);
  Us.push_back(S);
  return Us;
}

std::vector<UniTensor> UniTensor::_hosvd(size_t modeNum, size_t fixedNum, std::vector<std::map<Qnum, Matrix> >& Ls, bool returnL)const{
  if((status & HAVEBOND) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor without bonds(scalar).";
    throw std::runtime_error(exception_msg(err.str()));
  }
  if((status & HAVEELEM) == 0){
    std::ostringstream err;
    err<<"Cannot perform higher order SVD on a tensor before setting its elements.";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int bondNum = bonds.size();
  if((bondNum - fixedNum) % modeNum != 0){
    std::ostringstream err;
    err<<"Bond number cannot be divided by the input mode number("<<modeNum<<").";
    throw std::runtime_error(exception_msg(err.str()));
  }
  int combNum = (bondNum - fixedNum) / modeNum;
  UniTensor T(*this);
  for(int t = 0; t < T.labels.size(); t++)
    T.labels[t] = t;
  std::vector<int>ori_labels = T.labels;
  std::vector<int>rsp_labels = T.labels;
  if(returnL)
    Ls.assign(modeNum, std::map<Qnum, Matrix>());
  std::vector<UniTensor> Us;
  UniTensor S(T);
  std::vector<int>out_labels(S.labels.begin(), S.labels.begin() + fixedNum + modeNum);
  for(int m = 0; m < modeNum; m++){
    for(int l = 0; l < rsp_labels.size(); l++){
      if(l < combNum)
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l) % (bondNum - fixedNum))];
      else if(l < combNum + fixedNum)
        rsp_labels[l] = ori_labels[l - combNum];
      else
        rsp_labels[l] = ori_labels[fixedNum + (((m) * combNum + l - fixedNum) % (bondNum - fixedNum))];
    }
    T.permute(rsp_labels, combNum);
    std::vector<Bond> bonds(T.bonds.begin(), T.bonds.begin() + combNum);
    bonds.push_back(combine(bonds).dummy_change(BD_OUT));
    Us.push_back(UniTensor(bonds));
    for(std::map<Qnum, Block>::iterator it = T.blocks.begin(); it != T.blocks.end(); it++){
      std::vector<Matrix> svd = it->second.svd();
      Us[m].putBlock(it->first, svd[0]);
      if(returnL)
        Ls[m][it->first] = svd[1];
    }
    for(int c = 0; c < combNum; c++)
      Us[m].labels[c] = fixedNum + m * combNum + c;
    Us[m].labels[combNum] = -m - 1;
    out_labels[fixedNum + m] = -m -1;
    UniTensor UT = Us[m];
    S *= UT.transpose();
  }
  S.permute(out_labels, fixedNum);
  Us.push_back(S);
  return Us;
}

void RtoC(UniTensor& UniT){
  try{
    if(UniT.typeID() == 1){
      UniT.r_flag = RNULL;
      UniT.c_flag = CTYPE;
      UniT.c_elem = (Complex*)elemAlloc( UniT.m_elemNum * sizeof(Complex), UniT.ongpu);
      elemCast(UniT.c_elem, UniT.elem, UniT.m_elemNum, UniT.ongpu, UniT.ongpu);
      UniT.initBlocks(CTYPE);
      UniT.elem = NULL;
    }
  }
  catch(const std::exception& e){
    propogate_exception(e, "In constructor UniTensor::RtoC( )");
  }
}

}; /* namespace uni10 */

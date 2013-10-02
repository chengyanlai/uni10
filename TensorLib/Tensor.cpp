#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "mkl.h"
#include "mkl_lapack.h"
#include <string.h>
#include <assert.h>
#include <boost/random.hpp>
#include <stdint.h>
#include <limits.h>
using namespace boost;
#ifdef SEED
	mt19937 rng(SEED);
#else
	mt19937 rng(777);
#endif
uniform_01<mt19937> uni01_sampler(rng);
using namespace std;
size_t MEM;

#define INIT 		1		//initialized
#define HAVELABEL 	2	//tensor with label
#define HAVEELEM 	4	//tensor with elements
#define DISPOSABLE 	8	//The elements of tensor is disposable through 'clone', 'reshapeClone', 'reshapeElem'. The elements of disposable Tensor is read-only.
#define ELEMFREED 	16	//the memory space of elements is freed
#define HAVESYM		32
#define RBLOCK 		64	//the memory space of elements is freed
#define CBLOCK		128
#define ONBLOCK 	192	//RBLOCK + CBLOCK 

#include "Symmetry.cpp"
typedef struct{
	int status;		//Check initialization, 1 initialized, 3 initialized with label, 5 initialized with elements, 7 initialized completely.
	int bondNum;		//Tensor type
	int64_t elemNum;		//Number of elements in elem[] array
	int *bondDim;		//Dimemsions of the bonds, bonDim[0] is the high order bits in elem array.
	int *label;
	double *elem;		//Array of elements
	Symmetry *sym;
}Tensor;
/*
extern "C" {
  void dgemm(char* transA, char* transB, int* m, int* n, int* k, double* alpha, double* A, int* lda, double* B, int* ldb, 
              double *beta, double *c, int *ldc);	//c = alpha*op(A)*op(B)+beta*c
  void dcopy(int* n, double* x, int* incx, double* y, int* incy);		//y = x
  void daxpy(int* n, double* a, double* x, int* incx, double* y, int* incy);		//y = a*x+y
  void dgesvd(char* jobU, char* jobVT, int* M, int* N, double* A, int* ldA, double* S, double* U, int* ldU, double* VT, int* ldVT, double* work, int* lwork, int* info);
  void dsyev(char* jobz, char* uplo, int* N, double* A, int* ldA, double* Eig, double* work, int* lwork, int* info);
};
*/
//Initialization and memory allocatation
//Note that ,in matrix case, tensor's bondDim = {3,2} means it is a 3x2 matrix;
void initTensor(Tensor* T, int BondNumber, int *BondDimension);

//Add new label to a tensor
void addLabel(Tensor* T, int* Label);

//Add elememnts to a tensor
void addElem(Tensor* T, double* Elements);

//Output a Tensor struct.
void outTensorf(Tensor* T, char* Filename);	//formated data
void outTensor(Tensor* T, char* Filename);	//binary

//Fill a Tensor struct from file, to initialized an uninitilized Tensor.
void inTensorf(Tensor* T, char* Filename);	//formated
void inTensor(Tensor* T, char* Filename);

//Clone the input Tensor.
void clone(Tensor* Tin, Tensor* Tout);

//Reshape the element array of T to the given bond order. order[3] = 1 means the present forth bond should be the second bond.
//This function doesn't change the structure of tensor T; it just outputs the reshaped element array to NewElem.
void reshapeElem(Tensor* T, int* BondOrder, double *NewElem);

//Reshape the Tin tensor to the given bond order. order[3] = 1 means the present forth bond in Tin should be the second bond in Tout.
//This function doesn't change the structure of tensor Tin; it just outputs the reshaped Tensor to Tout.
void reshapeClone(Tensor* Tin, int* order, Tensor* Tout);

//Find the matched labels in Ta and Tb, contracted the corresponding matched bonds. Tc = Ta*Tb
void contraction(Tensor* Ta, Tensor* Tb, Tensor* Tc);


//transpose a matrix represented in C to the one represented in Fortran
void CtoF(int m, int n, double* Mc, double* Mf);
//transpose a matrix represented in Fortran to the one represented in C
void FtoC(int m, int n, double* Mf, double* Mc);

void Matrix_Product(double* A, int At, double* B, int Bt, int m, int k, int n, double* C);

void print_matrix(double* Matrix, int m, int n);

void Substract_Identity(double* Matrix, int size, double factor);


void initTensor(Tensor* T, int bn, int *bd){
	if(T->elem)
		MEM -= T->elemNum;
	T->bondNum = bn;
	int i;
	T->bondDim = (int*)realloc(T->bondDim, sizeof(int)*bn);
	memcpy((T->bondDim), bd, sizeof(int)*bn);
	T->label = (int*)realloc(T->label, sizeof(int)*bn);
	T->elemNum = 1;
	for(i = 0; i<bn; i++)
		T->elemNum *= bd[i];
	MEM += T->elemNum;
	T->elem = (double*)realloc(T->elem, sizeof(double)*(T->elemNum));
	T->status |= INIT;
}

void addLabel(Tensor* T, int* lb){
	assert(T->status & INIT);
	memcpy(T->label, lb, sizeof(int)*(T->bondNum));
	T->status |= HAVELABEL;
}

void addElem(Tensor* T, double* el){	
	assert(T->status & INIT);
	assert(!(T->status & ELEMFREED));
	memcpy(T->elem, el, sizeof(double)*(T->elemNum));
	T->status |= HAVEELEM;
}

/*-----------------Tensor output format------------------*/
/* The status of Tensor(integer)
 * Number of bonds(integer)
 * Number of elements(integer)
 * Array of bond dimensions(integer array)
 * Array of label, if the tensor has it.(integer array)
 * Array of elements, if the tensor has it.(double array)
 */
void outTensorf(Tensor* T, char* fn){
	FILE *fp = fopen(fn, "w");
	fprintf(fp, "%d\n", T->status);
	fprintf(fp, "%d\n", T->bondNum);
	fprintf(fp, "%ld\n", T->elemNum);
	int64_t i;
	for(i = 0; i<T->bondNum; i++)
		fprintf(fp, "%d ",T->bondDim[i]);
	fprintf(fp, "\n");
	if(T->status & HAVELABEL){
		for(i = 0; i<T->bondNum; i++)
			fprintf(fp, "%d ",T->label[i]);
		fprintf(fp, "\n");
	}
	if(T->status & HAVEELEM){
		for(i = 0; i <T->elemNum; i++)
			fprintf(fp, "%f ",T->elem[i]);
		fprintf(fp, "\n");
	}
	fclose(fp);
}

void outTensor(Tensor* T, char* fn){
	FILE *fp = fopen(fn, "w");
	fwrite(&T->status, sizeof(int), 1, fp);
	fwrite(&T->bondNum, sizeof(int), 1, fp);
	fwrite(&T->elemNum, sizeof(int64_t), 1, fp);
	fwrite(T->bondDim, sizeof(int), T->bondNum, fp);
	if(T->status & HAVELABEL)
		fwrite(T->label, sizeof(int), T->bondNum, fp);
	if(T->status & HAVEELEM)
		fwrite(T->elem, sizeof(double), T->elemNum, fp);
	fclose(fp);
}

void inTensorf(Tensor* T, char* fn){
	FILE *fp = fopen(fn, "r");
	assert(fp != NULL);
	fscanf(fp, "%d", &(T->status));
	fscanf(fp, "%d", &(T->bondNum));
	fscanf(fp, "%ld", &(T->elemNum));
	T->bondDim = (int*)realloc(T->bondDim, sizeof(int)*(T->bondNum));
	T->label = (int*)realloc(T->label, sizeof(int)*(T->bondNum));
	T->elem = (double*)realloc(T->elem, sizeof(double)*(T->elemNum));
	int64_t i;
	for(i = 0; i<T->bondNum; i++)
		fscanf(fp, "%d", &(T->bondDim[i]));
	if(T->status & HAVELABEL)
		for(i = 0; i<T->bondNum; i++)
			fscanf(fp, "%d", &(T->label[i]));
	if(T->status & HAVEELEM)
		for(i = 0; i<T->elemNum; i++)
			fscanf(fp, "%lf", &(T->elem[i]));
	fclose(fp);
}

void inTensor(Tensor* T, char* fn){
	FILE *fp = fopen(fn, "r");
	assert(fp != NULL);
	fread(&T->status, sizeof(int), 1, fp);
	fread(&T->bondNum, sizeof(int), 1, fp);
	fread(&T->elemNum, sizeof(int64_t), 1, fp);
	T->bondDim = (int*)realloc(T->bondDim, sizeof(int)*(T->bondNum));
	T->label = (int*)realloc(T->label, sizeof(int)*(T->bondNum));
	T->elem = (double*)realloc(T->elem, sizeof(double)*(T->elemNum));
	fread(T->bondDim, sizeof(int), T->bondNum, fp);
	if(T->status & HAVELABEL)
		fread(T->label, sizeof(int), T->bondNum, fp);
	if(T->status & HAVEELEM)
		fread(T->elem, sizeof(double), T->elemNum, fp);
	fclose(fp);
}

void clone(Tensor* Tin, Tensor* Tout){
	initTensor(Tout, Tin->bondNum, Tin->bondDim);
	if(Tin->status & HAVELABEL)
		addLabel(Tout, Tin->label);
	if(Tin->status & HAVEELEM){
		addElem(Tout, Tin->elem);
		if(Tin->status & DISPOSABLE){
			free(Tin->elem);
			Tin->status |= ELEMFREED;
		}
	}
}

void recycle(Tensor* T){
	free(T->bondDim);
	free(T->label);
	if((T->status & ELEMFREED) == 0){
		free(T->elem);
		MEM -= T->elemNum;
	}
	free(T);
}

void reshapeElem(Tensor* T, int* order, double *newElem){
	assert(T->status & HAVEELEM);
	int num = T->bondNum;
	int64_t i, j;
	//if order = [0 1 2 3], there's no need to reshape
	int done = 1;
	for(i = 0; i<num; i++)
		if(order[i] != i){
			done = 0;
			break;
		}
	if(done)
		memcpy(newElem, T->elem, sizeof(double)*(T->elemNum));
	else{
		int newDim[num];
		for(i = 0; i<num; i++)
			newDim[order[i]] = T->bondDim[i];
		int64_t newOffset[num];
		int64_t tempOffset[num];
		tempOffset[num-1] = 1;
		for(i = num-2; i >= 0; i--)
			tempOffset[i] = tempOffset[i+1]*newDim[i+1];
		for(i = 0; i<num; i++)
			newOffset[i] = tempOffset[order[i]];
		int64_t offset = 0;	//the offset of newElem.
		int idx[num];	//the indice of oldElem.
		memset(idx, 0, sizeof(int)*num);
		newElem[0] = T->elem[0];
		for(i = 1; i<T->elemNum; i++){
			for(j = num-1; j>=0; j--){
				if((idx[j]+1)/T->bondDim[j]){
					idx[j] = 0;
					offset -= (T->bondDim[j]-1)*newOffset[j];
				}
				else{
					idx[j]++;
					offset += newOffset[j];
					break;
				}
			}
			newElem[offset] = T->elem[i];
		}
	}	
	if(T->status & DISPOSABLE){
		free(T->elem);
		T->status |= ELEMFREED;
		MEM -= T->elemNum;
	}
}

void reshapeClone(Tensor* Tin, int* order, Tensor* Tout){
	int i;
	int newDim[Tin->bondNum];
	for(i = 0; i<Tin->bondNum; i++)
		newDim[order[i]] = Tin->bondDim[i];
	initTensor(Tout, Tin->bondNum, newDim);
	if(Tin->status & HAVELABEL){
		for(i = 0; i<Tout->bondNum; i++)
			Tout->label[order[i]] = Tin->label[i];
		Tout->status |= HAVELABEL;
	}
	if(Tin->status & HAVEELEM){
		reshapeElem(Tin, order, Tout->elem);
		Tout->status |= HAVEELEM;
	}
}

void contraction(Tensor* Ta, Tensor* Tb, Tensor* Tc){
	assert(Ta->status&HAVEELEM && Tb->status&HAVEELEM);
	//Tensor contraction. Tc = Ta*Tb.
	int M = 1, N = 1, K = 1;	//Ta = MxK, Tb = KxN, Tc = MxN
	int i, j, p;
	int num = 0;	//number of bonds which are to be contracted. If num = 0, outer product
	int aBondNum = Ta->bondNum;
	int bBondNum = Tb->bondNum;
	int aMatch[aBondNum];
	int bMatch[bBondNum];
	//set the reshape order, order[3] = 1 means the present forth bond should be the second bond.
	int aOrder[aBondNum];
	int bOrder[bBondNum];
	memset(aMatch, 0, sizeof(int)*aBondNum);
	memset(bMatch, 0, sizeof(int)*bBondNum);
	p = 1;
	for(i = 0; i<aBondNum; i++)
		for(j = 0; j<bBondNum; j++)
			if(Ta->label[i] == Tb->label[j]){
				//the ith bond of Ta match the jth bond of Tb.
				aMatch[i] = p;	
				bMatch[j] = p;
				num++;
				p++;
				assert(Ta->bondDim[i] == Tb->bondDim[j]);
			}
	//example for Ta->label=[12 29 7 43]; Tb->label=[43 33 12 19]  =>aMatch = [1 0 0 2], bMatch[2 0 1 0]
	//Generate a new Tensor C = A*B
	int cBondNum = aBondNum+bBondNum-2*num;
	int cBondDim[cBondNum];
	int cLabel[cBondNum];
	j = 0, p = 0;
	for(i = 0; i<aBondNum; i++){
		if(aMatch[i]){
			aOrder[i] = aBondNum-(num-aMatch[i])-1;
			K *= Ta->bondDim[i];
		}
		else{
			aOrder[i] = j;
			cBondDim[p] = Ta->bondDim[i];
			cLabel[p] = Ta->label[i];
			M *= Ta->bondDim[i];
			p++;
			j++;
		}
	}
	j = 0;
	for(i = 0; i<bBondNum; i++){
		if(bMatch[i])
			bOrder[i] = bMatch[i]-1;
		else{
			bOrder[i] = num+j;
			cBondDim[p] = Tb->bondDim[i];
			cLabel[p] = Tb->label[i];
			N *= Tb->bondDim[i];
			p++;
			j++;
		}
	}
	assert(p == cBondNum);
	initTensor(Tc, cBondNum, cBondDim);
	addLabel(Tc, cLabel);

	//Matrix Multiplication with BLAS dgemm
	//cElem = aNewElem*bNewElem; aNewElem is M*K matrix, bNewElem is K*N matrix, cElem = M*N matrix
	//Due to the different memory allignment in BLAS, what I do in the following may seem weird, but it's correct.
	double *aNewElem = (double*)malloc(Ta->elemNum*sizeof(double));
	MEM += Ta->elemNum;
	reshapeElem(Ta, aOrder, aNewElem);
	double *bNewElem = (double*)malloc(Tb->elemNum*sizeof(double));
	MEM += Tb->elemNum;
	reshapeElem(Tb, bOrder, bNewElem);
	//Do matrix multiplication.
	double alpha = 1, beta = 0;
	dgemm((char*)"N", (char*)"N", &N, &M, &K, &alpha, bNewElem, &N, aNewElem, &K, &beta, Tc->elem, &N);
	Tc->status |= HAVEELEM;	
	free(aNewElem);
	MEM -= Ta->elemNum;
	free(bNewElem);
	MEM -= Tb->elemNum;
}

void contraction(Tensor* Ta, Tensor* Tb, Tensor* Tc, int* outLabel){
	contraction(Ta, Tb, Tc);
	int bondNum = Tc->bondNum;
	int order[bondNum];
	int hit = 0;
	for(int i = 0; i < bondNum; i++)
		for(int j = 0; j < bondNum; j++)
			if(Tc->label[i] == outLabel[j]){
				order[i] = j;
				hit ++;
			}
	assert(hit == bondNum);
	double *cNewElem = (double*)malloc(Tc->elemNum*sizeof(double));
	MEM += Tc->elemNum;
	reshapeElem(Tc, order, cNewElem);
	addElem(Tc, cNewElem);
	int oldDim[bondNum];
	for(int i = 0; i<Tc->bondNum; i++)
		oldDim[i] = Tc->bondDim[i];
	for(int i = 0; i<Tc->bondNum; i++)
		Tc->bondDim[order[i]] = oldDim[i];
	addLabel(Tc, outLabel);
	free(cNewElem);
	MEM -= Tc->elemNum;
}

void fusion(vector<Tensor*>& Tlist, Tensor* Tout){

}

/*Generate a set of row vectors which form a othonormal basis
 *For the incoming Tensor, the number of row <= the number of column
 */
void OrthoRandomize(Tensor *Tin, int row_bond_num){
	int M = 1, N = 1;
	for(int i = 0; i < Tin->bondNum; i++){
		if(i < row_bond_num)
			M *= Tin->bondDim[i];
		else
			N *= Tin->bondDim[i];
	}
	int eleNum = M*N;
	double random[eleNum];
	for (int i=0 ; i<eleNum ; i++){
		random[i] = uni01_sampler();
	}
	assert(M <= N);
	Tensor * tmpT = (Tensor*)calloc(1, sizeof(Tensor));
	int tmp_bondDim[Tin->bondNum];
	for(int i = 0; i < Tin->bondNum; i++)	//row_bond_num of Tin is col_bond_num of tmp
		tmp_bondDim[i] = Tin->bondDim[(i + row_bond_num) % Tin->bondNum];
	initTensor(tmpT, Tin->bondNum, tmp_bondDim);
	int min = M; //min = min(M,N)
	int ldA = M, ldu = M, ldvT = min;
	double *S = (double*)malloc(min*sizeof(double));
	double *u = (double*)malloc(ldu*M*sizeof(double));
	int lwork = 16*N;
	double *work = (double*)malloc(lwork*sizeof(double));
	int info;
	//tmpT = u*S*vT
	dgesvd((char*)"A", (char*)"S", &M, &N, random, &ldA, S, u, &ldu, tmpT->elem, &ldvT, work, &lwork, &info);
	tmpT->status |= HAVEELEM;
	//reshape from Fortran format to C format
	int rsp_order[Tin->bondNum];
	for(int i = 0; i < Tin->bondNum; i++)
		rsp_order[i] = (i + row_bond_num)%Tin->bondNum;
	reshapeElem(tmpT, rsp_order, Tin->elem);
	Tin->status |= HAVEELEM;
	recycle(tmpT);
	free(S);
	free(u);
	free(work);
}

void Identity(Tensor* Tin, int row_bond_num){
	int M = 1, N = 1;
	for(int i = 0; i < Tin->bondNum; i++){
		if(i < row_bond_num)
			M *= Tin->bondDim[i];
		else
			N *= Tin->bondDim[i];
	}
	int min;
	if(M < N)	min = M;
	else		min = N;
	memset(Tin->elem, 0, Tin->elemNum * sizeof(double));
	for(int i = 0; i < min; i++)
		Tin->elem[i * N + i] = 1;
	Tin->status |= HAVEELEM;
}

//Tout = Tout + a * Ta
void addTensor(Tensor* T, double a, Tensor* Ta){
	assert(T->status & HAVEELEM);
	assert(Ta->status & HAVEELEM);
	int64_t left = T->elemNum;
	int inc = 1;
	assert(left == Ta->elemNum);
	int64_t offset = 0;
	int chunk;
	while(left > 0){
		if(left > INT_MAX)
			chunk = INT_MAX;
		else
			chunk = left;
		daxpy(&chunk, &a, Ta->elem + offset, &inc, T->elem + offset, &inc);
		offset += chunk;
		left -= INT_MAX;
	}
}

void print_tensor(Tensor* T, int row_bond_num){
	int M = 1, N = 1;
	for(int i = 0; i < T->bondNum; i++){
		if(i < row_bond_num)
			M *= T->bondDim[i];
		else
			N *= T->bondDim[i];
	}
	int i, j;
	printf("\nRank %d Tensor\n", T->bondNum);
	printf("Bonds Dimemsion: ");
	for(i = 0; i < T->bondNum; i++)
		printf("%d ", T->bondDim[i]);
	printf("\n");
	if(T->status & HAVELABEL){
		printf("Label: ");
		for(i = 0; i < T->bondNum; i++)
			printf("%d ", T->label[i]);
		printf("\n");
	}
	printf("\n");
	if(T->status & HAVEELEM)
		for(i = 0; i < M; i++) {
			for(j = 0; j < N; j++)
				printf("%6.2f", T->elem[i*N+j]);
			printf( "\n" );
			printf( "\n" );
		}
	printf("\n");
}

void CtoF(int m, int n, double* Mc, double* Mf){
	int i, j;
	for( i = 0; i < m; i++ )
		for( j = 0; j < n; j++ )  
			Mf[j*m+i] = Mc[i*n+j];
}
	
void FtoC(int m, int n, double* Mf, double* Mc){
	int i, j;
	for( i = 0; i < m; i++ )
		for( j = 0; j < n; j++ )  
			Mc[i*n+j] = Mf[j*m+i];
}

/*------For Symmetry------*/
#define TOBLOCK 1
#define OFFBLOCK 0
void addSym(Tensor* T, int row_bond_num, Symmetry* S, int status = 0){
	//status=ONBLOCK means that, the element is block diagonalized now;
	assert(T->status & INIT);
	int M = 1, N = 1;
	for(int i = 0; i < T->bondNum; i++)
		if(i < row_bond_num)
			M *= T->bondDim[i];
		else
			N *= T->bondDim[i];
	assert(S->totRow == M);
	assert(S->totCol == N);
	T->sym = S;
	T->status |= HAVESYM;
	assert(status == 0 || status == RBLOCK || status == CBLOCK || status == ONBLOCK);
	T->status |= status;
}

void addTable(Symmetry* S, int mapNum, int* table, double* factor){
	assert(S->group.size());
	int tableSz = S->totRow > S->totCol ? S->totRow: S->totCol;	//max(S->totRow, S->totCol)
	S->mapNum = mapNum;
	if(S->table != NULL)
		free(S->table);
	S->table = (int*)calloc(mapNum * tableSz, sizeof(int));
	memcpy(S->table, table, sizeof(int)*(mapNum * tableSz));
	if(S->invTable != NULL)
		free(S->invTable);
	S->invTable = (int*)calloc(mapNum * tableSz, sizeof(int));
    for (int i=0; i< mapNum * tableSz; i++)
    	S->invTable[i]  = 0;
	
    #ifdef SANITY_CHECK
	int check[tableSz];
	memset(check, 0, tableSz * sizeof(int));
	for(int i = 0; i < tableSz * mapNum; i++){
		if(mapNum == 1)
			check[table[i]] ++;
		else if(factor[i] != 0)
			check[table[i]] ++;
	}
	for(int i = 0; i < tableSz; i++){
		if (check[i] < 1)
			printf("check[%d] = %d; too small!!!\n", i, check[i]);
	   	if (check[i] > mapNum)
		    printf("check[%d] = %d; too large!!!\n", i, check[i]);
		assert(check[i] >= 1);
		assert(check[i] <= mapNum);
	}
	//printf("CHECK OK!\n");
	#endif

	if(mapNum == 1)
		for(int i = 0; i < tableSz; i++)
			S->invTable[table[i]] = i;
	else{
		if(S->factor != NULL)
			free(S->factor);
		S->factor = (double*)calloc(mapNum * tableSz, sizeof(double));
		memcpy(S->factor, factor, sizeof(double)*(mapNum * tableSz));
		if(S->invFactor != NULL)
			free(S->invFactor);
		S->invFactor = (double*)calloc(mapNum * tableSz, sizeof(double));
        for (int i=0; i<tableSz*mapNum; i++)
			S->invFactor[i] = 0;

		int count[tableSz];
		memset(count, 0, tableSz * sizeof(int));
		int elemNum = tableSz * mapNum;
		for(int i = 0; i < elemNum; i++)
			if(factor[i] != 0){
				S->invTable[table[i] * 2 + count[table[i]]] = (i / mapNum);
				S->invFactor[table[i] * 2 + count[table[i]]] = factor[i];
				count[table[i]]++;
			}
	}
}

//Caution!!! This is a unsafe function.
//Make sure you FREE the double pointer you get from it.
double* getBlock(Tensor* Tsrc, int row_bond_num, int groupNo){
	assert(Tsrc->status & HAVEELEM);
	assert(Tsrc->status & HAVESYM);
	assert(Tsrc->status & RBLOCK && Tsrc->status & CBLOCK);
	Symmetry* S = Tsrc->sym;
	assert(groupNo < S->group.size());
	Group g = S->group[groupNo];
	int M = 1, N = 1;
	for(int i = 0; i < Tsrc->bondNum; i++){
		if(i < row_bond_num)
			M *= Tsrc->bondDim[i];
		else
			N *= Tsrc->bondDim[i];
	}
	assert(M == S->totRow);
	assert(N == S->totCol);
	double *outBlock = (double*)malloc(g.col * g.row * sizeof(double));
	for(int i = 0; i < g.row; i++)
		memcpy(outBlock + i * g.col, Tsrc->elem + (i + g.row_begin) * N + g.col_begin, g.col * sizeof(double));
	return outBlock;
}

void putBlock(Tensor* Tout, int row_bond_num, int groupNo, double* src){
	assert(Tout->status & INIT);
	assert(Tout->status & HAVESYM);
	assert(Tout->status & RBLOCK && Tout->status & CBLOCK);
	Symmetry* S = Tout->sym;
	assert(groupNo < S->group.size());
	Group g = S->group[groupNo];
	int M = 1, N = 1;
	for(int i = 0; i < Tout->bondNum; i++){
		if(i < row_bond_num)
			M *= Tout->bondDim[i];
		else
			N *= Tout->bondDim[i];
	}
	assert(M == S->totRow);
	assert(N == S->totCol);
	for(int i = 0; i < g.row; i++)
		memcpy(Tout->elem + (i + g.row_begin) * N + g.col_begin, src + i * g.col, g.col * sizeof(double));
}

void symOrthoRandomize(Tensor* T, int row_bond_num){
	assert(T->status & INIT);
	assert(T->status & HAVESYM);
	memset(T->elem, 0, T->elemNum * sizeof(double));
	T->status |= RBLOCK;
	T->status |= CBLOCK;
	Symmetry* S = T->sym;
	int tmpDim[2];
	Tensor* Ttmp = (Tensor*)calloc(1, sizeof(Tensor));
	for(int i = 0; i < S->group.size(); i++){
		Group g = S->group[i];
		tmpDim[0] = g.row;
		tmpDim[1] = g.col;
		initTensor(Ttmp, 2, tmpDim);
		OrthoRandomize(Ttmp, 1);
		putBlock(T, row_bond_num, i, Ttmp->elem);
	}
	recycle(Ttmp);	
	T->status |= HAVEELEM;
}

//how = TOBLOCK: basis transform to block-diagonal basis
//how = OFFBLOCK: basis transform to original basis
void chCol(Tensor* T, int row_bond_num, int how){	
	assert(T->status & HAVEELEM);
	assert((T->sym)->mapNum);
	double *newElem = (double*)calloc(T->elemNum, sizeof(double));
	int M = (T->sym)->totRow;
	int N = (T->sym)->totCol;
	int mapNum = (T->sym)->mapNum;
	int *table;
	double *factor;
	if(how == OFFBLOCK){
		assert(T->status & CBLOCK);
		table = (T->sym)->invTable;
		factor = (T->sym)->invFactor;
	}
	else if(how == TOBLOCK){
		assert((T->status & CBLOCK) == 0);
		table = (T->sym)->table;
		factor = (T->sym)->factor;
	}
	else
		assert(1);
	int off;
	if(mapNum == 1)
		for(int i = 0; i < M; i++){
			off = i * N;
			for(int j = 0; j < N; j++)
				newElem[off + table[j]] = T->elem[off + j];
		}
	else
		for(int i = 0; i < M; i++){
			off = i * N;
			for(int j = 0; j < N; j++)
				for(int k = 0; k < mapNum; k++)
					newElem[off + table[j * mapNum + k]] += factor[j * mapNum + k] * T->elem[off + j];
		}
	T->status ^= CBLOCK;
	addElem(T, newElem);
	free(newElem);
}

void chRow(Tensor* T, int row_bond_num, int how){	
	assert(T->status & HAVEELEM);
	assert((T->sym)->mapNum);
	double *newElem = (double*)calloc(T->elemNum, sizeof(double));
	int M = (T->sym)->totRow;
	int N = (T->sym)->totCol;
	int mapNum = (T->sym)->mapNum;
	int *table;
	double* factor;
	if(how == OFFBLOCK){
		assert(T->status & RBLOCK);
		table = (T->sym)->invTable;
		factor = (T->sym)->invFactor;
	}
	else if(how == TOBLOCK){
		assert((T->status & RBLOCK) == 0);
		table = (T->sym)->table;
		factor = (T->sym)->factor;
	}
	else
		assert(1);
	int off;
	if(mapNum == 1)
		for(int i = 0; i < M; i++)
			memcpy(newElem + table[i] * N, T->elem + i * N, N * sizeof(double));
	else
		for(int i = 0; i < M; i++){
			int inc = 1;
			for(int k = 0; k < mapNum; k++)
				daxpy(&N, &factor[i * mapNum + k], T->elem + i * N, &inc, newElem + table[i * mapNum + k] * N, &inc);
		}
	T->status ^= RBLOCK;
	addElem(T, newElem);
	free(newElem);
}

void Block_Check(Tensor* T, int row_bond_number){
    #ifdef BLOCK_CHECK
	assert(T->status & HAVESYM);
	assert(T->status & ONBLOCK);
		                
	double error = (1.0E-10);
	for(int g = 0; g < (T->sym)->group.size(); g++){
		int row_start = (T->sym)->group[g].row_begin;
		int row_end   = row_start + (T->sym)->group[g].row;
		double temp;  
		for (int i = row_start; i< row_end; i++){
			for (int j = 0; j<(T->sym)->group[g].col_begin; j++){
				temp = T->elem[ i * ((T->sym)->totCol) + j ];
				if(temp < 0)
					temp = -temp;
				if (temp > error){
			   		printf("1. Group[%d] : elem[%d][%d] = %e\n", g, i, j, T->elem[ i * ((T->sym)->totCol) + j ]);
					print_tensor(T, row_bond_number);
				}
                assert(temp < error);
          	}
			for (int j = (T->sym)->group[g].col_begin + (T->sym)->group[g].col; j<(T->sym)->totCol; j++){
				temp = T->elem[ i * ((T->sym)->totCol) + j ];
				if (temp < 0)
					temp = -temp;
				if (temp > error){
				    printf("2. Group[%d] : elem[%d][%d] = %e\n", g, i, j, T->elem[ i * ((T->sym)->totCol) + j ]);
					print_tensor(T, row_bond_number);
				}
				assert(temp < error);
			}
		}
	}
	#endif
}

void Unitary_Check(Tensor* T, int row_bond_num){
	#ifdef UNITARY_CHECK
	assert(T->status & HAVEELEM);
	double error = 1.0E-10;
	int M = 1;
	int N = 1;
	for(int i = 0; i<row_bond_num; i++)
		M *= T->bondDim[i];
	for(int i = row_bond_num; i<T->bondNum; i++)
	    N *= T->bondDim[i];

	double* Result = (double*)malloc(M*M*sizeof(double));
	Matrix_Product(T->elem, 0, T->elem, 1, M, N, M, Result);
    for (int i=0; i<M; i++)
    	Result[i*M + i] -= 1;
	for (int i=0; i<M*M; i++)
		if (Result[i]<0)
        	Result[i] *= -1;

    for (int i=0; i<M*M; i++){
	    if (Result[i] > error){
	    	print_tensor(T, row_bond_num);
			printf("T * T dagger = :\n\n");
			print_matrix(Result,M,M);
			printf("elem[%d][%d] = %e\n", i/M, i%M, Result[i]);
		}
		assert (Result[i] < error);
	}
	free(Result);
	printf("Unitary Check OK!!\n");
	#endif
}


void Matrix_Product(double* A, int At, double* B, int Bt, int m, int k, int n, double* C){
	int M = n;
	int K = k;
	int N = m;
	double* a = B;
	double* b = A;
	int at = Bt;
	int bt = At;
	char* transa;
	char* transb;
	double alpha = 1;
	double beta = 0;
	int lda,ldb;
	int ldc = M;
	//at == 0 means no transpose, but we have to transpose it for mkl
	if (at == 0){
		transa = "N";
		lda = M;
	}
	else if (at == 1){
	    transa = "T";
		lda = K;
	}
	else
		printf("at must be 0 or 1!!!!!!\n");

	if (bt == 0){
		transb = "N";
		ldb = K;
	}
	else if (bt == 1){
		transb = "T";
		ldb = N;
	}
	else
	printf("bt must be 0 or 1!!!!!!\n");
	
	dgemm(transa, transb, &M, &N, &K, &alpha, a, &lda, b, &ldb, &beta, C, &ldc);
}

void print_matrix(double* Matrix, int m, int n){
	printf("\n\n%dx%d Matrix:\n\n", m, n);
	for (int i=0; i<m; i++){
		for (int j=0; j<n; j++)
			printf("%8.4f", Matrix[i*n + j]);
		printf("\n");
	}
	printf("\n\n");
}

void Substract_Identity(double* Matrix, int size, double factor){
	for (int i=0; i<size; i++)
		Matrix[i*size + i] -= factor;
}

double Trace(double* A, double* B, int N){
	double* C=(double*)malloc(N*N*sizeof(double));
	Matrix_Product(A, 0, B, 0, N, N, N, C);
	double TrC = 0;
	for (int i=0; i<N; i++)
		TrC += C[(N+1) * i];
	free(C);
	return TrC;
}
	

//  Copyright (C) 2013 Lindley Lentati

/*
*    This file is part of TempoNest 
* 
*    TempoNest is free software: you can redistribute it and/or modify 
*    it under the terms of the GNU General Public License as published by 
*    the Free Software Foundation, either version 3 of the License, or 
*    (at your option) any later version. 
*    TempoNest  is distributed in the hope that it will be useful, 
*    but WITHOUT ANY WARRANTY; without even the implied warranty of 
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
*    GNU General Public License for more details. 
*    You should have received a copy of the GNU General Public License 
*    along with TempoNest.  If not, see <http://www.gnu.org/licenses/>. 
*/

/*
*    If you use TempoNest and as a byproduct both Tempo2 and MultiNest
*    then please acknowledge it by citing Lentati L., Alexander P., Hobson M. P. (2013) for TempoNest,
*    Hobbs, Edwards & Manchester (2006) MNRAS, Vol 369, Issue 2, 
*    pp. 655-672 (bibtex: 2006MNRAS.369..655H)
*    or Edwards, Hobbs & Manchester (2006) MNRAS, VOl 372, Issue 4,
*    pp. 1549-1574 (bibtex: 2006MNRAS.372.1549E) when discussing the
*    timing model and MultiNest Papers here.
*/


#include <stdio.h>
#include <vector>
#include <string.h>
#include <gsl/gsl_sf_gamma.h>
#include "dgemm.h"
#include "dgemv.h"
#include "dpotri.h"
#include "dpotrf.h"
#include "dpotrs.h"
#include "tempo2.h"
#include "TempoNest.h"

void getLinearPriors(pulsar *psr, double **Dmatrix, long double **LDpriors, double **Dpriors, int numtofit, int fitsig){

	double *linearParams = new double[numtofit];
	double *nonLinearParams = new double[numtofit];
	double *errorvec = new double[numtofit];
		
	nonLinearParams[0]=(double)psr[0].offset_e;
	linearParams[0]=0;
	for(int j=1;j<numtofit;j++){
		nonLinearParams[j]=(double)LDpriors[j-1][1];
		linearParams[j]=0;
	}
	TNIupdateParameters(psr,0,nonLinearParams,errorvec, linearParams);

	double *Drms=new double[numtofit];
	for(int i =0; i < numtofit; i++){
		Drms[i]=0;
		
		for(int j =0; j < psr->nobs; j++){
			Drms[i] += Dmatrix[j][i]*Dmatrix[j][i];
// 			printf("%i %i %g \n",i,j,Dmatrix[j][i]);
		}
		Drms[i]=sqrt(Drms[i]/psr->nobs);
	}

	for(int i =0; i < numtofit; i++){
		if(Dpriors[i][0] !=0 && Dpriors[i][1] != 0){
			if(Drms[i] != 0){
				Dpriors[i][0]=Dpriors[i][0]*linearParams[i];
				Dpriors[i][1]=Dpriors[i][1]*linearParams[i];
				//printf("DP: %i %g %g %g %g\n",i,fitsig*trms,Drms[i],Dpriors[i][0], Dpriors[i][1]);
			}
			else if(Drms[i] == 0){
				Dpriors[i][0]=0;
				Dpriors[i][1]=0;
				//printf("DP: %i %g %g %g %g\n",i,fitsig*trms,Drms[i],Dpriors[i][0], Dpriors[i][1]);
			}

						
		}
		else {
			printf("DP: Prior %i is set to zero, marginalising?\n ", i);
		}

// 		printf("compare: %i %g %g \n",i,trms/Drms[i],linearParams[i]);
	}
}


void vHRedLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD, redamp, redalpha;
	int pcount=0;

// 	printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
// 	printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
		
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				

	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("EQUAD: %g \n",EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;
//		printf("E: %g %g \n",EQUAD,EFAC[0]);

	}


	redamp=Cube[pcount];
	pcount++;
	redalpha=Cube[pcount];
	pcount++;
  	

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];
	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}


	double secday=24*60*60;
	double LongestPeriod=1.0/pow(10.0,-5);
	double flo=1.0/LongestPeriod;

	double modelalpha=redalpha;
	double gwamp=pow(10.0,redamp);
	double gwampsquared=gwamp*gwamp*(pow((365.25*secday),2)/(12*M_PI*M_PI))*(pow(365.25,(1-modelalpha)))/(pow(flo,(modelalpha-1)));

	double timdiff=0;

	double covconst=gsl_sf_gamma(1-modelalpha)*sin(0.5*M_PI*modelalpha);
// 	printf("constants: %g %g \n",gwampsquared,covconst);


	
	double **CovMatrix = new double*[((MNStruct *)context)->pulse->nobs]; for(int o1=0;o1<((MNStruct *)context)->pulse->nobs;o1++)CovMatrix[o1]=new double[((MNStruct *)context)->pulse->nobs];

	for(int o1=0;o1<((MNStruct *)context)->pulse->nobs; o1++){

		for(int o2=0;o2<((MNStruct *)context)->pulse->nobs; o2++){
			timdiff=((MNStruct *)context)->pulse->obsn[o1].bat-((MNStruct *)context)->pulse->obsn[o2].bat;	
			double tau=2.0*M_PI*fabs(timdiff);
			double covsum=0;

			for(int k=0; k <=10; k++){
				covsum=covsum+pow(-1.0,k)*(pow(flo*tau,2*k))/(iter_factorial(2*k)*(2*k+1-modelalpha));

			}

			CovMatrix[o1][o2]=gwampsquared*(covconst*pow((flo*tau),(modelalpha-1)) - covsum);
// 			printf("%i %i %g %g %g\n",o1,o2,CovMatrix[o1][o2],fabs(timdiff),covsum);

			if(o1==o2){
				CovMatrix[o1][o2] += pow(((((MNStruct *)context)->pulse->obsn[o1].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o1]],2) + EQUAD;
			}

		}
	}

	double covdet=0;
	double *WorkDiffvec = new double[((MNStruct *)context)->pulse->nobs];
	for(int o1=0;o1<((MNStruct *)context)->pulse->nobs; o1++){
		WorkDiffvec[o1]=Diffvec[o1];
	}
	dpotrf(CovMatrix, ((MNStruct *)context)->pulse->nobs, covdet);
        dpotrs(CovMatrix, WorkDiffvec, ((MNStruct *)context)->pulse->nobs);


	double Chisq=0;


	for(int o1=0;o1<((MNStruct *)context)->pulse->nobs; o1++){
		Chisq += Diffvec[o1]*WorkDiffvec[o1];
	}

	if(isnan(covdet) || isinf(covdet) || isnan(Chisq) || isinf(Chisq)){

		lnew=-pow(10.0,200);
// 		printf("red amp and alpha %g %g\n",redamp,redalpha);
// 		printf("Like: %g %g %g \n",lnew,Chisq,covdet);
		
	}
	else{
		lnew = -0.5*(((MNStruct *)context)->pulse->nobs*log(2*M_PI) + covdet + Chisq);	
// 		printf("red amp and alpha %g %g\n",redamp,redalpha);


	}
// 	endClock = clock();
// //   	printf("Finishing off: time taken = %.2f (s)\n",(endClock-startClock)/(float)CLOCKS_PER_SEC);
	

	delete[] EFAC;
	for(int o=0;o<((MNStruct *)context)->pulse->nobs;o++){delete[] CovMatrix[o];}
	delete[] CovMatrix;
	delete[] WorkDiffvec;
	delete[] Diffvec;
	delete[] Fitvec;
	printf("Like: %g %g %g \n",lnew,Chisq,covdet);

}

void vHRedMarginLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD, redamp, redalpha;
	int pcount=0;

// 	printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
// 	printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
		
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				

	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("EQUAD: %g \n",EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;
//		printf("E: %g %g \n",EQUAD,EFAC[0]);

	}


	redamp=Cube[pcount];
	pcount++;
	redalpha=Cube[pcount];
	pcount++;
  	

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];

	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');

	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}

	double *WorkGDiffvec=new double[((MNStruct *)context)->Gsize];
	double *GDiffvec=new double[((MNStruct *)context)->Gsize];



	double secday=24*60*60;
	double LongestPeriod=1.0/pow(10.0,-5);
	double flo=1.0/LongestPeriod;

	double modelalpha=redalpha;
	double gwamp=pow(10.0,redamp);
	double gwampsquared=gwamp*gwamp*(pow((365.25*secday),2)/(12*M_PI*M_PI))*(pow(365.25,(1-modelalpha)))/(pow(flo,(modelalpha-1)));

	double timdiff=0;

	double covconst=gsl_sf_gamma(1-modelalpha)*sin(0.5*M_PI*modelalpha);
// 	printf("constants: %g %g \n",gwampsquared,covconst);


	
	double **CovMatrix = new double*[((MNStruct *)context)->pulse->nobs]; for(int o1=0;o1<((MNStruct *)context)->pulse->nobs;o1++)CovMatrix[o1]=new double[((MNStruct *)context)->pulse->nobs];

	for(int o1=0;o1<((MNStruct *)context)->pulse->nobs; o1++){

		for(int o2=0;o2<((MNStruct *)context)->pulse->nobs; o2++){
			timdiff=((MNStruct *)context)->pulse->obsn[o1].bat-((MNStruct *)context)->pulse->obsn[o2].bat;	
			double tau=2.0*M_PI*fabs(timdiff);
			double covsum=0;

			for(int k=0; k <=10; k++){
				covsum=covsum+pow(-1.0,k)*(pow(flo*tau,2*k))/(iter_factorial(2*k)*(2*k+1-modelalpha));

			}

			CovMatrix[o1][o2]=gwampsquared*(covconst*pow((flo*tau),(modelalpha-1)) - covsum);
// 			printf("%i %i %g %g %g\n",o1,o2,CovMatrix[o1][o2],fabs(timdiff),covsum);

			if(o1==o2){
				CovMatrix[o1][o2] += pow(((((MNStruct *)context)->pulse->obsn[o1].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o1]],2) + EQUAD;
			}

		}
	}

	double **CG = new double*[((MNStruct *)context)->pulse->nobs]; for(int o1=0;o1<((MNStruct *)context)->pulse->nobs;o1++)CG[o1]=new double[((MNStruct *)context)->Gsize];

	double **GCG= new double*[((MNStruct *)context)->Gsize]; for(int o1=0;o1<((MNStruct *)context)->Gsize;o1++)GCG[o1]=new double[((MNStruct *)context)->Gsize];


	dgemm(CovMatrix,((MNStruct *)context)->GMatrix, CG, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, 'N', 'N');

	dgemm(((MNStruct *)context)->GMatrix,CG, GCG, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, 'T', 'N');

	dgemv(((MNStruct *)context)->GMatrix,Diffvec,GDiffvec,((MNStruct *)context)->pulse->nobs,((MNStruct *)context)->Gsize,'T');

	double covdet=0;
	for(int o1=0;o1<((MNStruct *)context)->Gsize; o1++){
		WorkGDiffvec[o1]=GDiffvec[o1];
	}
	dpotrf(GCG, ((MNStruct *)context)->Gsize, covdet);
        dpotrs(GCG, WorkGDiffvec, ((MNStruct *)context)->Gsize);



	double Chisq=0;


	for(int o1=0;o1<((MNStruct *)context)->Gsize; o1++){
		Chisq += GDiffvec[o1]*WorkGDiffvec[o1];
	}

	if(isnan(covdet) || isinf(covdet) || isnan(Chisq) || isinf(Chisq)){

		lnew=-pow(10.0,200);
// 		printf("red amp and alpha %g %g\n",redamp,redalpha);

		
	}
	else{
		lnew = -0.5*(((MNStruct *)context)->Gsize*log(2*M_PI) + covdet + Chisq);	

	}
// 		printf("Like: %g %g %g \n",lnew,Chisq,covdet);

	delete[] EFAC;
	for(int o=0;o<((MNStruct *)context)->pulse->nobs;o++){delete[] CovMatrix[o];}
	delete[] CovMatrix;
	for(int o=0;o<((MNStruct *)context)->pulse->nobs;o++){delete[] CG[o];}
	delete[] CG;
	for(int o=0;o<((MNStruct *)context)->Gsize;o++){delete[] GCG[o];}
	delete[] GCG;

	delete[] GDiffvec;
	delete[] WorkGDiffvec;
	delete[] Diffvec;
	delete[] Fitvec;
	

}


void WhiteLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD;
	int pcount=0;
	
	//printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
	//printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

	//printf("here3\n");
	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				
	//printf("here4\n");
	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("E: %g %g\n",EFAC[0],EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;

	}

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];
	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}



	double Chisq=0;
	double noiseval=0;
	double detN=0;
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		noiseval=pow(((((MNStruct *)context)->pulse->obsn[o].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o]],2) + EQUAD;
		Chisq += pow((Diffvec[o]),2)/noiseval;
		detN += log(noiseval);
	}

	if(isnan(detN) || isinf(detN) || isnan(Chisq) || isinf(Chisq)){

		lnew=-pow(10.0,200);
	
	}
	else{
		lnew = -0.5*(((MNStruct *)context)->pulse->nobs*log(2*M_PI) + detN + Chisq);	
	}

	delete[] EFAC;
	delete[] Fitvec;
	delete[] Diffvec;
	//printf("%g %g %g \n",lnew,Chisq,detN);

}

void WhiteMarginLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD;
	int pcount=0;
	
// 	printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
// 	printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

// 	printf("here3\n");
	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				
// 	printf("here4\n");
	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("E: %g %g\n",EFAC[0],EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;

	}

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];
	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}


	double *Noise=new double[((MNStruct *)context)->pulse->nobs];
	double *GDiffvec=new double[((MNStruct *)context)->Gsize];

	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Noise[o]=pow(((((MNStruct *)context)->pulse->obsn[o].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o]],2) + EQUAD;
	}

	double **NG = new double*[((MNStruct *)context)->pulse->nobs]; for (int k=0; k<((MNStruct *)context)->pulse->nobs; k++) NG[k] = new double[((MNStruct *)context)->Gsize];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		for(int j=0;j<((MNStruct *)context)->Gsize; j++){
			NG[i][j]=((MNStruct *)context)->GMatrix[i][j]*Noise[i];

		}
	}

	double** GG = new double*[((MNStruct *)context)->Gsize]; for (int k=0; k<((MNStruct *)context)->Gsize; k++) GG[k] = new double[((MNStruct *)context)->Gsize];

	dgemm(((MNStruct *)context)->GMatrix, NG,GG,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, 'T','N');


	dgemv(((MNStruct *)context)->GMatrix,Diffvec,GDiffvec,((MNStruct *)context)->pulse->nobs,((MNStruct *)context)->Gsize,'T');



	double detN=0;
	double *WorkGDiffvec = new double[((MNStruct *)context)->Gsize];
	for(int o1=0;o1<((MNStruct *)context)->Gsize; o1++){
		WorkGDiffvec[o1]=GDiffvec[o1];
	}
	dpotrf(GG, ((MNStruct *)context)->Gsize, detN);
        dpotrs(GG, WorkGDiffvec, ((MNStruct *)context)->Gsize);



	double Chisq=0;
	for(int o1=0;o1<((MNStruct *)context)->Gsize; o1++){
		Chisq += GDiffvec[o1]*WorkGDiffvec[o1];
	}


	if(isnan(detN) || isinf(detN) || isnan(Chisq) || isinf(Chisq)){

		lnew=-pow(10.0,200);
	
	}
	else{
		lnew = -0.5*(((MNStruct *)context)->pulse->nobs*log(2*M_PI) + detN + Chisq);	
	}

	//printf("lnew: %g %g %g \n", lnew, detN, Chisq);

	delete[] EFAC;
	delete[] Fitvec;
	delete[] Diffvec;
	delete[] GDiffvec;
	delete[] WorkGDiffvec;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[] NG[j];
	}
	delete[] NG;

	for (int j = 0; j < ((MNStruct *)context)->Gsize; j++){
		delete[]GG[j];
	}
	delete[] GG;


}


void LRedLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD;
	int pcount=0;
	
// 	printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
// 	printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				

	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("EQUAD: %g \n",EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;
// 		printf("EQUAD: %g %g %g %i \n",EQUAD,EQUADPrior[0],EQUADPrior[1],((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps + ((MNStruct *)context)->numFitEFAC);
	}

  	

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];
	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}


	int FitCoeff=2*(((MNStruct *)context)->numFitRedCoeff);
	double *WorkNoise=new double[((MNStruct *)context)->pulse->nobs];
	double *powercoeff=new double[FitCoeff];


	double tdet=0;
	double timelike=0;



	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		//	printf("Noise %i %g %g %g\n",m1,Noise[m1],EFAC[flagList[m1]],EQUAD);
			WorkNoise[o]=pow(((((MNStruct *)context)->pulse->obsn[o].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o]],2) + EQUAD;
			
			tdet=tdet+log(WorkNoise[o]);
			WorkNoise[o]=1.0/WorkNoise[o];
			timelike=timelike+pow(Diffvec[o],2)*WorkNoise[o];

	}

	double *NFd = new double[FitCoeff];
	double **FMatrix=new double*[((MNStruct *)context)->pulse->nobs];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		FMatrix[i]=new double[FitCoeff];
	}

	double **NF=new double*[((MNStruct *)context)->pulse->nobs];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		NF[i]=new double[FitCoeff];
	}

	double **FNF=new double*[FitCoeff];
	for(int i=0;i<FitCoeff;i++){
		FNF[i]=new double[FitCoeff];
	}





	double start,end;
	int go=0;
	for (int i=0;i<((MNStruct *)context)->pulse->nobs;i++)
	  {
	    if (((MNStruct *)context)->pulse->obsn[i].deleted==0)
	      {
		if (go==0)
		  {
		    go = 1;
		    start = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		    end  = start;
		  }
		else
		  {
		    if (start > (double)((MNStruct *)context)->pulse->obsn[i].bat)
		      start = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		    if (end < (double)((MNStruct *)context)->pulse->obsn[i].bat)
		      end = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		  }
	      }
	  }
// 	printf("Total time span = %.6f days = %.6f years\n",end-start,(end-start)/365.25);
	double maxtspan=end-start;

	double freqdet=0;
	for (int i=0; i<FitCoeff/2; i++){
		int pnum=pcount;
		double pc=Cube[pcount];
		
		powercoeff[i]=pow(10.0,pc)/(maxtspan*24*60*60);///(365.25*24*60*60)/4;
		powercoeff[i+FitCoeff/2]=powercoeff[i];
		freqdet=freqdet+2*log(powercoeff[i]);
		pcount++;
	}


	int coeffsize=FitCoeff/2;
	std::vector<double>freqs(FitCoeff/2);
	for(int i=0;i<FitCoeff/2;i++){
		freqs[i]=double(i+1)/maxtspan;
	}	
	

	for(int i=0;i<FitCoeff/2;i++){
		for(int k=0;k<((MNStruct *)context)->pulse->nobs;k++){
			
			FMatrix[k][i]=cos(2*M_PI*freqs[i]*((double)((MNStruct *)context)->pulse->obsn[k].bat));
// 			printf("cos %i %i %g \n",i,k,FMatrix[k][i]);
		}
	}

	for(int i=0;i<FitCoeff/2;i++){
		for(int k=0;k<((MNStruct *)context)->pulse->nobs;k++){
			FMatrix[k][i+FitCoeff/2]=sin(2*M_PI*freqs[i]*((double)((MNStruct *)context)->pulse->obsn[k].bat));
// 			printf("sin %i %i %g \n",i+FitCoeff/2,k,FMatrix[k][i+FitCoeff/2]);
		}
	}





// 	makeFourier(((MNStruct *)context)->pulse, FitCoeff, FMatrix);

	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		for(int j=0;j<FitCoeff;j++){
// 			printf("%i %i %g %g \n",i,j,WorkNoise[i],FMatrix[i][j]);
			NF[i][j]=WorkNoise[i]*FMatrix[i][j];
		}
	}
	dgemv(NF,Diffvec,NFd,((MNStruct *)context)->pulse->nobs,FitCoeff,'T');
	dgemm(FMatrix, NF , FNF, ((MNStruct *)context)->pulse->nobs, FitCoeff, ((MNStruct *)context)->pulse->nobs, FitCoeff, 'T', 'N');


	double **PPFM=new double*[FitCoeff];
	for(int i=0;i<FitCoeff;i++){
		PPFM[i]=new double[FitCoeff];
		for(int j=0;j<FitCoeff;j++){
			PPFM[i][j]=0;
		}
	}


	for(int c1=0; c1<FitCoeff; c1++){
		PPFM[c1][c1]=1.0/powercoeff[c1];
	}



	for(int j=0;j<FitCoeff;j++){
		for(int k=0;k<FitCoeff;k++){
// 			printf("%i %i %g %g \n",j,k,PPFM[j][k],FNF[j][k]);
			PPFM[j][k]=PPFM[j][k]+FNF[j][k];
		}
	}

 	
	double jointdet=0;
	dpotrf(PPFM, FitCoeff, jointdet);
        dpotri(PPFM,FitCoeff);

	double freqlike=0;
	for(int i=0;i<FitCoeff;i++){
		for(int j=0;j<FitCoeff;j++){
			//sprintf("%i %i %g %g\n",i,j,NFd[i],PPFM[i][j]);
			freqlike=freqlike+NFd[i]*PPFM[i][j]*NFd[j];
		}
	}
	
	lnew=-0.5*(jointdet+tdet+freqdet+timelike-freqlike);

	if(isnan(lnew) || isinf(lnew)){

		lnew=-pow(10.0,200);
// 		printf("red amp and alpha %g %g\n",redamp,redalpha);
// 		printf("Like: %g %g %g \n",lnew,Chisq,covdet);
		
	}


	delete[] EFAC;
	delete[] WorkNoise;
	delete[] powercoeff;
	delete[] Diffvec;
	delete[] Fitvec;
	delete[] NFd;

	for (int j = 0; j < FitCoeff; j++){
		delete[] PPFM[j];
	}
	delete[] PPFM;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[] NF[j];
	}
	delete[] NF;

	for (int j = 0; j < FitCoeff; j++){
		delete[] FNF[j];
	}
	delete[] FNF;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[] FMatrix[j];
	}
	delete[] FMatrix;

// 	if(isinf(lnew) || isinf(jointdet) || isinf(tdet) || isinf(freqdet) || isinf(timelike) || isinf(freqlike)){
// 	printf("Chisq: %g %g %g %g %g %g \n",lnew,jointdet,tdet,freqdet,timelike,freqlike);
// 	}

}

void LRedMarginLinearLogLike(double *Cube, int &ndim, int &npars, double &lnew, void *context)
{

	int numfit=((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps+1;
	double Fitparams[numfit];
	double *EFAC;
	double EQUAD;
	int pcount=0;
	
// 	printf("here1\n");
	for(int p=0;p<ndim;p++){
// 		printf("param %i %g %g\n",p,((MNStruct *)context)->Dpriors[p][0],((MNStruct *)context)->Dpriors[p][1]);
		Cube[p]=(((MNStruct *)context)->Dpriors[p][1]-((MNStruct *)context)->Dpriors[p][0])*Cube[p]+((MNStruct *)context)->Dpriors[p][0];
	}
// 	printf("here1.5\n");
	for(int p=0;p < numfit; p++){
		Fitparams[p]=Cube[p];
		pcount++;
// 		printf("param: %i %g \n",p,Fitparams[p]);
	}

	if(((MNStruct *)context)->numFitEFAC == 0){
		EFAC=new double[1];
		EFAC[0]=1;
// 		
	}
	else if(((MNStruct *)context)->numFitEFAC == 1){
		EFAC=new double[1];
		EFAC[0]=Cube[pcount];
		pcount++;
	}
	else if(((MNStruct *)context)->numFitEFAC > 1){
		EFAC=new double[((MNStruct *)context)->numFitEFAC];
		for(int p=0;p< ((MNStruct *)context)->numFitEFAC; p++){
			EFAC[p]=Cube[pcount];
			pcount++;
		}
	}				

	if(((MNStruct *)context)->numFitEQUAD == 0){
		EQUAD=0;
// 		printf("EQUAD: %g \n",EQUAD);
	}
	else{
		
		EQUAD=pow(10.0,2*Cube[pcount]);
		pcount++;
// 		printf("EQUAD: %g %g %g %i \n",EQUAD,EQUADPrior[0],EQUADPrior[1],((MNStruct *)context)->numFitTiming + ((MNStruct *)context)->numFitJumps + ((MNStruct *)context)->numFitEFAC);
	}

  	

	double *Fitvec=new double[((MNStruct *)context)->pulse->nobs];
	double *Diffvec=new double[((MNStruct *)context)->pulse->nobs];
	dgemv(((MNStruct *)context)->DMatrix,Fitparams,Fitvec,((MNStruct *)context)->pulse->nobs,numfit,'N');
	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Diffvec[o]=((MNStruct *)context)->pulse->obsn[o].residual-Fitvec[o];
	}


	int FitCoeff=2*(((MNStruct *)context)->numFitRedCoeff);
	double *powercoeff=new double[FitCoeff];


	double *Noise=new double[((MNStruct *)context)->pulse->nobs];
	double *GDiffvec=new double[((MNStruct *)context)->Gsize];

	for(int o=0;o<((MNStruct *)context)->pulse->nobs; o++){
		Noise[o]=pow(((((MNStruct *)context)->pulse->obsn[o].toaErr)*pow(10.0,-6))*EFAC[((MNStruct *)context)->sysFlags[o]],2) + EQUAD;
	}

	double **NG = new double*[((MNStruct *)context)->pulse->nobs]; for (int k=0; k<((MNStruct *)context)->pulse->nobs; k++) NG[k] = new double[((MNStruct *)context)->Gsize];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		for(int j=0;j<((MNStruct *)context)->Gsize; j++){
			NG[i][j]=((MNStruct *)context)->GMatrix[i][j]*Noise[i];

		}
	}

	double** GG = new double*[((MNStruct *)context)->Gsize]; for (int k=0; k<((MNStruct *)context)->Gsize; k++) GG[k] = new double[((MNStruct *)context)->Gsize];

	dgemm(((MNStruct *)context)->GMatrix, NG,GG,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, 'T','N');

	
	double tdet=0;
	dpotrf(GG, ((MNStruct *)context)->Gsize, tdet);
	dpotri(GG,((MNStruct *)context)->Gsize);

	dgemm(((MNStruct *)context)->GMatrix, GG,NG,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, ((MNStruct *)context)->Gsize, ((MNStruct *)context)->Gsize, 'N','N');

	double **GNG = new double*[((MNStruct *)context)->pulse->nobs]; for (int k=0; k<((MNStruct *)context)->pulse->nobs; k++) GNG[k] = new double[((MNStruct *)context)->pulse->nobs];	

	dgemm(NG, ((MNStruct *)context)->GMatrix, GNG,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, ((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->Gsize, 'N','T');


	double timelike=0;
	for(int o1=0; o1<((MNStruct *)context)->pulse->nobs; o1++){
		for(int o2=0;o2<((MNStruct *)context)->pulse->nobs; o2++){
			timelike=timelike+Diffvec[o1]*GNG[o1][o2]*Diffvec[o2];
		}
	}

	double *NFd = new double[FitCoeff];
	double **FMatrix=new double*[((MNStruct *)context)->pulse->nobs];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		FMatrix[i]=new double[FitCoeff];
	}

	double **NF=new double*[((MNStruct *)context)->pulse->nobs];
	for(int i=0;i<((MNStruct *)context)->pulse->nobs;i++){
		NF[i]=new double[FitCoeff];
	}

	double **FNF=new double*[FitCoeff];
	for(int i=0;i<FitCoeff;i++){
		FNF[i]=new double[FitCoeff];
	}

	double start,end;
	int go=0;
	for (int i=0;i<((MNStruct *)context)->pulse->nobs;i++)
	  {
	    if (((MNStruct *)context)->pulse->obsn[i].deleted==0)
	      {
		if (go==0)
		  {
		    go = 1;
		    start = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		    end  = start;
		  }
		else
		  {
		    if (start > (double)((MNStruct *)context)->pulse->obsn[i].bat)
		      start = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		    if (end < (double)((MNStruct *)context)->pulse->obsn[i].bat)
		      end = (double)((MNStruct *)context)->pulse->obsn[i].bat;
		  }
	      }
	  }
// 	printf("Total time span = %.6f days = %.6f years\n",end-start,(end-start)/365.25);
	double maxtspan=end-start;


	double freqdet=0;
	for (int i=0; i<FitCoeff/2; i++){
		int pnum=pcount;
		double pc=Cube[pcount];
		
		powercoeff[i]=pow(10.0,pc)/(maxtspan*24*60*60);///(365.25*24*60*60)/4;
		powercoeff[i+FitCoeff/2]=powercoeff[i];
		freqdet=freqdet+2*log(powercoeff[i]);
		pcount++;
	}


	int coeffsize=FitCoeff/2;
	std::vector<double>freqs(FitCoeff/2);
	for(int i=0;i<FitCoeff/2;i++){
		freqs[i]=double(i+1)/maxtspan;
	}	
	

	for(int i=0;i<FitCoeff/2;i++){
		for(int k=0;k<((MNStruct *)context)->pulse->nobs;k++){
			double time=(double)((MNStruct *)context)->pulse->obsn[k].bat; //- (double)((MNStruct *)context)->pulse->param[param_pepoch].val[0] - maxtspan/2;
			FMatrix[k][i]=cos(2*M_PI*freqs[i]*time);
// 			printf("cos %i %i %g \n",i,k,time);
		}
	}

	for(int i=0;i<FitCoeff/2;i++){
		for(int k=0;k<((MNStruct *)context)->pulse->nobs;k++){
			double time=(double)((MNStruct *)context)->pulse->obsn[k].bat; //- (double)((MNStruct *)context)->pulse->param[param_pepoch].val[0] - maxtspan/2;
			FMatrix[k][i+FitCoeff/2]=sin(2*M_PI*freqs[i]*time);
// 			printf("sin %i %i %g \n",i+FitCoeff/2,k,time);
		}
	}


	dgemm(GNG, FMatrix , NF, ((MNStruct *)context)->pulse->nobs,((MNStruct *)context)->pulse->nobs, ((MNStruct *)context)->pulse->nobs, FitCoeff, 'N', 'N');

	dgemm(FMatrix, NF , FNF, ((MNStruct *)context)->pulse->nobs, FitCoeff, ((MNStruct *)context)->pulse->nobs, FitCoeff, 'T', 'N');

	dgemv(NF,Diffvec,NFd,((MNStruct *)context)->pulse->nobs,FitCoeff,'T');

	double **PPFM=new double*[FitCoeff];
	for(int i=0;i<FitCoeff;i++){
		PPFM[i]=new double[FitCoeff];
		for(int j=0;j<FitCoeff;j++){
			PPFM[i][j]=0;
		}
	}


	for(int c1=0; c1<FitCoeff; c1++){
		PPFM[c1][c1]=1.0/powercoeff[c1];
	}



	for(int j=0;j<FitCoeff;j++){
		for(int k=0;k<FitCoeff;k++){
			
			PPFM[j][k]=PPFM[j][k]+FNF[j][k];
		}
	}

 	
	double jointdet=0;
	dpotrf(PPFM, FitCoeff, jointdet);
        dpotri(PPFM,FitCoeff);

	double freqlike=0;
	for(int i=0;i<FitCoeff;i++){
		for(int j=0;j<FitCoeff;j++){
// 			printf("%i %i %g %g\n",i,j,NFd[i],PPFM[i][j]);
			freqlike=freqlike+NFd[i]*PPFM[i][j]*NFd[j];
		}
	}
	
	lnew=-0.5*(tdet+jointdet+freqdet+timelike-freqlike);

	if(isnan(lnew) || isinf(lnew)){

		lnew=-pow(10.0,200);
// 		printf("red amp and alpha %g %g\n",redamp,redalpha);
// 		printf("Like: %g %g %g \n",lnew,Chisq,covdet);
		
	}


	delete[] EFAC;
	delete[] powercoeff;
	delete[] NFd;

	for (int j = 0; j < FitCoeff; j++){
		delete[]PPFM[j];
	}
	delete[]PPFM;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[]NF[j];
	}
	delete[]NF;

	for (int j = 0; j < FitCoeff; j++){
		delete[]FNF[j];
	}
	delete[]FNF;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[]FMatrix[j];
	}
	delete[]FMatrix;

	delete[] Noise;
	delete[] Diffvec;
	delete[] Fitvec;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[] NG[j];
	}
	delete[] NG;

	for (int j = 0; j < ((MNStruct *)context)->Gsize; j++){
		delete[]GG[j];
	}
	delete[] GG;

	for (int j = 0; j < ((MNStruct *)context)->pulse->nobs; j++){
		delete[] GNG[j];
	}
	delete[] GNG;

// 	if(isinf(lnew) || isinf(jointdet) || isinf(tdet) || isinf(freqdet) || isinf(timelike) || isinf(freqlike)){
// 	printf("Chisq: %g %g %g %g %g %g \n",lnew,jointdet,tdet,freqdet,timelike,freqlike);
// 	}

}

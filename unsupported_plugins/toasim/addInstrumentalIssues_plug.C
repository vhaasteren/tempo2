//  Copyright (C) 2006,2007,2008,2009, George Hobbs, Russell Edwards
//
// Plugin to read a set of arrival time files and produce a list of Gaussian random numbers based on the TOA uncertainties 


/*
 *    This file is part of TEMPO2. 
 * 
 *    TEMPO2 is free software: you can redistribute it and/or modify 
 *    it under the terms of the GNU General Public License as published by 
 *    the Free Software Foundation, either version 3 of the License, or 
 *    (at your option) any later version. 
 *    TEMPO2 is distributed in the hope that it will be useful, 
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *    GNU General Public License for more details. 
 *    You should have received a copy of the GNU General Public License 
 *    along with TEMPO2.  If not, see <http://www.gnu.org/licenses/>. 
 */

/*
 *    If you use TEMPO2 then please acknowledge it by citing 
 *    Hobbs, Edwards & Manchester (2006) MNRAS, Vol 369, Issue 2, 
 *    pp. 655-672 (bibtex: 2006MNRAS.369..655H)
 *    or Edwards, Hobbs & Manchester (2006) MNRAS, VOl 372, Issue 4,
 *    pp. 1549-1574 (bibtex: 2006MNRAS.372.1549E) when discussing the
 *    timing model.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "T2toolkit.h"
#include "tempo2.h"
#include "toasim.h"


extern "C" int graphicalInterface(int argc,char *argv[],pulsar *psr,int *npsr) 
{
  char parFile[MAX_PSR][MAX_FILELEN];
  char timFile[MAX_PSR][MAX_FILELEN];
  int i,nit,j,p;
  char fname[MAX_FILELEN];
  double globalParameter;
  long double result;
  long seed = TKsetSeed();
  
  double instrumentOffset = 40e-9;
  int    instrumentNlevels = 5;
  int    ranInt;

  //
  // For the output file
  //
  toasim_header_t* header;
  toasim_header_t* read_header;
  FILE* file;
  double offsets[MAX_OBSN]; // Will change to doubles - should use malloc
  // Create a set of corrections.
  toasim_corrections_t* corr = (toasim_corrections_t*)malloc(sizeof(toasim_corrections_t));
  
  corr->offsets=offsets;
  // Same length string in every iteration - defined in r_param_length see below
  corr->a0=0; // constant
  corr->a1=0; // a1*x
  corr->a2=0; // a2*x*X
  
  *npsr = 0;
  nit = 1;
  
  printf("Graphical Interface: addInstrumentalIssues\n");
  printf("Author:              G. Hobbs, D. Reardon\n");
  printf("Version:             1.0\n");
  
  /* Obtain all parameters from the command line */
  for (i=2;i<argc;i++)
    {
      
      if (strcmp(argv[i],"-nreal")==0){
	nit=atoi(argv[++i]);
      }
      if (strcmp(argv[i],"-f")==0)
	{
	  strcpy(parFile[*npsr],argv[++i]); 
	  strcpy(timFile[*npsr],argv[++i]);
	  (*npsr)++;
	}
      if (strcmp(argv[i],"-seed")==0){
	sscanf(argv[++i],"%ld",&seed);
      }
      if (strcmp(argv[i],"-o")==0) // Offset spacing
	sscanf(argv[++i],"%lf",&instrumentOffset);
      if (strcmp(argv[i],"-n")==0) // Levels
	sscanf(argv[++i],"%d",&instrumentNlevels);
    }
  
  if (seed > 0)seed=-seed;
  
  readParfile(psr,parFile,timFile,*npsr); /* Load the parameters       */
  // Now read in all the .tim files
  readTimfile(psr,timFile,*npsr); /* Load the arrival times    */
  
  preProcess(psr,*npsr,argc,argv);
  
  for (p=0;p<*npsr;p++)
    {
      printf("NTOA = %d\n",psr[p].nobs);
      header = toasim_init_header();
      strcpy(header->short_desc,"addInstrumentalIssues");
      strcpy(header->invocation,argv[0]);
      strcpy(header->timfile_name,timFile[p]);
      strcpy(header->parfile_name,"Unknown");
      header->seed = seed;
      
      header->ntoa = psr[p].nobs;
      header->nrealisations = nit;
      
      // First we write the header...
      sprintf(fname,"%s.addInstrument",timFile[p]);
      file = toasim_write_header(header,fname);
      
      
      for (i=0;i<nit;i++)
	{
	  if(i%10 == 0){
	    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
	    printf("Iteration %d/%d",i+1,nit);
	    fflush(stdout);
	  }
	  for (j=0;j<psr[p].nobs;j++){
	    {
	      ranInt = (int)(TKranDev(&seed)*instrumentNlevels+0.5)-instrumentNlevels/2;
	      offsets[j] = ranInt*instrumentOffset;
	    }
	  }


	  toasim_write_corrections(corr,header,file);
	}
      printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
      printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
      printf("Iteration %d/%d\n",i,nit);
      
      printf("Close file\n");
      fclose(file);
    }
  return 0;
}

const char * plugVersionCheck = TEMPO2_h_VER;

/****************************************************************************/
/*                                matropt.c                                 */
/****************************************************************************/
/*                                                                          */
/* MATRix operation OPTimization for laspack                                */
/*                                                                          */
/* Copyright (C) 1992-1995 Tomas Skalicky. All rights reserved.             */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*        ANY USE OF THIS CODE CONSTITUTES ACCEPTANCE OF THE TERMS          */
/*              OF THE COPYRIGHT NOTICE (SEE FILE COPYRGHT.H)               */
/*                                                                          */
/****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <laspack/errhandl.h>
#include <laspack/vector.h>
#include <laspack/qmatrix.h>
#include <laspack/operats.h>
#include <laspack/version.h>
#include <laspack/copyrght.h>

#include <xc/getopts.h>

#include "testproc.h"

#define MAX_TEST_NR 10

static void GenMatr(QMatrix *Q);
static void PrintHelp(void);

int main(int argc, char **argv)
{
    double Factor, ClockLASPack = 0.0, Clock[MAX_TEST_NR + 1];
    int TestNo, NoCycles, Test, Cycle;
    clock_t BegClock, EndClock;
    size_t Dim;
    Boolean Help;
    QMatrix A;
    Vector x, y;

    OptTabType OptTab;

    /* initial message */
    fprintf(stderr, "matropt             Version %s\n", LASPACK_VERSION);
    fprintf(stderr, "                    (C) 1992-1995 Tomas Skalicky\n");
    fprintf(stderr, "                    Use option -h for help.\n");

    /* generate options table */
    OptTab.No = 3;
    OptTab.Descr = (OptDescrType *)malloc(OptTab.No * sizeof(OptDescrType));
    if (OptTab.Descr != NULL) {
        /* boolean option */
        OptTab.Descr[0].KeyChar = 'd';
        OptTab.Descr[0].Type = SizeOptType;
        OptTab.Descr[0].Variable = (void *)&Dim;

        /* int option */
        OptTab.Descr[1].KeyChar = 'c';
        OptTab.Descr[1].Type = IntOptType;
        OptTab.Descr[1].Variable = (void *)&NoCycles;

        /* option for help */
        OptTab.Descr[2].KeyChar = 'h';
        OptTab.Descr[2].Type = BoolOptType;
        OptTab.Descr[2].Variable = (void *)&Help;

        /* initialize variables with default values */
#if defined(__MSDOS__) || defined(MSDOS)
        /* the most MS-DOS compilers use size_t of 16 bits size, which rescricts
	   arrays to maximal 65536 / 8 = 8192 double elements */
	Dim = 1000;
#else
        Dim = 10000;
#endif	    
        NoCycles = 10;
        Help = False;

        /* analyse options in the command line */
        GetOpts(&argc, argv, &OptTab);
        if (OptResult() == OptOK && !Help) {
            /* construction of test matrix and vectors */
            /* rem.: matrix A have to be nonsymmetric, because 
               the test procedures can not handle other types */ 
            Q_Constr(&A, "A", Dim, False, Rowws, Normal, True);
            V_Constr(&x, "x", Dim, Normal, True);
            V_Constr(&y, "y", Dim, Normal, True);
            GenMatr(&A);

            printf("\n");
            printf("This program tests efficiency of several implementations\n");
            printf("of the matrix by vector product:\n\n");
            printf("    <Vector 2> = <Matrix> <Vector 1>.\n");
	    fprintf(stderr, "\n");

            Factor = 1.0 / CLOCKS_PER_SEC / NoCycles;

            Test = 0;

            /* implementation as in the current LASPack version */

            if (LASResult() == LASOK) {
	        fprintf(stderr, ".");
                V_SetAllCmp(&x, 0.0);
                V_SetAllCmp(&y, 0.0);

                Asgn_VV(&y, Mul_QV(&A, &x));
            
                BegClock = clock();
                for(Cycle = 1; Cycle <= NoCycles; Cycle++) {
                    Asgn_VV(&y, Mul_QV(&A, &x));
                }
                EndClock = clock();
                ClockLASPack = (double)(EndClock - BegClock) * Factor;
            }

            /* a very simple implementation */

            Test++;
            if (LASResult() == LASOK) {
	        fprintf(stderr, ".");
                V_SetAllCmp(&x, 0.0);
                V_SetAllCmp(&y, 0.0);

                BegClock = clock();
                for(Cycle = 1; Cycle <= NoCycles; Cycle++) {
                    Asgn_VV(&y, Test1_QV(&A, &x));
                }
                EndClock = clock();
                Clock[Test] = (double)(EndClock - BegClock) * Factor;
            }

            /* implementation like LASPack version 0.06 */

            Test++;
            if (LASResult() == LASOK) {
	        fprintf(stderr, ".");
                V_SetAllCmp(&x, 0.0);
                V_SetAllCmp(&y, 0.0);

                BegClock = clock();
                for(Cycle = 1; Cycle <= NoCycles; Cycle++) {
                    Asgn_VV(&y, Test2_QV(&A, &x));
                }
                EndClock = clock();
                Clock[Test] = (double)(EndClock - BegClock) * Factor;
            }
    
            /* implementation using local variables and pointers,
               ascended counting of matrix elements */

            Test++;
            if (LASResult() == LASOK) {
	        fprintf(stderr, ".");
                V_SetAllCmp(&x, 0.0);
                V_SetAllCmp(&y, 0.0);

                BegClock = clock();
                for(Cycle = 1; Cycle <= NoCycles; Cycle++) {
                    Asgn_VV(&y, Test3_QV(&A, &x));
                }
                EndClock = clock();
                Clock[Test] = (double)(EndClock - BegClock) * Factor;
            }

            /* implementation using local variables and pointers,
               descended counting of matrix elements */

            Test++;
            if (LASResult() == LASOK) {
	        fprintf(stderr, ".");
                V_SetAllCmp(&x, 0.0);
                V_SetAllCmp(&y, 0.0);

                BegClock = clock();
                for(Cycle = 1; Cycle <= NoCycles; Cycle++) {
                    Asgn_VV(&y, Test4_QV(&A, &x));
                }
                EndClock = clock();
                Clock[Test] = (double)(EndClock - BegClock) * Factor;
            }

            fprintf(stderr, "\n");
            TestNo = Test;

            /* output of results */

            if (LASResult() == LASOK) {
                printf("\n");
                printf("Results:    (for dimension %lu, cycles %d)\n",
                    (unsigned long)Dim, NoCycles);
                printf("--------\n\n");
                printf(" implementation    CPU time\n");
                printf("-----------------------------------------\n");
                if (ClockLASPack > 1e-5)
                    printf("    LASPack    %12.3e s = 100.0 %%\n", ClockLASPack);
                for (Test = 1; Test <= TestNo; Test++) {
                    if (Clock[Test] > 1e-5)
                        printf("      %2d       %12.3e s = %5.1f %%\n",
                            Test, Clock[Test], 100.0 * Clock[Test] / ClockLASPack);
                }
                printf("-----------------------------------------\n\n");
                printf("For details to the implementations look at the file\n");
		printf("laspack/examples/matropt/testproc.c.\n");
		printf("The current LASPack version corresponds to the implementation 3.\n");
		printf("This is, in comparison with LASPack, simplified and can be therefore\n");
		printf("a little faster.\n");
            }

            /* LASPack error messages */
            if (LASResult() != LASOK) {
                fprintf(stderr, "\n");
                fprintf(stderr, "LASPack error: ");
                WriteLASErrDescr(stderr);     
            }

            /* destruction of test matrix and vectors */
            Q_Destr(&A);
            V_Destr(&x);
            V_Destr(&y);
        } else {
            /* error messages */
            if (OptResult() == OptNotDefErr || OptResult() == OptSyntaxErr || Help) {
                fprintf(stderr, "\n");
                PrintHelp();
            }
            if (OptResult() == OptDescrErr) {
                fprintf(stderr, "\n");
                fprintf(stderr, "Description of an option faulty.\n");
            }
        }
        
        if (OptTab.Descr != NULL) 
            free(OptTab.Descr);
    } else {
        /* error message */
        fprintf(stderr, "\n");
        fprintf(stderr, "Not enought memory for analysis of command line options.\n");
    }
    
    return(0);
}

static void GenMatr(QMatrix *Q)
/* generate matrix Q with a given number of subdiagonals 
   for both triangular parts of the matrix */
{
    size_t NoSubdiag = 3;

    size_t Dim, RoC, Len1, Len2, Entry, Subdiag, Pos;
    Boolean Symmetry;
    ElOrderType ElOrder;

    if (LASResult() == LASOK) {
        Dim = Q_GetDim(Q);
	Symmetry = Q_GetSymmetry(Q);
	ElOrder = Q_GetElOrder(Q);
        for (RoC = 1; RoC <= Dim; RoC++) {
            if (RoC + NoSubdiag <= Dim)
	        Len1 = NoSubdiag;
	    else
	        Len1 = Dim - RoC;
	    if (RoC > NoSubdiag)
	        Len2 = NoSubdiag;
	    else
	        Len2 = RoC - 1;
	    if (Symmetry) {
	        if (ElOrder == Rowws)
                    Q_SetLen(Q, RoC, Len1 + 1);
	        if (ElOrder == Clmws)
                    Q_SetLen(Q, RoC, Len2 + 1);
	    } else {
                Q_SetLen(Q, RoC, Len1 + Len2 + 1);
	    }
            if (LASResult() == LASOK) {
	        /* main diagonal entry */
	        Entry = 0;
		Q__SetEntry(Q, RoC, Entry, RoC, 1.0);
		/* entries for subdiagonals in both triagonal parts */
	        for (Subdiag = 1; Subdiag <= NoSubdiag ; Subdiag++) {
		    Pos = RoC + Subdiag;
	            if ((!Symmetry || (ElOrder == Rowws)) && Pos <= Dim) {
		        Entry++;
  	                Q__SetEntry(Q, RoC, Entry, Pos, 1.0);
		    }
  	            if ((!Symmetry || (ElOrder == Clmws)) && RoC > Subdiag) {
		        Pos = RoC - Subdiag;
 	                Entry++;
    	                Q__SetEntry(Q, RoC, Entry, Pos, 1.0);
	            }
		}
	    }
	}
    }
}

static void PrintHelp(void)
/* print help for syntax and avaible options */
{
    fprintf(stderr, "Syntax:\n");
    fprintf(stderr, "  matropt [-d<dim>] [-c<cycles>] [-h]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d  set matrix dimension to <dim>, default is %lu\n",
#if defined(__MSDOS__) || defined(MSDOS)
	    (unsigned long)1000);
#else
            (unsigned long)10000);
#endif	    
    fprintf(stderr, "  -c  set number of cycles to <cycles>, default is 10\n");
    fprintf(stderr, "  -h  print this help\n");
}


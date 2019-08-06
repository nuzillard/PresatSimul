/*
 * Calculation of multiple presaturation profiles in NMR using Bloch equations (approximate solution)
 * Precession and relaxation are treated here sequencially over "small" time intervals.
 * This code is provided as is, without any warranty of any kind.
 *
 * Compilation depends on:
 * - libxml2 and on its dependencies
 * - libsimu1 from https://github.com/nuzillard/Libsimu1
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libsimu1.h>
#include "xmlvalues.h"

#define VERSION 2

/* a variable of the new "data" type contains all the problem-related parameters */
typedef struct
{
/* nucleus relaxation times (s) and relaxation rates (/s) */
	double T1, T2, R1, R2 ;
/* presaturation lasts for d1, cut into shaped pulse durations of bigpulse (s) */
/* each shaped pulse is made of elementary pulses of length onepulse (s) */
	double d1, bigpulse, onepulse ;
/* each shaped pulse is made of npulse elementary pulses */
/* the evolution during each elementary pulse is cut into nsubstep steps */
	int npulse, nsubstep ;
/* nu0 is the offset of the currently studied nucleus, which varies from nu0min to nu0max, in dnu0 steps (all Hz) */
/* the offset om0 and offset increment dom0 are like nu0 and dnu0 but in rad/s and not in Hz */
	double nu0, nu0min, nu0max, dnu0, om0, dom0 ;
/* nnu0 is the number of nu0 increments between nu0min and nuOmax, both included */ 
	int nnu0 ;
/* nseq is the number of times the shaped pulse (bigpulse) is repeated during the relaxation delay (d1)*/
	int nseq ;
/* nu1 is the intensity of the radiofrequency pulse for presaturation (Hz) */
	double nu1 ;
/* mzeq is the value of the z component of the equilibrium magnetisation, equal to 1.0 */
	double mzeq ;
/* m stores the rotation matrix of an elementary pulse */
	Mat33 m ;
/* ax stores the components of the rotation axis associated to an elementary pulse */
/* v1 and v2 store magnetization components */
	Vec3 ax, v1, v2 ;
/* shifts points to table of shaped pulse modulation frequencies (Hz) */
/* om1 is like nu1 but in rad/s instead of Hz */
	double *shifts, om1 ;
/* nshift is the number of phase modulations of the shaped pulse */
	int nshift ;
/* om1x and om1y define the shape of the presaturation pulse, x and y components of the B1 field */
	double *om1x, *om1y ;
/* any rotation speed (rad/s) below tol is considered as null */
	double tol ;
} data ;

/* pdata is a pointer type for a data structure */
typedef data *pdata ;


/*
 * relax calculates the effect of relaxation on the magnetization in p->v2 during dt
 * and stores the result in p->v1. relax uses the exact solution of relaxation-only equations.
 */
void relax(pdata p, double dt)
{
/* f2 is the scaling factor associated to transverse relaxation */
	double f2 ;
	double mzeq ;

/* relaxation at rate p->R2 for a time p->onepulse */
	f2 = exp(-p->R2 * dt) ;
/* equilibrium magnetization */
	mzeq = p->mzeq ;
/* calculate and store in p->v1 the result of the action of transverse relaxation */
	p->v1[0] = p->v2[0] * f2 ;
	p->v1[1] = p->v2[1] * f2 ;
/* calculate and store in p->v1 the result for longitudinal relaxation on p->v2 according to p->R1 */
	p->v1[2] = mzeq + (p->v2[2] - mzeq) * exp(-p->R1 * dt) ;
}


/*
 * step transforms and stores in p->v1 the magnetization components in p->v1 according to
 * the current nucleus precession frequency, relaxation rates, and elementary pulse duration.
 */
void step(pdata p, int ipulse)
{
/* omeff: effective rotation frequency, angle: the rotation angle, dt: substep duration */
/* om1x and om1y: the current components of the B1 field (in rad/s) along the X and Y axis */
	double omeff, angle, om1x, om1y, dt ;
/* isubstep is the substep counter, from 0 to nsubstep excluded */
	int isubstep, nsubstep ;

/* extract current components of the presaturation field */	
	om1x = p->om1x[ipulse] ;
	om1y = p->om1y[ipulse] ;
/* calculate effective rotation frequency */
	omeff = hypot(p->om0, hypot(om1x, om1y)) ;
/* do successive transformations of p->v1 over calculation substeps if omeff is not considered null */
	if(omeff > p->tol) {
/* get number of substeps */
		nsubstep = p->nsubstep ;
/* dt is the duration of a substep */
		dt = p->onepulse / nsubstep ;
/* calculate rotation angle from rotation frequency and duration */
		angle = omeff * dt;
/* define and normalize the vector that defines the rotation axis */		
		p->ax[0] = om1x / omeff ;
		p->ax[1] = om1y / omeff ;
		p->ax[2] = p->om0 / omeff ;
/* calculate the rotation matrix */		
		matrot(p->m, p->ax, angle) ;
/* iterate over substeps */
		for (isubstep = 0 ; isubstep < nsubstep ; isubstep++) {
/* apply rotation matrix to p->v1 */
			matvec(p->m, p->v1, p->v2) ;
/* relax during dt */
			relax(p, dt) ;			
		}
/* no substep if there is no rotation, apply relaxation during p->onepulse only */
	} else {
/* copy v1 in v2 for no-rotation during p->onepulse */
		p->v2[0] = p->v1[0] ;
		p->v2[1] = p->v1[1] ;
		p->v2[2] = p->v1[2] ;	
/* relax during p->onepulse */
		relax(p, p->onepulse) ;
	}
}

/*
 * onetraj applies the presaturation RF field during p->d1 to a nucleus 
 * whose precession frequency is p->om0. The final magnetization componenets are stores in p->v1.
 */
void onetraj(pdata p)
{
	int ipulse, iseq ;
/* set initial magnetization vector */	
	p->v1[0] = 0.0 ;
	p->v1[1] = 0.0 ;
	p->v1[2] = p->mzeq ;
/* iterate on the number of shaped pulses */
	for (iseq = 0 ; iseq < p->nseq ; iseq++) {
/* iterate on elementary pulses within each replicate of the shaped pulse */
		for(ipulse = 0 ; ipulse < p->npulse ; ipulse++) {
			step(p, ipulse) ;
		}
	}
}

/*
 * alltraj calculates magnetization trajectories starting from equilibrium
 * for a nucleus whose precession frequency (Hz) varies from p->nu0min to p->nu0max (all included)
 * in p->nnu0 steps (1 + p->nnu0 trajectories).
 * The current precession frequency and the amount of final z magnetisation are printed
 * for each trajectory.
 */
void alltraj(pdata p)
{
/* inu0 is the index of the current trajectory simulation */
	int inu0 ;
/* loop over precession frequencies */
	for (inu0 = 0 ; inu0 <= p->nnu0 ; inu0++) {
/* calculate the magnetization trajectory corresponding to p->nu0 as nucleus precession frequency */
		onetraj(p) ;
/* prints the nucleus precession frequency (Hz) and the corresponding final amount of z magnetization */
		printf("%9.3f\t%9.6f\n", p->nu0, p->v1[2]) ;
/* update precession frequency and precession angular frequency */
		p->nu0 += p->dnu0 ;
		p->om0 += p->dom0 ;
	}
}

/*
 * prepom1xy calculates the shape of the pulse used for multiple presaturation
 * and stores the result in p->om1x and p->om1y.
 * This implements the concept of shifted laminar pulses (SLP).
 */
void prepom1xy(pdata p)
{
/* npulse elementary pulses indexed by i */
	int i, npulse ;
/* phi: current phase of an elementary pulse , dphi: phase increment between elementary pulses */
/* om1: the p->om1 RF field intensity (rad/s) is equally split between the individual phase modulations */
	double phi, dphi, om1 ;
/* ishift is the current index of the shape modulation, nshift phase modulations */
	int ishift, nshift ;
	double shift ;
/* memory allocation for the storage of the shaped pulse definition, set values to 0 with calloc */ 
	npulse = p->npulse ;
	p->om1x = (double *)calloc(2 * npulse, sizeof(double)) ;
	p->om1y = p->om1x + npulse ;
/* get number of modulations and calculate RF field intensity for each modulation (equal repartition) */
	nshift = p->nshift ;
	om1 = p->om1 / nshift ;
/* loop over modulations */
	for(ishift = 0 ; ishift < nshift ; ishift++) {
/* get current modulation frequency and calculate the corresponding phase increment after each elementary pulse */
		shift = p->shifts[ishift] ;
		dphi = 2 * M_PI * shift * p->onepulse ;
/* loop over elementary pulses */		
		for(i = 0, phi = 0 ; i < npulse ; i++, phi += dphi) {
/* accumulate the current phase modulated B1 field values (unscaled by om1) over the preceding ones (if any */
			p->om1x[i] += cos(phi) ;
			p->om1y[i] += sin(phi) ;		
		}
	}
/* scale om1x and om1y values according to om1, which is the same for all modulations */
	for(i = 0 ; i < 2 * npulse ; i++) {
		p->om1x[i] *= om1 ;
	}
}

int main(int argc, char **argv)
{
	char *defaultfilename = "presat.xml";
	char *filename;
	char **valuesarray = NULL;
	int ishift;
	int version;
/* d is the parameter set of the current simulation, whose address is p */
	data d ;
	pdata p = &d ;

	if(argc > 2) {
		fprintf(stderr, "usage: presat6 [xml-file.xml]. Default is %s\n", defaultfilename);
		exit(1);
	}
	filename = (argc == 1) ? defaultfilename : argv[1];
	
	valuesarray = getvaluesarray(filename);
	version = getintvalue(&valuesarray);
	if(version != VERSION) {
		fprintf(stderr, "Bad version of data file. Expected %d, got %d\n", VERSION, version);
		freevaluesarray(&valuesarray);
		exit(1);
	}
	p->T1 = getdoublevalue(&valuesarray);
	p->T2 = getdoublevalue(&valuesarray);
	p->nnu0 = getintvalue(&valuesarray);
	p->nu0min = getdoublevalue(&valuesarray);
	p->nu0max = getdoublevalue(&valuesarray);
	p->nshift = getintvalue(&valuesarray);
	p->shifts = (double *)malloc(p->nshift * sizeof(double)) ;
	for(ishift = 0 ; ishift < p->nshift ; ishift++) {
		p->shifts[ishift] = getdoublevalue(&valuesarray);
	}
	p->d1 = getdoublevalue(&valuesarray);
	p->bigpulse = getdoublevalue(&valuesarray);
	p->npulse = getintvalue(&valuesarray);
	p->nsubstep = getintvalue(&valuesarray);
	p->nu1 = getdoublevalue(&valuesarray);
	freevaluesarray(&valuesarray);
/* uncomment the following commented section to check for parameter reading only */
/*
	printf("T1: %.3f\n", p->T1);
	printf("T2: %.3f\n", p->T2);
	printf("nnu0: %d\n", p->nnu0);
	printf("nu0min: %.3f\n", p->nu0min);
	printf("nu0max: %.3f\n", p->nu0max);
	printf("nshift: %d\n", p->nshift);
	for(ishift = 0 ; ishift < p->nshift ; ishift++) {
		printf("shift %d: %.3f\n", ishift+1, p->shifts[ishift]);
	}
	printf("d1: %.3f\n", p->d1);
	printf("bigpulse: %.3f\n", p->bigpulse);
	printf("npulse: %d\n", p->npulse);
	printf("nsubstep: %d\n", p->nsubstep);
	printf("nu1: %.3f\n", p->nu1);
	exit(0);
*/
/* calculated parameters */	
	p->mzeq = 1.0 ;
	p->tol = 1.0e-5 ;
	p->R1 = 1.0 / p->T1 ;
	p->R2 = 1.0 / p->T2 ;
	p->nseq = (int)(round(p->d1 / p->bigpulse)) ;
	p->onepulse = p->bigpulse / p->npulse ;
	p->nu0 = p->nu0min ;
	p->dnu0 = (p->nu0max - p->nu0min) / p->nnu0 ;
	p->om0 = 2 * M_PI * p->nu0 ;
	p->dom0 = 2 * M_PI * p->dnu0 ;
	p->om1 = 2 * M_PI * p->nu1 ;
	
/* calculate the shape of the multiple frequency saturation pulse */
	prepom1xy(p) ;
/* use pulse shape for the calculation of the corresponding presaturation profile */
	alltraj(p) ;
/* free dynamically allocated memory */	
	free(p->shifts) ;
	free(p->om1x) ;
/* That's all folks! */
	return 0 ;
}

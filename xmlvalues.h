/*
 * File: xmlvalues.h
 */

/* 
 *getvaluesarray() returns an array of pointers to leaf values in XML file named filename
 */
char **getvaluesarray(char *filename);

/*
 * getvalue() returns next pointer to value from the address of the array of pointers to values.
 * returns NULL if the end of the last value was reterned from the previous call.
 * return of NULL also deallocates memory allocated for the newline-separated values string
 * and for the array of pointers to values.
 */
char *getvalue(char ***pvaluesarray);

/*
 * getdoublevalue() returns the next value as a double. exit(1) in case of failure.
 */
double getdoublevalue(char ***pvaluesarray);

/*
 * getintvalue() returns the next value as a int. exit(1) in case of failure.
 */
int getintvalue(char ***pvaluesarray);

/*
 * getstringvalue() returns the next value as a string. Same as getvalue().
 */
char *getstringvalue(char ***pvaluesarray);

/*
 * freevaluesarray() forces the array of pointers to values to be read up to the end
 * in order to force memory deallocation.
 */
void freevaluesarray(char ***pvaluesarray);

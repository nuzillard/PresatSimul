/*
 * xmlvalues.c provides functions for extracting the values stored
 * in a simple xml file such as presat.xml.
 * This code is based on
 * http://xmlsoft.org/tutorial/apd.html
 * and is provided with any warranty of any kind.
 * No clean exception handling yet, sorry.
 */

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <string.h>

/* 
 * compile with gcc -c `xml2-config --cflags` xmlvalues.c
 * link with gcc -o target target.o xmlvalues.o `xml2-config --libs`
 */

xmlDocPtr
getdoc (char *docname) {
	xmlDocPtr doc;
	doc = xmlParseFile(docname);
	
	if (doc == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return NULL;
	}

	return doc;
}

xmlXPathObjectPtr
getnodeset (xmlDocPtr doc, xmlChar *xpath){
	
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	context = xmlXPathNewContext(doc);
	if (context == NULL) {
		printf("Error in xmlXPathNewContext\n");
		return NULL;
	}
	result = xmlXPathEvalExpression(xpath, context);
	xmlXPathFreeContext(context);
	if (result == NULL) {
		printf("Error in xmlXPathEvalExpression\n");
		return NULL;
	}
	if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
		xmlXPathFreeObject(result);
                printf("No result\n");
		return NULL;
	}
	return result;
}

/* reader() reads an XML document and returns the values of the leaves as a single string.
 * Each value is finished by a newline, so that the returned value can be readily printed.
 * the user is responsible for memory deallocation.
 */
char *
reader(char *docname) {

	
	xmlDocPtr doc;
	xmlChar *xpath = (xmlChar*) "//*";
	xmlNodeSetPtr nodeset;
	xmlXPathObjectPtr result;
	xmlChar *value;
	int i;
	int len, k;
	char c;
	char *values = NULL;
	int valueslen = 0;

/* values will contain the values from the .xml file, concatenated and separated by newlines */	
	values = (char *)malloc(sizeof(char));
/* values starts with an empty character string */
	values[0] = '\0';
/* the length of the values string is 1 */
	valueslen = 1;

	doc = getdoc(docname);
	result = getnodeset (doc, xpath);
	if (result) {
		nodeset = result->nodesetval;
		for (i=0; i < nodeset->nodeNr; i++) {
			value = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
			len = strlen(value);
			for(k=0 ; k<len ; k++) {
				c = value[k];
				if(c != ' ' && c != '\n') {
					break;
				}
			}
/* if value does not contain only spaces and newlines, it contains something useful */
			if(k != len) {
/* the length of values is augmented by the length of value, plus one for the added newline */
				valueslen += (len + 1);
/* entend values to reveive value and the newline */
				values = (char *)realloc(values, (valueslen * sizeof(char)));
/* update values */
				strcat(values, value);
				strcat(values, "\n");
			}
			xmlFree(value);
		}
		xmlXPathFreeObject (result);
	}
	xmlFreeDoc(doc);
	xmlCleanupParser();

/* return the address of the result of XMLPath analysis */
	return values;
}

/*
 * countvalues() counts the number of values in values by counting the number of newline characters
 */
int
countvalues(char *values)
{
	char *p = values;
	int count = 0;
	char c;
	
	while(c = *p) {
		if(c == '\n') {
			count++;
		}
		p++;
	}
	return count;
}

/*
 * splitvalues() returns an array of count pointers to values in values
 */
char **
splitvalues(char *values, int count)
{
	char **pvalues;
	int i = 0;
	char c;
	char *p;
	char *start;

/* scanning the values string for the beginning */
	p = values;
/* the first value starts where values start */
	start = values;
/* the size of the allocated array is the number of values, +1 for NULL, +1 for the address of values */
/* storing the address of values here allows for freeing the memory allocated for values by reader() */
	pvalues = (char **)malloc((count+2) * sizeof(char *));
/* NULL after the count value addresses */
	pvalues[count] = NULL;
/* stores the address of values, needed to free memory */
	pvalues[count+1] = values;
/* scans values for \n to delimit each value */
	while((c = *p) && (i<count)) {
/* if a newline is found */
		if (c == '\n') {
/* newline replaced by end-of-string (\0) */
			*p = '\0';
/* stores the position of the string in pvalues, ready for the next one */
			pvalues[i++] = start;
/* next value starts immediately after the place of the end-of-string */
			start = p+1;
		}
/* analyse next character in values */
		p++;
	}
	
	return pvalues;
}

/* 
 *getvaluesarray() returns an array of pointers to leaf values in XML file named filename
 */
char **
getvaluesarray(char *filename)
{
	char *values;
	int nvalues;

/* get the newline-separated values*/
	values = reader(filename);
/* get the number of values */
	nvalues = countvalues(values);
/* return the array of the pointers to the values */
	return splitvalues(values, nvalues);
}

/*
 * getvalue() returns next pointer to value from the address of the array of pointers to values.
 * returns NULL if the end of the last value was reterned from the previous call.
 * return of NULL also deallocates memory allocated for the newline-separated values string
 * and for the array of pointers to values.
 */
char *
getvalue(char ***pvaluesarray)
{
	static int index = 0;
	char **valuesarray;
	char *value;
	
	valuesarray = *pvaluesarray;
/* returns NULL if the array of pointers to values is not allocated yet or was deallocated */
	if(valuesarray == NULL) {
		return NULL;
	}
/* get pointer to current value */
	value = valuesarray[index++];
/* the current value is NULL because all the values were already extracted from the XML file */
	if(value == NULL) {
/* valuesarray[index] points to the values string and is ready for deallocation */
		free(valuesarray[index]);
/* deallocate array of pointers to strings */
		free(valuesarray);
/* set to NULL the address of the array of pointer to strings in the caller function */
		*pvaluesarray = NULL;
	}
	return value;
}

/*
 * freevaluesarray() forces the array of pointers to values to be read up to the end
 * in order to force memory deallocation.
 */
void
freevaluesarray(char ***pvaluesarray)
{
	char *value;

/* if no allocation or deallocation already done, the nothing to do. */
	if(*pvaluesarray == NULL) {
		return;
	}
/* get values up to the end to force memory deallocation */
	while(value = getvalue(pvaluesarray)) {
	}
}

/*
 * getdoublevalue() returns the next value as a double. exit(1) in case of failure.
 */
double
getdoublevalue(char ***pvaluesarray)
{
	double d;
	int n;
	
	n = sscanf(getvalue(pvaluesarray), "%lf", &d);
	if(n == 0) {
		freevaluesarray(pvaluesarray);
		exit(1);
	}
	return d;
}

/*
 * getintvalue() returns the next value as a int. exit(1) in case of failure.
 */
int
getintvalue(char ***pvaluesarray)
{
	int i;
	int n;
	
	n = sscanf(getvalue(pvaluesarray), "%d", &i);
	if(n == 0) {
		freevaluesarray(pvaluesarray);
		exit(1);
	}
	return i;
}

/*
 * getstringvalue() returns the next value as a string. Same as getvalue().
 */
char *
getstringvalue(char ***pvaluesarray)
{
	return getvalue(pvaluesarray);
}

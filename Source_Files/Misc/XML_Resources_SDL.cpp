/*
 *  XML_Resources_SDL.h - Parser for XML-containing resource files
 *
 *  Written in 2000 by Christian Bauer
 */

#include "cseries.h"
#include "XML_Resources_SDL.h"
#include "resource_manager.h"

#include <stdio.h>
#include <vector>
#include <algorithm>


/*
 *  Gets some XML data to parse
 */

bool XML_Resources_SDL::GetData()
{
	if (data == NULL)
		return false;

	Buffer = (char *)data;
	BufLen = data_size;
	LastOne = true;

	return true;
}


/*
 *  Reports a read error
 */

void XML_Resources_SDL::ReportReadError()
{
	fprintf(stderr, "Error in reading resources\n");
	exit(1);
}


/*
 *  Reports an XML parsing error
 */

void XML_Resources_SDL::ReportParseError(const char *ErrorString, int LineNumber)
{
	fprintf(stderr, "XML parsing error: %s at line %d\n", ErrorString, LineNumber);
	exit(1);
}


/*
 *  Reports an interpretation error
 */

const int MaxErrorsToShow = 7;

void XML_Resources_SDL::ReportInterpretError(const char *ErrorString)
{
	if (GetNumInterpretErrors() < MaxErrorsToShow)
		fprintf(stderr, ErrorString);
}


/*
 *  Requests aborting of parsing (reasonable if there were lots of errors)
 */

bool XML_Resources_SDL::RequestAbort()
{
	return (GetNumInterpretErrors() >= MaxErrorsToShow);
}


/*
 *  Parse all resources in a set
 */

bool XML_Resources_SDL::ParseResourceSet(uint32 Type)
{
	int num = CountResources(Type);
	if (num <= 0)
		return false;

	// Get sorted list of available resource IDs
	vector<int> ids;
	GetResourceIDList(Type, ids);
	sort(ids.begin(), ids.end());

	// Parse each resource
	vector<int>::const_iterator i, end = ids.end();
	for (i=ids.begin(); i!=end; i++) {
		data = GetResource(Type, *i, &data_size);
		if (!DoParse()) {
			fprintf(stderr, "There were configuration file parsing errors\n");
			exit(1);
		}
		free(data);
		data = NULL;
	}
}

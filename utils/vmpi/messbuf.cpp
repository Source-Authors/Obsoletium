// Copyright Valve Corporation, All rights reserved.
//
//

#include "messbuf.h"

#include <cstdlib>
#include <cstring>

#include "tier1/strtools.h"


///////////////////////////
//
//
//
MessageBuffer::MessageBuffer()
{
	size = DEFAULT_MESSAGE_BUFFER_SIZE;
	data = (char *) malloc(size);
	len = 0;
	offset = 0;
}

///////////////////////////
//
//
//
MessageBuffer::MessageBuffer(ptrdiff_t minsize)
{
	size = minsize;
	data = (char *) malloc(size);
	len = 0;
	offset = 0;
}

///////////////////////////
//
//
//
MessageBuffer::~MessageBuffer()
{
	free(data);
}

///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::setLen(ptrdiff_t nlen)
{
	if (nlen < 0) return -1;
	if (nlen > size) {
		resize(nlen);
	}

	return std::exchange(len, nlen);
}

///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::setOffset(ptrdiff_t noffset)
{
	if (noffset < 0 || noffset > len) return -1;

	return std::exchange(offset, noffset);
}


///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::write(void const * p, ptrdiff_t bytes)
{
	if (bytes + len > size) {
		resize(bytes + len);
	}
	memcpy(data + len, p, bytes);

	ptrdiff_t res = len;
	len += bytes;

	return res;
}

///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::update(ptrdiff_t loc, void const * p, ptrdiff_t bytes)
{
	if (loc + bytes > size) {
		resize(loc + bytes);
	}
	memcpy(data + loc, p, bytes);

	if (len < loc + bytes) {
		len = loc + bytes;
	}

	return len;
}

///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::extract(ptrdiff_t loc, void * p, ptrdiff_t bytes)
{
	if (loc + bytes > len) return -1;
	memcpy(p, data + loc, bytes);

	return loc + bytes;
}

///////////////////////////
//
//
//
ptrdiff_t
MessageBuffer::read(void * p, ptrdiff_t bytes)
{
	if (offset + bytes > len) return -1;
	memcpy(p, data + offset, bytes);

	offset += bytes;
	return offset;
}

ptrdiff_t MessageBuffer::WriteString( const char *pString )
{
	return write( pString, V_strlen( pString ) + 1 );
}

ptrdiff_t MessageBuffer::ReadString( char *pOut, ptrdiff_t bufferLength )
{
	ptrdiff_t nChars = 0;
	while ( 1 )
	{
		char ch;
		if ( read( &ch, sizeof( ch ) ) == -1 )
		{
			pOut[0] = 0;
			return -1;
		}
		
		if ( ch == 0 || nChars >= (bufferLength-1) )
			break;
			
		pOut[nChars] = ch;
		++nChars;
	}
	pOut[nChars] = 0;
	return nChars + 1;
}


///////////////////////////
//
//
//
void
MessageBuffer::clear()
{
	memset(data, 0, size);
	offset = 0;
	len = 0;
}

///////////////////////////
//
//
//
void
MessageBuffer::clear(ptrdiff_t minsize)
{
	if (minsize > size) {
		resize(minsize);
    }
	memset(data, 0, size);
	offset = 0;
	len = 0;
}

///////////////////////////
//
//
//
void
MessageBuffer::reset(ptrdiff_t minsize)
{
	if (minsize > size) {
		resize(minsize);
    }
	len = 0;
	offset = 0;
}

///////////////////////////
//
//
//
void
MessageBuffer::resize(ptrdiff_t minsize)
{
	if (minsize < size) return;

	if (size * 2 > minsize) minsize = size * 2;

	char * odata = data;
	data = (char *) malloc(minsize);
	memcpy(data, odata, len);
	size = minsize;
	free(odata);
}


///////////////////////////
//
//
void
MessageBuffer::print(FILE * ofile, ptrdiff_t num) const
{
	fprintf(ofile, "Len: %zd Offset: %zd Size: %zd\n", len, offset, size);
    if (num > size) num = size;
	for (ptrdiff_t i=0; i<num; ++i) {
		fprintf(ofile, "%02x ", (unsigned char) data[i]);
	}
	fprintf(ofile, "\n");
}
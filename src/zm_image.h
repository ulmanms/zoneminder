//
// ZoneMinder Image Class Interface, $Date$, $Revision$
// Copyright (C) 2001-2008 Philip Coombes
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 

#ifndef ZM_IMAGE_H
#define ZM_IMAGE_H

#include "zm.h"
extern "C"
{
#include "zm_jpeg.h"
}
#include "zm_rgb.h"
#include "zm_coord.h"
#include "zm_box.h"
#include "zm_poly.h"
#include "zm_mem_utils.h"
#include "zm_utils.h"

#include <errno.h>

#if HAVE_ZLIB_H
#include <zlib.h>
#endif // HAVE_ZLIB_H

#define ZM_BUFTYPE_DONTFREE 0
#define ZM_BUFTYPE_MALLOC 1 
#define ZM_BUFTYPE_NEW 2
#define ZM_BUFTYPE_AVMALLOC 3
#define ZM_BUFTYPE_ZM 4

//
// This is image class, and represents a frame captured from a 
// camera in raw form.
//
class Image
{
protected:

	struct Edge
	{
		int min_y;
		int max_y;
		double min_x;
		double _1_m;

		static int CompareYX( const void *p1, const void *p2 )
		{
			const Edge *e1 = (const Edge *)p1, *e2 = (const Edge *)p2;
			if ( e1->min_y == e2->min_y )
				return( int(e1->min_x - e2->min_x) );
			else
				return( int(e1->min_y - e2->min_y) );
		}
		static int CompareX( const void *p1, const void *p2 )
		{
			const Edge *e1 = (const Edge *)p1, *e2 = (const Edge *)p2;
			return( int(e1->min_x - e2->min_x) );
		}
	};
	
	inline void DumpBuffer() {
		if (buffer && buffertype != ZM_BUFTYPE_DONTFREE) {
			if(buffertype == ZM_BUFTYPE_ZM)
				zm_freealigned(buffer);
			else if(buffertype == ZM_BUFTYPE_MALLOC)
				free(buffer);
			else if(buffertype == ZM_BUFTYPE_NEW)
				delete buffer;
			/*else if(buffertype == ZM_BUFTYPE_AVMALLOC)
				av_free(buffer);
			*/
		}
		buffer = NULL;
		allocation = 0;
	}
	
	inline void AllocBuffer(size_t p_bufsize) {
		if(buffer)
			DumpBuffer();
		
		buffer = (uint8_t*)zm_mallocaligned(16,p_bufsize);
		if(buffer == NULL)
			Panic("Memory allocation failed: %s",strerror(errno));
		
		buffertype = ZM_BUFTYPE_ZM;
		allocation = p_bufsize;
	}

public:
	enum { CHAR_HEIGHT=11, CHAR_WIDTH=6 };
	enum { LINE_HEIGHT=CHAR_HEIGHT+0 };

protected:
	static bool initialised;
	static unsigned char *abs_table;
	static unsigned char *y_r_table;
	static unsigned char *y_g_table;
	static unsigned char *y_b_table;
	static jpeg_compress_struct *jpg_ccinfo[100];
	static jpeg_decompress_struct *jpg_dcinfo;
	static struct zm_error_mgr jpg_err;

protected:
	int width;
	int height;
	int pixels;
	int colours;
	int size;
	int subpixelorder;
	unsigned long allocation;
	uint8_t *buffer;
	int buffertype; /* 0=not ours, no need to call free(), 1=malloc() buffer, 2=new buffer */
	int holdbuffer; /* Hold the buffer instead of replacing it with new one */
	char text[1024];

protected:
	static void Initialise();

public:
	Image();
	Image( const char *filename );
	Image( int p_width, int p_height, int p_colours, int p_subpixelorder, uint8_t *p_buffer=0);
	Image( const Image &p_image );
	~Image();

	inline int Width() const { return( width ); }
	inline int Height() const { return( height ); }
	inline int Pixels() const { return( pixels ); }
	inline int Colours() const { return( colours ); }
	inline int SubpixelOrder() const { return( subpixelorder ); }
	inline int Size() const { return( size ); }
	
	/* Internal buffer should not be modified from functions outside of this class */
	inline const uint8_t* Buffer() const { return( buffer ); }
	inline const uint8_t* Buffer( unsigned int x, unsigned int y= 0 ) const { return( &buffer[colours*((y*width)+x)] ); }
	/* Request writeable buffer */
	uint8_t* WriteBuffer(const int p_width, const int p_height, const int p_colours, const int p_subpixelorder);
	
	inline int IsBufferHeld() const {
		return holdbuffer;
	}
	
	inline void HoldBuffer(int tohold) {
		holdbuffer = tohold;
	}
	
	inline void Empty() {
	if(!holdbuffer)
		DumpBuffer();
	
	width = height = colours = size = pixels = subpixelorder = 0;
	}
	
	void Assign( int p_width, int p_height, int p_colours, int p_subpixelorder, const uint8_t* new_buffer, const size_t buffer_size);
	void Assign( const Image &image );
	void AssignDirect( const int p_width, const int p_height, const int p_colours, const int p_subpixelorder, uint8_t *new_buffer, const size_t buffer_size, const int p_buffertype);

	inline void CopyBuffer( const Image &image )
	{
		if ( image.size != size )
        {
            Panic( "Attempt to copy different size image buffers, expected %d, got %d", size, image.size );
        }
		sse2_aligned_memcpy( buffer, image.buffer, size );
	}
	inline Image &operator=( const unsigned char *new_buffer )
	{
		sse2_aligned_memcpy( buffer, new_buffer, size );
		return( *this );
	}

	bool ReadRaw( const char *filename );
	bool WriteRaw( const char *filename ) const;

	bool ReadJpeg( const char *filename, int p_colours, int p_subpixelorder);
	bool WriteJpeg( const char *filename, int quality_override=0 ) const;
	bool DecodeJpeg( const JOCTET *inbuffer, int inbuffer_size, int p_colours, int p_subpixelorder);
	bool EncodeJpeg( JOCTET *outbuffer, int *outbuffer_size, int quality_override=0 ) const;

#if HAVE_ZLIB_H
	bool Unzip( const Bytef *inbuffer, unsigned long inbuffer_size );
	bool Zip( Bytef *outbuffer, unsigned long *outbuffer_size, int compression_level=Z_BEST_SPEED ) const;
#endif // HAVE_ZLIB_H

	bool Crop( int lo_x, int lo_y, int hi_x, int hi_y );
	bool Crop( const Box &limits );

	void Overlay( const Image &image );
	void Overlay( const Image &image, int x, int y );
	void Blend( const Image &image, int transparency=10 ) const;
	static Image *Merge( int n_images, Image *images[] );
	static Image *Merge( int n_images, Image *images[], double weight );
	static Image *Highlight( int n_images, Image *images[], const Rgb threshold=RGB_BLACK, const Rgb ref_colour=RGB_RED );
	//Image *Delta( const Image &image ) const;
	void Delta( const Image &image, Image* targetimage) const;

	const Coord centreCoord( const char *text ) const;
	void Annotate( const char *p_text, const Coord &coord,  const Rgb fg_colour=RGB_WHITE, const Rgb bg_colour=RGB_BLACK );
	Image *HighlightEdges( Rgb colour, const Box *limits=0 );
	//Image *HighlightEdges( Rgb colour, const Polygon &polygon );
	void Timestamp( const char *label, const time_t when, const Coord &coord );
	void Colourise(const int p_reqcolours, const int p_reqsubpixelorder);
	void DeColourise();

	void Clear() { memset( buffer, 0, size ); }
	void Fill( Rgb colour, const Box *limits=0 );
	void Fill( Rgb colour, int density, const Box *limits=0 );
	void Outline( Rgb colour, const Polygon &polygon );
	void Fill( Rgb colour, const Polygon &polygon );
	void Fill( Rgb colour, int density, const Polygon &polygon );

	void Rotate( int angle );
	void Flip( bool leftright );
	void Scale( unsigned int factor );
};

#endif // ZM_IMAGE_H


typedef void (*blend_fptr_t)(uint8_t*, uint8_t*, uint8_t*, unsigned long, double);
typedef void (*delta_fptr_t)(uint8_t*, uint8_t*, uint8_t*, unsigned long);

/* Blend functions */
void sse2_fastblend(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count, double blendpercent);
void std_fastblend(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count, double blendpercent);
void std_blend(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count, double blendpercent);

/* Delta functions */
void std_delta8_gray8(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_rgb(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_bgr(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_rgba(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_bgra(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_argb(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void std_delta8_abgr(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void sse2_delta8_gray8(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void ssse3_delta8_rgba(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void ssse3_delta8_bgra(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void ssse3_delta8_argb(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
void ssse3_delta8_abgr(uint8_t* col1, uint8_t* col2, uint8_t* result, unsigned long count);
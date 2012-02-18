#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <SDL/SDL.h>

#include "utils.h"
#include "image.h"
#include "qti.h"
#include "qtc.h"
#include "qtv.h"
#include "ppm.h"

/*******************************************************************************
* This is a simple qtv player using SDL.                                       *
*******************************************************************************/

int main( int argc, char *argv[] )
{
	struct image image, refimage;
	struct qti compimage;
	struct qtv video;

	SDL_Surface *screen;
	SDL_Event event;

	int opt, analyze, transform, colordiff, qtw;
	int done, keyframe, playing, step, printfps;
	long int delay, start;
	double fps;
	char *infile;

	analyze = 0;
	transform = 1;
	colordiff = 1;
	printfps = 0;
	qtw = 0;
	infile = NULL;

	while( ( opt = getopt( argc, argv, "wi:" ) ) != -1 )
	{
		switch( opt )
		{
			case 'w':
				qtw = 1;
			break;

			case 'i':
				infile = strdup( optarg );
			break;

			default:
			case '?':
				fputs( "main: Can not parse command line: unknown option\n", stderr );
				return 1;
			break;
		}
	}

	done = 0;
	playing = 1;
	step = 0;
	fps = 0.0;

	if( ! qtv_read_header( &video, qtw, infile ) )
		return 0;

	fprintf( stderr, "Video: %ix%i, %iFPS\n", video.width, video.height, video.framerate );

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
	{
		fputs( "main: SDL_Init: Can not initialize SDL\n", stderr );
		return 1;
	}

	screen = SDL_SetVideoMode( video.width, video.height, 32, SDL_SWSURFACE );

	if( ! screen )
	{
		fputs( "main: SDL_SetVideoMode: Can not set video mode\n", stderr );
		SDL_Quit();
		return 1;
	}

	SDL_WM_SetCaption( "QTV Play", "QTV Play" );

	image_create( &refimage, video.width, video.height );

	do
	{
		start = get_time();

		if( playing || step )
		{
			if( ! qtv_read_frame( &video, &compimage, &keyframe ) )
				return 0;

			if( !analyze )
			{
				if( keyframe )
				{
					if( ! qtc_decompress( &compimage, NULL, &image, 1, compimage.colordiff >= 2 ) )
						return 0;
				}
				else
				{
					if( ! qtc_decompress( &compimage, &refimage, &image, 1, compimage.colordiff >= 2 ) )
						return 0;
				}

				image_copy( &image, &refimage );

				if( transform )
				{
					if( compimage.transform == 1 )
						image_transform_fast_rev( &image );
					else if( compimage.transform == 2 )
						image_transform_rev( &image );
				}

				if( colordiff )
				{
					if( compimage.colordiff >= 1 )
						image_color_diff_rev( &image );
				}
			}
			else
			{
				if( ! qtc_decompress_ccode( &compimage, &image, !keyframe, 1, compimage.colordiff >= 2, analyze-1 ) )
					return 0;	
			}

			memcpy( screen->pixels, image.pixels, video.width*video.height*4 );

			SDL_Flip( screen );

			image_free( &image );
			qti_free( &compimage );

			step = 0;
		}

		if( ! qtv_can_read_frame( &video ) )
			done = 1;


		while( SDL_PollEvent( &event ) )
		{
			switch( event.type )
			{
				case SDL_QUIT:
					done = 1;
				break;

				case SDL_KEYDOWN:
					switch( event.key.keysym.sym )
					{
						case 'q':
						case SDLK_ESCAPE:
							fputs( "Quit\n", stderr );
							done = 1;
						break;

						case 'f':
							printfps = !printfps;
						break;

						case 'a':
							if( analyze == 0 )
							{
								fputs( "Analyze ON (Luma)\n", stderr );
								analyze = 1;
							}
							else if( analyze == 1 )
							{
								fputs( "Analyze ON (Chroma)\n", stderr );
								analyze = 2;
							}
							else
							{
								fputs( "Analyze OFF\n", stderr );
								analyze = 0;
							}
						break;

						case 't':
							if( transform )
							{
								fputs( "Transform OFF\n", stderr );
								transform = 0;
							}
							else
							{
								fputs( "Transform ON\n", stderr );
								transform = 1;
							}
						break;

						case 'y':
							if( colordiff )
							{
								fputs( "Fakeyuv OFF\n", stderr );
								colordiff = 0;
							}
							else
							{
								fputs( "Fakeyuv ON\n", stderr );
								colordiff = 1;
							}
						break;

						case SDLK_SPACE:
							if( playing )
							{
								fputs( "PAUSE\n", stderr );
								playing = 0;
							}
							else
							{
								fputs( "PLAYING\n", stderr );
								playing = 1;
							}
						break;

						case '.':
							fputs( "Step\n", stderr );
							playing = 0;
							step = 1;
						break;

						case SDLK_LEFT:
							if( video.has_index )
							{
								qtv_seek( &video, video.framenum - 10*video.framerate );
								fprintf( stderr, "Seek to: %i \n", video.framenum );
							}
							else
							{
								fputs( "Cannot seek without index\n", stderr );
							}
						break;

						case SDLK_RIGHT:
							if( video.has_index )
							{
								qtv_seek( &video, video.framenum + 10*video.framerate );
								fprintf( stderr, "Seek to: %i \n", video.framenum );
							}
							else
							{
								fputs( "Cannot seek without index\n", stderr );
							}
						break;

						case SDLK_DOWN:
							if( video.has_index )
							{
								qtv_seek( &video, video.framenum - 60*video.framerate );
								fprintf( stderr, "Seek to: %i \n", video.framenum );
							}
							else
							{
								fputs( "Cannot seek without index\n", stderr );
							}
						break;

						case SDLK_UP:
							if( video.has_index )
							{
								qtv_seek( &video, video.framenum + 60*video.framerate );
								fprintf( stderr, "Seek to: %i \n", video.framenum );
							}
							else
							{
								fputs( "Cannot seek without index\n", stderr );
							}
						break;

						default:
						break;
					}
				break;

				default:
				break;
			}
		}

		delay = (1000000l/(long int)video.framerate)-(get_time()-start);

		if( delay > 0 )
			usleep( delay );

		fps = fps*0.75 + 0.25*(1000000.0/(get_time()-start));
		if( printfps )
			fprintf( stderr, "FPS: %.2f\n", fps );
	}
	while( ! done );

	image_free( &refimage );
	qtv_free( &video );

	free( infile );

	SDL_Quit();

	return 0;
}


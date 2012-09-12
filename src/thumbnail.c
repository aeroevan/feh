/* thumbnail.c

Copyright (C) 1999-2003 Tom Gilbert.
Copyright (C) 2010-2011 Daniel Friesel.
Copyright (C) 2012      Christopher Hrabak  bhs_hash() stuff


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "feh.h"
#include "winwidget.h"
#include "options.h"
#include "thumbnail.h"
#include "md5.h"
#include "feh_png.h"
#include "index.h"
#include "signals.h"
#include "feh_ll.h"


static LLMD *thumb_md;          /* replacement for *thumbnails = NULL; */

static thumbmode_data td;

/* TODO Break this up a bit ;) */
/* TODO s/bit/lot */
void init_thumbnail_mode(void)
{
	/* moved to thumbnail_data:
	   Imlib_Image im_main;
	   Imlib_Image bg_im = NULL;
	   Imlib_Font fn = NULL;
	   Imlib_Font title_fn = NULL;

	   int w = 800, h = 600;
	   int bg_w = 0, bg_h = 0;

	   int text_area_w, text_area_h;
	   int max_column_w = 0;
	 */

	Imlib_Load_Error err;
	Imlib_Image im_temp;
	int ww = 0, hh = 0, www, hhh, xxx, yyy;
	int orig_w, orig_h;
	int x = 0, y = 0;
	winwidget winwid = NULL;
	Imlib_Image im_thumb = NULL;
	unsigned char trans_bg = 0;
	int title_area_h = 0;
	int tw = 0, th = 0;
	int fw, fh;
	int thumbnailcount = 0;
	feh_file *file = NULL;
	feh_node *l, *last = NULL;
	int lineno;
	int index_image_width, index_image_height;
	char *s;
	unsigned int thumb_counter = 0;

  static char *start, *end;        /* substr markers in alp[] */
  static char last1;               /* save the final char */
  static ld *alp;                  /* alp stands for Array(of)LinePointers */
  int i = 0;                       /* index to walk thru alp[] array */


  /* set up the rn (root_node) for the thumbnails */
  thumb_md = init_LLMD();          /* replaces the old thumbnails */

	/* initialize thumbnail mode data */
	td.im_main = NULL;
	td.im_bg = NULL;
	td.font_main = NULL;
	td.font_title = NULL;

	td.w = 640;
	td.h = 480;
	td.bg_w = 0;
	td.bg_h = 0;
	td.thumb_tot_h = 0;
	td.text_area_w = 0;
	td.text_area_h = 0;

	td.vertical = 0;
	td.max_column_w = 0;

	mode = "thumbnail";

	if (opt.font)
		td.font_main = gib_imlib_load_font(opt.font);

	if (!td.font_main)
		td.font_main = gib_imlib_load_font(DEFAULT_FONT);

	if (opt.title_font) {
		int fh, fw;

		td.font_title = gib_imlib_load_font(opt.title_font);
		if (!td.font_title)
			td.font_title = gib_imlib_load_font(DEFAULT_FONT_TITLE);

		gib_imlib_get_text_size(td.font_title, "W", NULL, &fw, &fh,
				IMLIB_TEXT_TO_RIGHT);
		title_area_h = fh + 4;
	} else
		td.font_title = imlib_load_font(DEFAULT_FONT_TITLE);

	if ((!td.font_main) || (!td.font_title))
		eprintf("Error loading fonts");

	/* Work out how tall the font is */
	gib_imlib_get_text_size(td.font_main, "W", NULL, &tw, &th,
			IMLIB_TEXT_TO_RIGHT);
	get_index_string_dim(NULL, td.font_main, &fw, &fh);
	td.text_area_h = fh + 5;

	/* This includes the text area for index data */
	td.thumb_tot_h = opt.thumb_h + td.text_area_h;

	/* Use bg image dimensions for default size */
	if (opt.bg && opt.bg_file) {
		if (!strcmp(opt.bg_file, "trans"))
			trans_bg = 1;
		else {

			D(("Time to apply a background to blend onto\n"));
			if (feh_load_image_char(&td.im_bg, opt.bg_file) != 0) {
				td.bg_w = gib_imlib_image_get_width(td.im_bg);
				td.bg_h = gib_imlib_image_get_height(td.im_bg);
			}
		}
	}

	/* figure out geometry for the main window and entries */
	feh_thumbnail_calculate_geometry();

	index_image_width = td.w;
	index_image_height = td.h + title_area_h;
	td.im_main = imlib_create_image(index_image_width, index_image_height);
	gib_imlib_image_set_has_alpha(td.im_main, 1);

	if (!td.im_main)
		eprintf("Imlib error creating index image, are you low on RAM?");

	if (td.im_bg)
		gib_imlib_blend_image_onto_image(td.im_main, td.im_bg,
						 gib_imlib_image_has_alpha
						 (td.im_bg), 0, 0, td.bg_w, td.bg_h, 0, 0,
						 td.w, td.h, 1, 0, 0);
	else if (trans_bg) {
		gib_imlib_image_fill_rectangle(td.im_main, 0, 0, td.w,
				td.h + title_area_h, 0, 0, 0, 0);
		gib_imlib_image_set_has_alpha(td.im_main, 1);
	} else {
		/* Colour the background */
		gib_imlib_image_fill_rectangle(td.im_main, 0, 0, td.w,
				td.h + title_area_h, 0, 0, 0, 255);
	}

	/* Create title now */

	if (!opt.title)
		s = estrdup(PACKAGE " [thumbnail mode]");
	else
		s = estrdup(feh_printf(opt.title, NULL, NULL));

	if (opt.display) {
		winwid = winwidget_create_from_image(td.im_main, s, WIN_TYPE_THUMBNAIL);
		winwidget_show(winwid);
	}

	/* make sure we have an ~/.thumbnails/normal directory for storing
	   permanent thumbnails */
	td.cache_thumbnails = opt.cache_thumbnails;

	if (td.cache_thumbnails) {
		if (opt.thumb_w > opt.thumb_h)
			td.cache_dim = opt.thumb_w;
		else
			td.cache_dim = opt.thumb_h;

		if (td.cache_dim > 256) {
			/* No caching as specified by standard. Sort of. */
			td.cache_thumbnails = 0;
		} else if (td.cache_dim > 128) {
			td.cache_dim = 256;
			td.cache_dir = estrdup("large");
		} else {
			td.cache_dim = 128;
			td.cache_dir = estrdup("normal");
		}
		feh_thumbnail_setup_thumbnail_dir();
	}

	for (l = feh_md->rn->next ;  l != feh_md->rn ; l = l->next) {
		file = FEH_FILE(l->data);
		if ( last ) {
      feh_md->cn = last;
			feh_file_remove_from_list( feh_md );
			last = NULL;
		}
		D(("About to load image %s\n", file->filename));
		if (feh_thumbnail_get_thumbnail(&im_temp, file, &orig_w, &orig_h) != 0) {
			if (opt.verbose)
				feh_display_status('.');
			D(("Successfully loaded %s\n", file->filename));
			www = opt.thumb_w;
			hhh = opt.thumb_h;
			ww = gib_imlib_image_get_width(im_temp);
			hh = gib_imlib_image_get_height(im_temp);

			if (!orig_w) {
				orig_w = ww;
				orig_h = hh;
			}

			thumbnailcount++;
			if (gib_imlib_image_has_alpha(im_temp))
				imlib_context_set_blend(1);
			else
				imlib_context_set_blend(0);

			if (opt.aspect) {
				double ratio = 0.0;

				/* Keep the aspect ratio for the thumbnail */
				ratio = ((double) ww / hh) / ((double) www / hhh);

				if (ratio > 1.0)
					hhh = opt.thumb_h / ratio;
				else if (ratio != 1.0)
					www = opt.thumb_w * ratio;
			}

			if ((!opt.stretch) && ((www > ww) || (hhh > hh))) {
				/* Don't make the image larger unless stretch is specified */
				www = ww;
				hhh = hh;
			}

			im_thumb = gib_imlib_create_cropped_scaled_image(im_temp, 0, 0,
					ww, hh, www, hhh, 1);
			gib_imlib_free_image_and_decache(im_temp);

			if (opt.alpha) {
				DATA8 atab[256];

				D(("Applying alpha options\n"));
				gib_imlib_image_set_has_alpha(im_thumb, 1);
				memset(atab, opt.alpha_level, sizeof(atab));
				gib_imlib_apply_color_modifier_to_rectangle
				    (im_thumb, 0, 0, www, hhh, NULL, NULL, NULL, atab);
			}

			td.text_area_w = opt.thumb_w;
			/* Now draw on the info text */
			if (opt.index_info) {
				get_index_string_dim(file, td.font_main, &fw, &fh);
				if (fw > td.text_area_w)
					td.text_area_w = fw;
				if (fh > td.text_area_h) {
					td.text_area_h = fh + 5;
					td.thumb_tot_h = opt.thumb_h + td.text_area_h;
				}
			}
			if (td.text_area_w > opt.thumb_w)
				td.text_area_w += 5;

			if (td.vertical) {
				if (td.text_area_w > td.max_column_w)
					td.max_column_w = td.text_area_w;
				if (y > td.h - td.thumb_tot_h) {
					y = 0;
					x += td.max_column_w;
					td.max_column_w = 0;
				}
				if (x > td.w - td.text_area_w)
					break;
			} else {
				if (x > td.w - td.text_area_w) {
					x = 0;
					y += td.thumb_tot_h;
				}
				if (y > td.h - td.thumb_tot_h)
					break;
			}

			/* center image relative to the text below it (if any) */
			xxx = x + ((td.text_area_w - www) / 2);
			yyy = y;

			if (opt.aspect)
				yyy += (opt.thumb_h - hhh) / 2;

			/* Draw now */
			gib_imlib_blend_image_onto_image(td.im_main,
							 im_thumb,
							 gib_imlib_image_has_alpha
							 (im_thumb), 0, 0,
							 www, hhh, xxx,
							 yyy, www, hhh, 1,
							 gib_imlib_image_has_alpha(im_thumb), 0);

      feh_ll_add_end( thumb_md, feh_thumbnail_new(file, xxx, yyy, www, hhh));

			gib_imlib_free_image_and_decache(im_thumb);

			lineno = 0;
			if (opt.index_info) {
          alp = feh_wrap_string( td.font_main, create_index_string(file),
                                 NULL, opt.thumb_w * 3);

          for (i=1; i<= alp[0].L0.tot_lines ; i++ ) {
              fw = alp[i].L1.wide;
              fh = alp[i].L1.high;
           		start = alp[i].L1.line;
              end   = alp[i].L1.line + alp[i].L1.len;
              /* null term this substring b4 the call ...*/
              last1 = end[0];  end[0]   = '\0';

              feh_imlib_text_draw(td.im_main, td.font_main, &opt.style[ STYLE_WHITE ],
                                  x + ((td.text_area_w - fw) >> 1),
                                  y + opt.thumb_h + (lineno++ * (th + 2)) + 2,
                                  start,	IMLIB_TEXT_TO_RIGHT );
              /* ... then restore that last char afterwards */
              end[0] = last1;
          }
			}

			if (td.vertical)
				y += td.thumb_tot_h;
			else
				x += td.text_area_w;
		} else {
			if (opt.verbose)
				feh_display_status('x');
			last = l;
		}

		if (opt.display) {
			/* thumb_counter is unsigned, so no need to catch overflows */
			if (++thumb_counter == opt.thumb_redraw) {
				winwidget_render_image(winwid, 0, 1);
				thumb_counter = 0;
			}
			if (!feh_main_iteration(0))
				exit(0);
		}
	}

	if (thumb_counter != 0)
		winwidget_render_image(winwid, 0, 1);

	if (opt.verbose)
		putc('\n', stdout);

	if (opt.title_font) {
		int fw, fh, fx, fy;
		char *s;

		s = create_index_title_string(thumbnailcount, td.w, td.h);
		gib_imlib_get_text_size(td.font_title, s, NULL, &fw, &fh,
				IMLIB_TEXT_TO_RIGHT);
		fx = (index_image_width - fw) >> 1;
		fy = index_image_height - fh - 2;
		feh_imlib_text_draw(td.im_main, td.font_title, &opt.style[ STYLE_WHITE ],
                  fx, fy, s, IMLIB_TEXT_TO_RIGHT );

		if (opt.display)
			winwidget_render_image(winwid, 0, 1);
	}

	if (opt.output && opt.output_file) {
		char output_buf[1024];

		if (opt.output_dir)
			snprintf(output_buf, 1024, "%s/%s", opt.output_dir, opt.output_file);
		else
			strncpy(output_buf, opt.output_file, 1024);
		ungib_imlib_save_image_with_error_return(td.im_main, output_buf, &err);
		if (err) {
			feh_imlib_print_load_error(output_buf, td.im_main, err);
		}
		else if (opt.verbose) {
			int tw, th;

			tw = gib_imlib_image_get_width(td.im_main);
			th = gib_imlib_image_get_height(td.im_main);
			fprintf(stdout, PACKAGE " - File saved as %s\n", output_buf);
			fprintf(stdout,
					"    - Image is %dx%d pixels and contains %d thumbnails\n",
					tw, th, thumbnailcount);
		}
	}

	if (!opt.display)
		gib_imlib_free_image_and_decache(td.im_main);

	free(s);
	return;
}

feh_thumbnail *feh_thumbnail_new(feh_file * file, int x, int y, int w, int h)
{
	feh_thumbnail *thumb;

	thumb = (feh_thumbnail *) emalloc(sizeof(feh_thumbnail));
	thumb->x = x;
	thumb->y = y;
	thumb->w = w;
	thumb->h = h;
	thumb->file = file;
	thumb->exists = 1;

	return(thumb);
}

feh_file *feh_thumbnail_get_file_from_coords(int x, int y)
{
  feh_node *l;
	feh_thumbnail *thumb;

	for (l = thumb_md->rn->next ;  l != thumb_md->rn ; l = l->next) {
		thumb = FEH_THUMB(l->data);
		if (XY_IN_RECT(x, y, thumb->x, thumb->y, thumb->w, thumb->h)) {
			if (thumb->exists) {
				return(thumb->file);
			}
		}
	}
	D(("No matching %d %d\n", x, y));
	return(NULL);
}

feh_thumbnail *feh_thumbnail_get_thumbnail_from_coords(int x, int y)
{
  feh_node *l;
	feh_thumbnail *thumb;

	for (l = thumb_md->rn->next ;  l != thumb_md->rn ; l = l->next) {
		thumb = FEH_THUMB(l->data);
		if (XY_IN_RECT(x, y, thumb->x, thumb->y, thumb->w, thumb->h)) {
			if (thumb->exists) {
				return(thumb);
			}
		}
	}
	D(("No matching %d %d\n", x, y));
	return(NULL);
}

feh_thumbnail *feh_thumbnail_get_from_file(feh_file * file)
{
  feh_node *l;
	feh_thumbnail *thumb;

	for (l = thumb_md->rn->next ;  l != thumb_md->rn ; l = l->next) {
		thumb = FEH_THUMB(l->data);
		if (thumb->file == file) {
			if (thumb->exists) {
				return(thumb);
			}
		}
	}
	D(("No match\n"));
	return(NULL);
}

void feh_thumbnail_mark_removed(feh_file * file, int deleted)
{
	feh_thumbnail *thumb;
	winwidget w;

	thumb = feh_thumbnail_get_from_file(file);
	if (thumb) {
		w = winwidget_get_first_window_of_type(WIN_TYPE_THUMBNAIL);
		if (w) {
			int tw, th;
			if (deleted)
				gib_imlib_image_fill_rectangle(w->im, thumb->x, thumb->y,
						thumb->w, thumb->h, 255, 0, 0, 150);
			else
				gib_imlib_image_fill_rectangle(w->im, thumb->x, thumb->y,
						thumb->w, thumb->h, 0, 0, 255, 150);

			gib_imlib_get_text_size(td.font_main, "X", NULL, &tw, &th,
					IMLIB_TEXT_TO_RIGHT);
			feh_imlib_text_draw(w->im, td.font_main, &opt.style[ STYLE_YELLOW ],
					thumb->x + ((thumb->w - tw) / 2),
					thumb->y + ((thumb->h - th) / 2), "X", IMLIB_TEXT_TO_RIGHT );
			winwidget_render_image(w, 0, 1);
		}
		thumb->exists = 0;
	}
	return;
}

void feh_thumbnail_calculate_geometry(void)
{
	if (!opt.limit_w && !opt.limit_h) {
		if (td.im_bg) {
			opt.limit_w = td.bg_w;
			opt.limit_h = td.bg_h;
		} else
			opt.limit_w = 800;
	}

	/* Here we need to whiz through the files, and look at the filenames and
	   info in the selected font, work out how much space we need, and
	   calculate the size of the image we will require */

	if (opt.limit_w) {
		td.w = opt.limit_w;

		index_calculate_height(td.font_main, td.w, &td.h, &td.thumb_tot_h);

		if (opt.limit_h) {
			if (td.h> opt.limit_h)
				weprintf(
					"The image size you specified (%dx%d) is not large\n"
					"enough to hold all %d thumbnails. To fit all\n"
					"the thumnails, either decrease their size, choose a smaller font,\n"
					"or use a larger image (like %dx%d)",
					opt.limit_w, opt.limit_h, FEH_LL_LEN( feh_md ), opt.limit_w, td.h);
			td.h = opt.limit_h;
		}
	} else if (opt.limit_h) {
		td.vertical = 1;
		td.h = opt.limit_h;

		index_calculate_width(td.font_main, &td.w, td.h, &td.thumb_tot_h);
	}
}

int feh_thumbnail_get_thumbnail(Imlib_Image * image, feh_file * file,
	int * orig_w, int * orig_h)
{
	int status = 0;
	char *thumb_file = NULL, *uri = NULL;

	*orig_w = 0;
	*orig_h = 0;

	if (!file || !file->filename)
		return (0);

	if (td.cache_thumbnails) {
		uri = feh_thumbnail_get_name_uri(file->filename);
		thumb_file = feh_thumbnail_get_name(uri);
		status = feh_thumbnail_get_generated(image, file, thumb_file,
			orig_w, orig_h);

		if (!status)
			status = feh_thumbnail_generate(image, file, thumb_file, uri,
				orig_w, orig_h);

		D(("uri is %s, thumb_file is %s\n", uri, thumb_file));
		free(uri);
		free(thumb_file);
	} else
		status = feh_load_image(image, file);

	return status;
}

char *feh_thumbnail_get_name(char *uri)
{
	char *home = NULL, *thumb_file = NULL, *md5_name = NULL;

	/* FIXME: make sure original file isn't under ~/.thumbnails */

	md5_name = feh_thumbnail_get_name_md5(uri);

	home = getenv("HOME");
	if (home) {
		thumb_file = estrjoin("/", home, ".thumbnails", td.cache_dir, md5_name, NULL);
	}

	free(md5_name);

	return thumb_file;
}

char *feh_thumbnail_get_name_uri(char *name)
{
	char *cwd, *uri = NULL;

	/* FIXME: what happens with http, https, and ftp? MTime etc */
	if ((strncmp(name, "http://", 7) != 0) &&
	    (strncmp(name, "https://", 8) != 0) && (strncmp(name, "ftp://", 6) != 0)
	    && (strncmp(name, "file://", 7) != 0)) {

		/* make sure it's an absoulte path */
		/* FIXME: add support for ~, need to investigate if it's expanded
		   somewhere else before adding (unecessary) code */
		if (name[0] != '/') {
			/* work around /some/path/./image.ext */
			if ((strncmp(name, "./", 2)) == 0)
				name += 2;
			cwd = getcwd(NULL, 0);
			uri = estrjoin("/", "file:/", cwd, name, NULL);
			free(cwd);
		} else {
			uri = estrjoin(NULL, "file://", name, NULL);
		}
	} else
		uri = estrdup(name);

	return uri;
}

char *feh_thumbnail_get_name_md5(char *uri)
{
	int i;
	char *pos, *md5_name;
	md5_state_t pms;
	md5_byte_t digest[16];

	/* generate the md5 sum */
	md5_init(&pms);
	md5_append(&pms, (unsigned char *)uri, strlen(uri));
	md5_finish(&pms, digest);

	/* print the md5 as hex to a string */
	md5_name = emalloc(32 + 4 + 1 * sizeof(char));	/* md5 + .png + '\0' */
	for (i = 0, pos = md5_name; i < 16; i++, pos += 2) {
		sprintf(pos, "%02x", digest[i]);
	}
	sprintf(pos, ".png");

	return md5_name;
}

int feh_thumbnail_generate(Imlib_Image * image, feh_file * file,
		char *thumb_file, char *uri, int * orig_w, int * orig_h)
{
	int w, h, thumb_w, thumb_h;
	Imlib_Image im_temp;
	struct stat sb;
	char c_width[8], c_height[8];

	if (feh_load_image(&im_temp, file) != 0) {
		*orig_w = w = gib_imlib_image_get_width(im_temp);
		*orig_h = h = gib_imlib_image_get_height(im_temp);
		thumb_w = td.cache_dim;
		thumb_h = td.cache_dim;

		if ((w > td.cache_dim) || (h > td.cache_dim)) {
			double ratio = (double) w / h;
			if (ratio > 1.0)
				thumb_h = td.cache_dim / ratio;
			else if (ratio != 1.0)
				thumb_w = td.cache_dim * ratio;
		}

		*image = gib_imlib_create_cropped_scaled_image(im_temp, 0, 0, w, h,
				thumb_w, thumb_h, 1);

		if (!stat(file->filename, &sb)) {
			char c_mtime[128];
			sprintf(c_mtime, "%d", (int)sb.st_mtime);
			snprintf(c_width, 8, "%d", w);
			snprintf(c_height, 8, "%d", h);
			feh_png_write_png(*image, thumb_file, "Thumb::URI", uri,
					"Thumb::MTime", c_mtime,
					"Thumb::Image::Width", c_width,
					"Thumb::Image::Height", c_height);
		}

		gib_imlib_free_image_and_decache(im_temp);

		return (1);
	}

	return (0);
}

int feh_thumbnail_get_generated(Imlib_Image * image, feh_file * file,
	char *thumb_file, int * orig_w, int * orig_h)
{
	struct stat sb;
	char *c_mtime=NULL, *c_width=NULL, *c_height=NULL;
	time_t mtime = 0;
  bhs_node **aHash;
  int i=0, bhs_size = 3;             /* change this if you add Thumb::URI */
  char * ts1 = "Thumb::MTime";
  char * ts2 = "Thumb::Image::Width";
  char * ts3 = "Thumb::Image::Height";

	if (!stat(file->filename, &sb)) {

    /* create aHash[], prefilling with just the three keys we care about.
     * When feh_png_read_comments() fills in the ->data part, any attempts
     * to add a new hash key fail cause the hash is only 3 keys deep.
     */
    aHash = bhs_hash_init( bhs_size  );
    i    += bhs_hash_get( aHash , ts1, NULL, ADDIT_YES );
    i    += bhs_hash_get( aHash , ts2, NULL, ADDIT_YES );
    i    += bhs_hash_get( aHash , ts3, NULL, ADDIT_YES );
    if ( i != 6 )
        eprintf( "Failed to store hash keys.  That's Odd! ");

    /* then fill the data that goes with these three keys. */

		if ( feh_png_read_comments(thumb_file, aHash ) == 0 ) {
      if ( bhs_hash_get(aHash, ts1 , NULL , ADDIT_NO ))
          c_mtime  = (char *) aHash[aHash[0]->h0.which]->h1.data;
      if ( bhs_hash_get(aHash, ts2 , NULL , ADDIT_NO ))
          c_width  = (char *) aHash[aHash[0]->h0.which]->h1.data;
      if ( bhs_hash_get(aHash, ts3 , NULL , ADDIT_NO ))
          c_height  = (char *) aHash[aHash[0]->h0.which]->h1.data;

			if (c_mtime != NULL)
				mtime = (time_t) strtol(c_mtime, NULL, 10);
			if (c_width != NULL)
				*orig_w = atoi(c_width);
			if (c_height != NULL)
				*orig_h = atoi(c_height);

      /* free all the data elements */
      for ( i=1; i< bhs_size+1 ; i++ ) {
          if ( aHash[i] )
              free( aHash[i]->h1.data );
      }

      /* FIXME: should we bother about Thumb::URI? */
      if (mtime == sb.st_mtime ) {
        feh_load_image_char(image, thumb_file);
        return (1);
      }
    }
	}

	return (0);
}

void feh_thumbnail_show_fullsize(feh_file *thumbfile)
{
  feh_node *l;
	winwidget thumbwin = NULL;
	char *s;

	if (!opt.thumb_title)
		s = thumbfile->name;
	else
		s = feh_printf(opt.thumb_title, thumbfile, NULL);

	thumbwin = winwidget_get_first_window_of_type(WIN_TYPE_THUMBNAIL_VIEWER);
  l = feh_ll_new();           /* FIXME Is this a memory leak? */
  l->data = thumbfile;
	if (!thumbwin) {
		thumbwin = winwidget_create_from_file( l,s, WIN_TYPE_THUMBNAIL_VIEWER);
		if (thumbwin)
			winwidget_show(thumbwin);
	} else if (FEH_FILE(thumbwin->file->data) != thumbfile) {
		free(thumbwin->file);
		thumbwin->file = l;
		winwidget_rename(thumbwin, s);
		feh_reload_image(thumbwin, 1, 1);
	}
}

void feh_thumbnail_select(winwidget winwid, feh_thumbnail *thumbnail)
{
	Imlib_Image origwin;

	if (thumbnail == td.selected)
		return;

	if (thumbnail) {
		origwin = winwid->im;
		winwid->im = gib_imlib_clone_image(origwin);
		gib_imlib_image_fill_rectangle(winwid->im,
				thumbnail->x, thumbnail->y, thumbnail->w,
				thumbnail->h, 50, 50, 255, 100);
		gib_imlib_image_draw_rectangle(winwid->im,
				thumbnail->x, thumbnail->y, thumbnail->w,
				thumbnail->h, 255, 255, 255, 255);
		gib_imlib_image_draw_rectangle(winwid->im,
				thumbnail->x + 1, thumbnail->y + 1,
				thumbnail->w - 2, thumbnail->h - 2,
				0, 0, 0, 255);
		gib_imlib_image_draw_rectangle(winwid->im,
				thumbnail->x + 2, thumbnail->y + 2,
				thumbnail->w - 4, thumbnail->h - 4,
				255, 255, 255, 255);
		winwidget_render_image(winwid, 0, 0);
		gib_imlib_free_image_and_decache(winwid->im);
		winwid->im = origwin;
	} else
		winwidget_render_image(winwid, 0, 0);

	td.selected = thumbnail;
}

void feh_thumbnail_select_next(winwidget winwid, int jump)
{
	feh_node *l, *tmp;
	int i;

	for (l  = thumb_md->rn->next ;
      (l != thumb_md->rn) && (l->next != thumb_md->rn) ;
       l = l->next) {
		tmp = l;
		for (i = jump; (i > 0) && tmp->next; i--)
			tmp = tmp->next;
		if (tmp->data == td.selected)
			break;
	}

	feh_thumbnail_select(winwid, FEH_THUMB(l->data));
}

void feh_thumbnail_select_prev(winwidget winwid, int jump)
{
	feh_node *l;
	feh_thumbnail *thumb;
	int i;

	for (l = thumb_md->rn->next ;  l != thumb_md->rn ; l = l->next) {
		thumb = FEH_THUMB(l->data);
		if ((thumb == td.selected) && l->next) {
			for (i = jump; (i > 0) && l->next; i--)
				l = l->next;
			feh_thumbnail_select(winwid, FEH_THUMB(l->data));
			return;
		}
	}
	feh_thumbnail_select(winwid, FEH_THUMB(FEH_LL_CUR_DATA(thumb_md)));
}

inline void feh_thumbnail_show_selected()
{
	if (td.selected && td.selected->file)
		feh_thumbnail_show_fullsize(td.selected->file);
}

inline feh_file* feh_thumbnail_get_selected_file()
{
	if (td.selected)
		return td.selected->file;
	return NULL;
}

int feh_thumbnail_setup_thumbnail_dir(void)
{
	int status = 0;
	struct stat sb;
	char *dir, *dir_thumbnails, *home;

	home = getenv("HOME");
	if (home != NULL) {
		dir = estrjoin("/", home, ".thumbnails", td.cache_dir, NULL);

		if (!stat(dir, &sb)) {
			if (S_ISDIR(sb.st_mode))
				status = 1;
			else
				weprintf("%s should be a directory", dir);
		} else {
			dir_thumbnails = estrjoin("/", home, ".thumbnails", NULL);

			if (stat(dir_thumbnails, &sb) != 0) {
				if (mkdir(dir_thumbnails, 0700) == -1)
					weprintf("unable to create directory %s", dir_thumbnails);
			}

			free(dir_thumbnails);

			if (mkdir(dir, 0700) == -1)
				weprintf("unable to create directory %s", dir);
			else
				status = 1;
		}
		free(dir);
	}

	return status;
}

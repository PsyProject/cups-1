/*
 * "$Id: rastertopwg.c 10005 2011-09-20 18:36:23Z mike $"
 *
 *   CUPS raster to PWG raster format filter for CUPS.
 *
 *   Copyright 2011 Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright law.
 *   Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry for filter.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/raster.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * 'main()' - Main entry for filter.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* Raster file */
  cups_raster_t		*inras,		/* Input raster stream */
			*outras;	/* Output raster stream */
  cups_page_header2_t	inheader,	/* Input raster page header */
			outheader;	/* Output raster page header */
  int			y;		/* Current line */
  unsigned char		*line;		/* Line buffer */
  int			page = 0,	/* Current page */
			page_width,	/* Actual page width */
			page_height,	/* Actual page height */
			page_top,	/* Top margin */
			page_bottom,	/* Bottom margin */
			page_left,	/* Left margin */
			linesize,	/* Bytes per line */
			lineoffset;	/* Offset into line */
  unsigned char		white;		/* White pixel */
  ppd_file_t		*ppd;		/* PPD file */
  ppd_attr_t		*back;		/* cupsBackSize attribute */
  _ppd_cache_t		*cache;		/* PPD cache */
  _pwg_size_t		*pwg_size;	/* PWG media size */
  _pwg_media_t		*pwg_media;	/* PWG media name */
  int	 		num_options;	/* Number of options */
  cups_option_t		*options = NULL;/* Options */
  const char		*val;		/* Option value */


  if (argc < 6 || argc > 7)
  {
    puts("Usage: rastertopwg job user title copies options [filename]");
    return (1);
  }
  else if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: Unable to open print file");
      return (1);
    }
  }
  else
    fd = 0;

  inras  = cupsRasterOpen(fd, CUPS_RASTER_READ);
  outras = cupsRasterOpen(1, CUPS_RASTER_WRITE_PWG);

  ppd   = ppdOpenFile(getenv("PPD"));
  back  = ppdFindAttr(ppd, "cupsBackSide", NULL);

  num_options = cupsParseOptions(argv[5], 0, &options);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  cache = ppd ? ppd->cache : NULL;

  while (cupsRasterReadHeader2(inras, &inheader))
  {
   /*
    * Compute the real raster size...
    */

    page ++;

    fprintf(stderr, "PAGE: %d %d\n", page, inheader.NumCopies);

    page_width  = (int)(inheader.cupsPageSize[0] * inheader.HWResolution[0] /
                        72.0);
    page_height = (int)(inheader.cupsPageSize[1] * inheader.HWResolution[1] /
                        72.0);
    page_left   = (int)(inheader.cupsImagingBBox[0] *
                        inheader.HWResolution[0] / 72.0);
    page_bottom = (int)(inheader.cupsImagingBBox[1] *
                        inheader.HWResolution[1] / 72.0);
    page_top    = page_height - page_bottom - inheader.cupsHeight;
    linesize    = (page_width * inheader.cupsBitsPerPixel + 7) / 8;
    lineoffset  = page_left * inheader.cupsBitsPerPixel / 8; /* Round down */

    switch (inheader.cupsColorSpace)
    {
      case CUPS_CSPACE_W :
      case CUPS_CSPACE_RGB :
      case CUPS_CSPACE_SW :
      case CUPS_CSPACE_SRGB :
      case CUPS_CSPACE_ADOBERGB :
          white = 255;
	  break;

      case CUPS_CSPACE_K :
      case CUPS_CSPACE_CMYK :
      case CUPS_CSPACE_DEVICE1 :
      case CUPS_CSPACE_DEVICE2 :
      case CUPS_CSPACE_DEVICE3 :
      case CUPS_CSPACE_DEVICE4 :
      case CUPS_CSPACE_DEVICE5 :
      case CUPS_CSPACE_DEVICE6 :
      case CUPS_CSPACE_DEVICE7 :
      case CUPS_CSPACE_DEVICE8 :
      case CUPS_CSPACE_DEVICE9 :
      case CUPS_CSPACE_DEVICEA :
      case CUPS_CSPACE_DEVICEB :
      case CUPS_CSPACE_DEVICEC :
      case CUPS_CSPACE_DEVICED :
      case CUPS_CSPACE_DEVICEE :
      case CUPS_CSPACE_DEVICEF :
          white = 0;
	  break;

      default :
	  _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
	  fprintf(stderr, "DEBUG: Unsupported cupsColorSpace %d on page %d.\n",
	          inheader.cupsColorSpace, page);
	  return (1);
    }

    if (inheader.cupsColorOrder != CUPS_ORDER_CHUNKED)
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
      fprintf(stderr, "DEBUG: Unsupported cupsColorOrder %d on page %d.\n",
              inheader.cupsColorOrder, page);
      return (1);
    }

    if (inheader.cupsBitsPerPixel != 1 &&
        inheader.cupsBitsPerColor != 8 && inheader.cupsBitsPerColor != 16)
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
      fprintf(stderr, "DEBUG: Unsupported cupsBitsPerColor %d on page %d.\n",
              inheader.cupsBitsPerColor, page);
      return (1);
    }

    memcpy(&outheader, &inheader, sizeof(outheader));
    outheader.cupsWidth        = page_width;
    outheader.cupsHeight       = page_height;
    outheader.cupsBytesPerLine = linesize;

    outheader.cupsInteger[14]  = 0;	/* VendorIdentifier */
    outheader.cupsInteger[15]  = 0;	/* VendorLength */

    if ((val = cupsGetOption("print-content-optimize", num_options,
                             options)) != NULL)
    {
      if (!strcmp(val, "automatic"))
        strlcpy(outheader.OutputType, "Automatic",
                sizeof(outheader.OutputType));
      else if (!strcmp(val, "graphics"))
        strlcpy(outheader.OutputType, "Graphics", sizeof(outheader.OutputType));
      else if (!strcmp(val, "photo"))
        strlcpy(outheader.OutputType, "Photo", sizeof(outheader.OutputType));
      else if (!strcmp(val, "text"))
        strlcpy(outheader.OutputType, "Text", sizeof(outheader.OutputType));
      else if (!strcmp(val, "text-and-graphics"))
        strlcpy(outheader.OutputType, "TextAndGraphics",
                sizeof(outheader.OutputType));
      else
      {
        fprintf(stderr, "DEBUG: Unsupported print-content-type \"%s\".\n", val);
        outheader.OutputType[0] = '\0';
      }
    }

    if ((val = cupsGetOption("print-quality", num_options, options)) != NULL)
    {
      int quality = atoi(val);		/* print-quality value */

      if (quality >= IPP_QUALITY_DRAFT && quality <= IPP_QUALITY_HIGH)
	outheader.cupsInteger[8] = quality;
      else
      {
	fprintf(stderr, "DEBUG: Unsupported print-quality %d.\n", quality);
	outheader.cupsInteger[8] = 0;
      }
    }

    if ((val = cupsGetOption("print-rendering-intent", num_options,
                             options)) != NULL)
    {
      if (!strcmp(val, "absolute"))
        strlcpy(outheader.cupsRenderingIntent, "Absolute",
                sizeof(outheader.cupsRenderingIntent));
      else if (!strcmp(val, "automatic"))
        strlcpy(outheader.cupsRenderingIntent, "Automatic",
                sizeof(outheader.cupsRenderingIntent));
      else if (!strcmp(val, "perceptual"))
        strlcpy(outheader.cupsRenderingIntent, "Perceptual",
                sizeof(outheader.cupsRenderingIntent));
      else if (!strcmp(val, "relative"))
        strlcpy(outheader.cupsRenderingIntent, "Relative",
                sizeof(outheader.cupsRenderingIntent));
      else if (!strcmp(val, "relative-bpc"))
        strlcpy(outheader.cupsRenderingIntent, "RelativeBpc",
                sizeof(outheader.cupsRenderingIntent));
      else if (!strcmp(val, "saturation"))
        strlcpy(outheader.cupsRenderingIntent, "Saturation",
                sizeof(outheader.cupsRenderingIntent));
      else
      {
        fprintf(stderr, "DEBUG: Unsupported print-rendering-intent \"%s\".\n",
                val);
        outheader.cupsRenderingIntent[0] = '\0';
      }
    }

    if (inheader.cupsPageSizeName[0] &&
        (pwg_size = _ppdCacheGetSize(cache, inheader.cupsPageSizeName)) != NULL)
    {
      strlcpy(outheader.cupsPageSizeName, pwg_size->map.pwg,
	      sizeof(outheader.cupsPageSizeName));
    }
    else
    {
      pwg_media = _pwgMediaForSize((int)(2540.0 * inheader.cupsPageSize[0] /
                                         72.0),
                                   (int)(2540.0 * inheader.cupsPageSize[1] /
                                         72.0));

      if (pwg_media)
        strlcpy(outheader.cupsPageSizeName, pwg_media->pwg,
                sizeof(outheader.cupsPageSizeName));
      else
      {
        fprintf(stderr, "DEBUG: Unsupported PageSize %.2fx%.2f.\n",
                inheader.cupsPageSize[0], inheader.cupsPageSize[1]);
        outheader.cupsPageSizeName[0] = '\0';
      }
    }

    if (inheader.Duplex && !(page & 1) &&
        back && _cups_strcasecmp(back->value, "Normal"))
    {
      if (_cups_strcasecmp(back->value, "Flipped"))
      {
        if (inheader.Tumble)
        {
	  outheader.cupsInteger[1] = -1;/* CrossFeedTransform */
	  outheader.cupsInteger[2] = 1;	/* FeedTransform */

	  outheader.cupsInteger[3] = page_width - page_left -
	                             inheader.cupsWidth;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_top;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_width - page_left;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_bottom;
      					/* ImageBoxBottom */
        }
        else
        {
	  outheader.cupsInteger[1] = 1;	/* CrossFeedTransform */
	  outheader.cupsInteger[2] = -1;/* FeedTransform */

	  outheader.cupsInteger[3] = page_left;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_bottom;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_left + inheader.cupsWidth;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_top;
      					/* ImageBoxBottom */
        }
      }
      else if (_cups_strcasecmp(back->value, "ManualTumble"))
      {
        if (inheader.Tumble)
        {
	  outheader.cupsInteger[1] = -1;/* CrossFeedTransform */
	  outheader.cupsInteger[2] = -1;/* FeedTransform */

	  outheader.cupsInteger[3] = page_width - page_left -
	                             inheader.cupsWidth;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_bottom;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_width - page_left;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_top;
      					/* ImageBoxBottom */
        }
        else
        {
	  outheader.cupsInteger[1] = 1;	/* CrossFeedTransform */
	  outheader.cupsInteger[2] = 1;	/* FeedTransform */

	  outheader.cupsInteger[3] = page_left;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_top;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_left + inheader.cupsWidth;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_bottom;
      					/* ImageBoxBottom */
        }
      }
      else if (_cups_strcasecmp(back->value, "Rotated"))
      {
        if (inheader.Tumble)
        {
	  outheader.cupsInteger[1] = -1;/* CrossFeedTransform */
	  outheader.cupsInteger[2] = -1;/* FeedTransform */

	  outheader.cupsInteger[3] = page_width - page_left -
	                             inheader.cupsWidth;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_bottom;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_width - page_left;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_top;
      					/* ImageBoxBottom */
        }
        else
        {
	  outheader.cupsInteger[1] = 1;	/* CrossFeedTransform */
	  outheader.cupsInteger[2] = 1;	/* FeedTransform */

	  outheader.cupsInteger[3] = page_left;
					/* ImageBoxLeft */
	  outheader.cupsInteger[4] = page_top;
					/* ImageBoxTop */
	  outheader.cupsInteger[5] = page_left + inheader.cupsWidth;
      					/* ImageBoxRight */
	  outheader.cupsInteger[6] = page_height - page_bottom;
      					/* ImageBoxBottom */
        }
      }
      else
      {
       /*
        * Unsupported value...
        */

        fprintf(stderr, "DEBUG: Unsupported cupsBackSide \"%s\".\n", back->value);

	outheader.cupsInteger[1] = 1;	/* CrossFeedTransform */
	outheader.cupsInteger[2] = 1;	/* FeedTransform */

	outheader.cupsInteger[3] = page_left;
					/* ImageBoxLeft */
	outheader.cupsInteger[4] = page_top;
					/* ImageBoxTop */
	outheader.cupsInteger[5] = page_left + inheader.cupsWidth;
      					/* ImageBoxRight */
	outheader.cupsInteger[6] = page_height - page_bottom;
      					/* ImageBoxBottom */
      }
    }
    else
    {
      outheader.cupsInteger[1] = 1;	/* CrossFeedTransform */
      outheader.cupsInteger[2] = 1;	/* FeedTransform */

      outheader.cupsInteger[3] = page_left;
					/* ImageBoxLeft */
      outheader.cupsInteger[4] = page_top;
					/* ImageBoxTop */
      outheader.cupsInteger[5] = page_left + inheader.cupsWidth;
      					/* ImageBoxRight */
      outheader.cupsInteger[6] = page_height - page_bottom;
      					/* ImageBoxBottom */
    }

    if (!cupsRasterWriteHeader2(outras, &outheader))
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
      fprintf(stderr, "DEBUG: Unable to write header for page %d.\n", page);
      return (1);
    }

   /*
    * Copy raster data...
    */

    line = malloc(linesize);

    memset(line, white, linesize);
    for (y = page_top; y > 0; y --)
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        page_top - y + 1, page);
	return (1);
      }

    for (y = inheader.cupsHeight; y > 0; y --)
    {
      cupsRasterReadPixels(inras, line + lineoffset, inheader.cupsBytesPerLine);
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        inheader.cupsHeight - y + page_top + 1, page);
	return (1);
      }
    }

    memset(line, white, linesize);
    for (y = page_bottom; y > 0; y --)
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        page_bottom - y + page_top + inheader.cupsHeight + 1, page);
	return (1);
      }

    free(line);
  }

  cupsRasterClose(inras);
  if (fd)
    close(fd);

  cupsRasterClose(outras);

  return (0);
}


/*
 * End of "$Id: rastertopwg.c 10005 2011-09-20 18:36:23Z mike $".
 */

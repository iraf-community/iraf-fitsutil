#include <string.h>
#include <stdio.h>
#include "fitsio.h"

int main(int argc, char *argv[])
{
    fitsfile *fptr;   /* FITS file pointer, defined in fitsio.h */
    int status = 0;   /* CFITSIO status value MUST be initialized to zero! */
    int bitpix, naxis, ii, anynul;
    long naxes[2] = {1,1}, fpixel[2] = {1,1};
    double *pixels;
    char format[20], hdformat[20];

    if (argc != 2) {
      printf("Usage:  imlist filename[ext][section filter] \n");
      printf("\n");
      printf("List the the pixel values in a FITS image \n");
      printf("\n");
      printf("Example: \n");
      printf("  imlist image.fits                    - list the whole image\n");
      printf("  imlist image.fits[100:110,400:410]   - list a section\n");
      printf("  imlist table.fits[2][bin (x,y) = 32] - list the pixels in\n");
      printf("         an image constructed from a 2D histogram of X and Y\n");
      printf("         columns in a table with a binning factor = 32\n");
      return(0);
    }

    if (!fits_open_file(&fptr, argv[1], READONLY, &status))
    {
        if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status) )
        {
          if (naxis > 2 || naxis == 0)
             printf("Error: only 1D or 2D images are supported\n");
          else
          {
            /* get memory for 1 row */
            pixels = (double *) malloc(naxes[0] * sizeof(double));

            if (pixels == NULL) {
                printf("Memory allocation error\n");
                return(1);
            }

            if (bitpix > 0) {  /* set the default output format string */
               strcpy(hdformat, " %7d");
               strcpy(format,   " %7.0f");
            } else {
               strcpy(hdformat, " %15d");
               strcpy(format,   " %15.5f");
            }

            printf("\n      ");          /* print column header */
            for (ii = 1; ii <= naxes[0]; ii++)
               printf(hdformat, ii);
            printf("\n");                /* terminate header line */

            /* loop over all the rows in the image, top to bottom */
            for (fpixel[1] = naxes[1]; fpixel[1] >= 1; fpixel[1]--)
            {
               if (fits_read_pix(fptr, TDOUBLE, fpixel, naxes[0], NULL,
                    pixels, NULL, &status) )  /* read row of pixels */
                  break;  /* jump out of loop on error */

               printf(" %4d ",fpixel[1]);  /* print row number */
               for (ii = 0; ii < naxes[0]; ii++)
                  printf(format, pixels[ii]);   /* print each value  */
               printf("\n");                    /* terminate line */
            }
            free(pixels);
          }
        }
        fits_close_file(fptr, &status);
    } 

    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}

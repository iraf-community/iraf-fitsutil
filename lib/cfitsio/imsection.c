#include <string.h>
#include <stdio.h>
#include "fitsio.h"

int main(int argc, char *argv[])
{
    fitsfile *infptr, *outfptr; 
    int status = 0;
    char infile[180], extspec[30], section[100];

    /* Parse the input file string to get the file name, extension, and
       section substrings.   */

    fits_parse_input_url(argv[1], NULL, infile, NULL, extspec, section,
       NULL, NULL, &status);
       
    /* if no section is specified, take whole image */
    if (strlen(section) == 0) strcpy(section, "*,*");

    if (strlen(extspec)) {  /* append the HDU name/number to file name */
        strcat(infile, "[");
        strcat(infile, extspec);
        strcat(infile, "]");
    }
    
    /* open the input file */
    fits_open_file(&infptr, infile, READONLY, &status);

    /* create the output file */
    /* Note: if the output file name is "stream://", then */
    /* the file will be written to the stdout stream */
    fits_create_file(&outfptr, argv[2], &status);

    /* copy the image section from input to output file */
    fits_copy_image_section(infptr, outfptr, section, &status);

    /* close both files */
    fits_close_file(outfptr,  &status);
    fits_close_file(infptr, &status);

    /* check for errors */
    if (status)
       fits_report_error(stderr, status);

    return(status);
}

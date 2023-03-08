include <error.h>
include <fset.h>

define SZ_CARD	80


# FHEADER -- Procedure to catalog a fits file from disk. The user can select
# one line output or the entire header. The user can select  the fields to print
# in the one line output through a an ascii file with format information.

procedure t_fxheader()

char	infile[SZ_FNAME]	# fits file
char	in_fname[SZ_FNAME]	# input file name
char	format_file[SZ_FNAME]	# input file name with format information
				# for one line output per fits file.
pointer	list
int	lenlist
int	file_number, stat
char    ksection[SZ_CARD], dnap[SZ_CARD]

bool	clgetb(), count_lines
int	btoi(), imtgetim(), group
int	imtlen(), itmp
pointer	imtopen()
int	short_header, long_header, cat_print_header()

begin
	# Set up the standard output to flush on a newline
	call fseti (STDOUT, F_FLUSHNL, YES)

	# Get RFITS parameters.
	call clgstr ("fits_file", infile, SZ_FNAME)
	long_header = btoi (clgetb ("long_header"))
	count_lines = clgetb("count_lines")
	
	short_header = YES
	if (long_header == YES)
	    short_header = NO

	list = imtopen (infile)
	lenlist = imtlen (list)


        # Get format file name if short_header is selected
        if (short_header == YES) {
            call clgstr ("format_file", format_file, SZ_FNAME)
	    if (format_file[1] == EOS)
	        call strcpy ("fitsutil$format.mip", format_file, SZ_FNAME)
            call dfread_formats (format_file)
	    # Print the keywords in format_file as one line title.
            call print_titles
        }


	# See if extension number has been specified.
	file_number = 0
	while (imtgetim (list, in_fname, SZ_FNAME) != EOF) {
            call imparse (in_fname, infile, SZ_FNAME, ksection, SZ_CARD,
		dnap, SZ_CARD, group, itmp) 
	    if (group != -1) {
	           call sprintf(in_fname, SZ_FNAME, "%s[%d]")
		       call pargstr(infile)
		       call pargi(group)
	           call strcpy(in_fname, infile, SZ_FNAME)
	    }
	    if (stat == -3) call printf("\n")   # NL if mef
	    iferr (stat = cat_print_header (infile, file_number, 
		  count_lines, short_header, ksection) )
		  call erract(EA_WARN)
	#	  break
	    file_number = file_number + 1
	}
	if (list != NULL)
	    call imtclose (list) 
	
end

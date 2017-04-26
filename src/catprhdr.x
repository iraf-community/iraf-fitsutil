include <error.h>
include <mef.h>
include "dfits.h"

# CAT_PRINT_HEADER -- Procedure to read FITS tape or disk file
#		      and print part of its content.

int procedure cat_print_header (fitsfile, number, count_lines,
    short_header, ksection)

char	fitsfile[SZ_FNAME]  # Fits file name
int	number		    # input file number
bool    count_lines	    # I Do we want line number on long output?
int	short_header	    # I YES,NO
char	ksection[ARB]

pointer	mef, mef_open()
int	enumber, ecount, extver, stat
int	mef_rdhdr_gn(), mef_rdhdr_exnv()
char	extname[LEN_CARD]

errchk	mef_open, mef_rdhdr_gn, mef_rdhdr_exnv

begin
	mef = mef_open( fitsfile, READ_ONLY, 0)
	enumber = MEF_ENUMBER(mef)
	
        call mef_ksection (ksection, extname, extver)

	ecount = 0
	if (enumber ==  -1 && ksection[1] == EOS) {
	    # Do all the extensions in the file
	    while (mef_rdhdr_gn(mef, ecount) != EOF) {
	        call cat_print_unit (mef, ecount, short_header, count_lines)
	        ecount = ecount + 1
	    } 

	    call mef_close(mef)
	    if (ecount > 1) 
	        return(-3)
	    else
	        return(EOF)
	} else {       # Do only one extension
	    if (ksection[1] != EOS) {
	        stat = mef_rdhdr_exnv (mef, extname, extver)
	        enumber = MEF_CGROUP(mef) - 1
	    } else
	        stat = mef_rdhdr_gn (mef, enumber)

	    if (stat == EOF) {
	        if (enumber > MEF_CGROUP(mef)) {
	 	    call sprintf(extname, LEN_CARD, 
		      "Extension not found: %s[%d]")
		       call pargstr(MEF_FNAME(mef))
		       call pargi(enumber)
	            call mef_close(mef)
		    call error(13, extname)
	        }
	        if (MEF_CGROUP(mef) < 0) {
	 	    call sprintf(extname, LEN_CARD, 
		      "Extension not found: %s%s")
		       call pargstr(MEF_FNAME(mef))
		       call pargstr(ksection)
	            call mef_close(mef)
		    call error(13, extname)
	        }
	        call mef_close(mef)
	        return (EOF)
	    }
	    call cat_print_unit (mef, enumber, short_header, count_lines)
	}

	call mef_close(mef)
	return (0)
end


# CAT_PRINT_MAIN -  Output to stdout and/or the the log_file one
# line of information per input fits file according to the field
# specifications in the file format_file.
 
procedure cat_print_unit (mef, number, short_header, count_lines)
 
pointer mef			# Mef descriptor
int	number  		# input extension sequence
int     short_header		# YES. Print one line per FITS unit
bool	count_lines		# YES, NO. Print line number of long output

char	str[LEN_CARD]		# card data string
int	nk, i, nch
char	sdim[SZ_KEYWORD]
char    line[SZ_LINE]
 
int	nl, nbc, fd
int	strmatch(), itoc(), strcmp(), stropen(), getline(), mef_gnbc()
include	"dfits.com"
 
begin
     if (short_header != YES) {
	 # Print the entire FITS header.
	 call printf("\nFile: %s[%d] *************************\n")   
	     call pargstr(MEF_FNAME(mef))
	     call pargi(number)
	 fd = stropen (Memc[MEF_HDRP(mef)], ARB, READ_ONLY)
	 if (!count_lines)
	     while(getline(fd, line) != EOF) {
		 call printf("%s")
		    call pargstr(line)
	    }
	 else {
	    nl = 1
	    while(getline(fd, line) != EOF) {
		call printf("%2.2d: %75.75s\n")
		    call pargi(nl)
		    call pargstr(line)
		nl = nl + 1
	    }
	 }
	 call close(fd)
     }else {
	 # Search the keyword in the card table
	 line[1] = EOS
	 do nk = 1, nkeywords {
	     if (strcmp (Memc[key_table[nk]], "EXT#") == 0) {
		 nch= itoc (number, str, LEN_CARD)
	     } else if (strmatch (Memc[key_table[nk]], "EXTNAME") > 0) {
		 if (MEF_EXTNAME(mef) == EOS)
		     call mefgstr (mef, "EXTNAME", MEF_EXTNAME(mef), MEF_SZVALSTR)
		 call strcpy (MEF_EXTNAME(mef), str, LEN_CARD)
	     } else if (strmatch (Memc[key_table[nk]], "DIMENS") > 0) {
		 str[1] = EOS
		 do i = 1, MEF_NDIM(mef) {
		     nch= itoc (MEF_NAXIS(mef,i), sdim, SZ_KEYWORD)
		     call strcat (sdim, str, LEN_CARD)
		     if (i != MEF_NDIM(mef))
			 call strcat ("x", str, LEN_CARD)
		 }
	     } else if (strmatch (Memc[key_table[nk]], "EXTVER") > 0) {
		 if (MEF_EXTVER(mef) == INDEFL)
		     str[1] = EOS
		 else
		     nch= itoc (MEF_EXTVER(mef), str, SZ_KEYWORD)
	     } else if (strmatch (Memc[key_table[nk]], "BITPIX") > 0) {
		 nch= itoc (MEF_BITPIX(mef), str, SZ_KEYWORD)
	     } else if (strmatch (Memc[key_table[nk]], "EXTTYPE") > 0) {
		 if (number > 0) {
		     # Right justify one space
		     str[1] = ' '
		     i = 2
		     call strcpy (MEF_EXTTYPE(mef), str[i], LEN_CARD)
		 } else {
		     i = 1
		     call strcpy (MEF_FNAME(mef), str[i], LEN_CARD)
		}
	     } else if (strcmp (Memc[key_table[nk]], "HOFF") == 0)  {
		 i = (MEF_HOFF(mef)-1)*2
		 nch= itoc (i, str, LEN_CARD)
	     } else if (strcmp (Memc[key_table[nk]], "NBC") == 0)  {
	         nbc = mef_gnbc (mef)
		 nch= itoc (nbc, str, LEN_CARD)
	     } else if (strcmp (Memc[key_table[nk]], "POFF") == 0) {
		 i = (MEF_POFF(mef)-1)*2
		 if (MEF_NDIM(mef) == 0)
		     i = INDEFL
		 nch= itoc (i, str, LEN_CARD)
	     } else  {
		 iferr (call mef_findkw (MEF_HDRP(mef),
		      Memc[key_table[nk]], str))
		     str[1] = EOS
	     } 
	     call print_string (line, str, Memc[fmt_table[nk]], opt_table[nk])
	 }
	 call printf("%80.80s\n")
	     call pargstr(line)
     }
 end


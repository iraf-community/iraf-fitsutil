include <imio.h>
include <imhdr.h>
include <mef.h>
include <error.h>
include <ctype.h>

define MAX_RANGES  100
define SZ_EXTN	     4

         
# FXEXTRACT --  Extract 1 or more extensions into single FITS files. The options
# is to have the output files with or without PHU.

procedure t_fxextract()

char	input_file[SZ_PATHNAME]
char	output_file[SZ_PATHNAME]
char	imtlist[SZ_LINE], lbrk		

char	group_list[SZ_LINE]
int	group_range[2*MAX_RANGES+1]

int	nch, gn, ninfiles, nitems, list, ip, ogrp
int	imtopen(), imtgetim(), imtlen()
int	lget_next_number(), ldecode_ranges(), stridx(), ctoi()
     
pointer mefi, mefo, fxx_open_output(), mef_open()
pointer sp, op
bool    clgetb(), use_extnm
bool    verbose, phu
errchk  fcopy

begin

	call smark (sp)
	call salloc (op, SZ_PATHNAME, TY_CHAR)

	call clgstr ("input", imtlist, SZ_LINE)
	call clgstr ("output", output_file, SZ_LINE)
	use_extnm = clgetb("use_extnm")
	phu  = clgetb("phu")
	verbose = clgetb("verbose")

	list = imtopen (imtlist)
	ninfiles = imtlen (list)
	
	ogrp = -1
	lbrk = '['
	if (ninfiles == 1) {
	    ip = stridx (lbrk, imtlist)
	    if (ip > 0)
		nch = ctoi (imtlist, ip+1, ogrp)
	}
		
	# Ignore group_list parameter if more that one input file.
	# Make sure there is a group number in each input filename.
	if (ninfiles > 1) {
	    ip = 1
	    while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {
	   	ip = stridx (lbrk, input_file[ip])
	   	if (ip  == 0) {
		    call eprintf("Input file '%s' ")
		        call pargstr(input_file)
	            call eprintf(" need to have extension number.\n")
		    call erract (EA_FATAL)
		}
	    }
	    call imtrew (list)
	}

	# Extract individuals extensions from each input file in the list.
	if (ninfiles > 1 || ogrp >= 0) {
	    while (imtgetim (list, input_file, SZ_PATHNAME) != EOF) {
		mefi = mef_open(input_file, READ_ONLY, 0)

	        call strcpy (output_file, Memc[op], SZ_LINE)
	        ogrp = MEF_ENUMBER(mefi)
	        mefo =  fxx_open_output (Memc[op], mefi, use_extnm, ninfiles,
			ogrp)
	 	if (!phu) 
		    MEF_KEEPXT(mefo) = YES
        	# If verbose print the operation.
		if (verbose) {
		    call eprintf ("%s -> %s\n")
		        call pargstr (input_file)
		        call pargstr (Memc[op])
		}
	        if (phu) {	
		   call mef_dummyhdr (MEF_FD(mefo), NULL)
		    MEF_KEEPXT(mefo) = YES
	        }
		call mef_copy_extn (mefi, mefo, MEF_ENUMBER(mefi))
		   
		call mef_close (mefi)
                call mef_close (mefo)
	    }
	    call imtclose (list)

	} else {
	    # Copy selected extensions from one input file to individuals
	    # output files.

            call clgstr ("groups", group_list, SZ_LINE)
	    if (group_list[1] == EOS) {
		call eprintf ("Error: parameter 'group' is empty.\n")
		call erract(EA_FATAL)
	    }

	    # Since ranges handles only positive numbers, see if we need
	    # to look for zero group (PHDU).

            if (ldecode_ranges (group_list,group_range,MAX_RANGES,nitems) ==ERR)
	        call error (0, "Illegal file number list.")

	    list = imtopen (imtlist)
	    nch = imtgetim (list, input_file, SZ_PATHNAME) 
	    mefi = mef_open (input_file, READ_ONLY, 0)

	    call strcpy (output_file, Memc[op], SZ_LINE)

	    gn = -1
	    while (lget_next_number (group_range, gn) != EOF) {
	        mefo =  fxx_open_output (Memc[op], mefi, use_extnm, nitems, 
			gn)
	 	if (!phu) 
		    MEF_KEEPXT(mefo) = YES
	        call mef_copy_extn (mefi, mefo, gn)
	        if (verbose) {
		    call eprintf ("%s[%d] -> %s\n")
		    call pargstr (input_file)
		    call pargi(gn)
		    call pargstr (Memc[op])
	        }
                call mef_close (mefo)

		# Restore output name with no group_number
		call strcpy (output_file, Memc[op], SZ_LINE)
	    }
	    call imtclose (list)
	}

	call sfree(sp)
end


# FXX_OPEN_OUTPUT -- Open new output file and return the mef descriptor.

pointer procedure fxx_open_output (output_file, mefi, use_extnm, ninf, gn)

char	output_file[ARB]	#I output filename
pointer mefi			#I input mef descriptor
bool	use_extnm		#I true if want to use EXTNAME
int	ninf			#I Number of input files
int	gn			#I Group number

pointer mef, sp, root, extn
pointer mef_open()
char	name[SZ_FNAME], dirname[SZ_FNAME]
int	len, lenr, lend, junk, null_root, stat
int	fnroot(), fnextn(), itoc(), fnldir(), mef_rdhdr_gn()
errchk  mef_open

begin
	lend = fnldir (output_file, dirname, SZ_FNAME)
	if (use_extnm) {
	    stat = mef_rdhdr_gn (mefi, gn)
	    call mefgstr (mefi, "EXTNAME", name, LEN_CARD)
	    if (name[1] != EOS) {
		# Get root name only
		len = fnroot (name, name, LEN_CARD)
		if (len > 0)
		    name[len+1] = EOS
		else {
		    # EXTNAME did not contain a root name, choose input
		    # filename as root.

		    len = fnroot (MEF_FNAME(mefi), name, LEN_CARD)
	        }
	    }else {
		# EXTNAME was empty. Use input filename and add the extension
		# number.
		len = fnroot (MEF_FNAME(mefi), name, LEN_CARD)
		len = itoc (MEF_ENUMBER(mefi), name[len+1], LEN_CARD)
	    } 
	    output_file[1] = EOS
	    if (lend > 0)
		call strcpy (dirname, output_file, SZ_FNAME)
	    call strcat (name, output_file, SZ_FNAME)			
	    call strcat (".fits", output_file, LEN_CARD)
	    mef = mef_open (output_file, NEW_FILE, 0)

	} else {
	    call smark (sp)
	    call salloc (root, SZ_FNAME, TY_CHAR)
	    call salloc (extn, SZ_EXTN, TY_CHAR)

	    call strcpy (output_file[lend+1], name, SZ_FNAME)
	    null_root = NO
	    if (name[1] == EOS) {
		call strcpy (MEF_FNAME(mefi), name, SZ_FNAME)
		null_root = YES
	    }
	    lenr = fnroot (name, Memc[root], SZ_FNAME)
	    len = fnextn (name, Memc[extn], SZ_EXTN)
	    if (Memc[extn] == EOS)
		call strcpy ("fits", Memc[extn], SZ_EXTN)
	    # Append group number to root
	    if (gn >= 0 && null_root == YES || ninf > 1)
	        junk = itoc (gn, Memc[root+lenr], SZ_FNAME)
	    call strcpy (dirname, output_file, SZ_FNAME)
	    call strcat (Memc[root], output_file, SZ_FNAME)
	    call strcat (".", output_file, SZ_FNAME)
	    call strcat (Memc[extn], output_file, SZ_FNAME)
	    mef = mef_open (output_file, NEW_FILE, 0)

	    call sfree(sp)
	}

	return (mef)
end

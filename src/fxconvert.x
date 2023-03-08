# Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.

include	<imhdr.h>

# FXCONVERT -- Convert image(s)
#
# The input images are given by an image template list.  The output
# is either a matching list of images or a directory.
# The number of input images may be either one or match the number of output
# images.  Image sections are allowed in the input images and are ignored
# in the output images.  If the input and output image names are the same
# then the copy is performed to a temporary file which then replaces the
# input image.

procedure t_fxconvert()

char	imtlist1[SZ_LINE]			# Input image list
char	imtlist2[SZ_LINE]			# Output image list
bool	verbose					# Print operations?

char	image1[SZ_PATHNAME]			# Input image name
char	image2[SZ_PATHNAME]			# Output image name
char	dirname1[SZ_PATHNAME]			# Directory name
char	dirname2[SZ_PATHNAME]			# Directory name

char	dot
int	list1, list2, root_len

int	imtopen(), imtgetim(), imtlen()
int	fnldir(), isdirectory(), strldx()
bool	clgetb()

begin
	# Get input and output image template lists.

	call clgstr ("input", imtlist1, SZ_LINE)
	call clgstr ("output", imtlist2, SZ_LINE)
	verbose = clgetb ("verbose")

	# Check if the output string is a directory.

	dot ='.'
	if (isdirectory (imtlist2, dirname2, SZ_PATHNAME) > 0) {
	    list1 = imtopen (imtlist1)
	    while (imtgetim (list1, image1, SZ_PATHNAME) != EOF) {

		# Strip the image section first because fnldir recognizes it
		# as part of a directory.  Place the input image name
		# without a directory or image section in string dirname1.

		call get_root (image1, image2, SZ_PATHNAME)
		root_len = fnldir (image2, dirname1, SZ_PATHNAME)
		call strcpy (image2[root_len + 1], dirname1, SZ_PATHNAME)

		call strcpy (dirname2, image2, SZ_PATHNAME)
		call strcat (dirname1, image2, SZ_PATHNAME)
	
		root_len = strldx (dot, image2)
		if (root_len != 0)
		   image2[root_len] = EOS
		call fxg_imcopy (image1, image2, verbose)
	    }
	    call imtclose (list1)

	} else {
	    # Expand the input and output image lists.

	    list1 = imtopen (imtlist1)
	    list2 = imtopen (imtlist2)

	    if (imtlen (list1) != imtlen (list2)) {
	        call imtclose (list1)
	        call imtclose (list2)
	        call error (0, "Number of input and output images not the same")
	    }

	    # Do each set of input/output images.

	    while ((imtgetim (list1, image1, SZ_PATHNAME) != EOF) &&
		(imtgetim (list2, image2, SZ_PATHNAME) != EOF)) {

		call fxg_imcopy (image1, image2, verbose)
	    }

	    call imtclose (list1)
	    call imtclose (list2)
	}
end


# FXG_IMCOPY -- Copy an image.  Use sequential routines to permit copying
# images of any dimension.  Perform pixel i/o in the datatype of the image,
# to avoid unnecessary type conversion.

procedure fxg_imcopy (image1, image2, verbose)

char	image1[ARB]			# Input image
char	image2[ARB]			# Output image
bool	verbose				# Print the operation

int	npix, junk
pointer	buf1, buf2, im1, im2
pointer	sp, imtemp, section
long	v1[IM_MAXDIM], v2[IM_MAXDIM]

int	imgnls(), imgnll(), imgnlr(), imgnld(), imgnlx()
int	impnls(), impnll(), impnlr(), impnld(), impnlx()

pointer	immap()

begin
	call smark (sp)
	call salloc (imtemp, SZ_PATHNAME, TY_CHAR)
	call salloc (section, SZ_FNAME, TY_CHAR)

	# Map the input image.
	im1 = immap (image1, READ_ONLY, 0)

	# If the output has a section appended we are writing to a
	# section of an existing image.  Otherwise get a temporary
	# output image name and map it as a copy of the input image.
	# Copy the input image to the temporary output image and unmap
	# the images.  Release the temporary image name.

	call imgsection (image2, Memc[section], SZ_FNAME)
	if (Memc[section] != EOS) {
	    call strcpy (image2, Memc[imtemp], SZ_PATHNAME)
	    im2 = immap (image2, READ_WRITE, 0)
	} else {
	    call xt_mkimtemp (image1, image2, Memc[imtemp], SZ_PATHNAME)
	    im2 = immap (image2, NEW_COPY, im1)
	}
	# If verbose print the operation.
	if (verbose) {
	    call printf ("%s -> %s\n")
		call pargstr (IM_HDRFILE(im1))
		call pargstr (IM_HDRFILE(im2))
	}

	# Setup start vector for sequential reads and writes.

	call amovkl (long(1), v1, IM_MAXDIM)
	call amovkl (long(1), v2, IM_MAXDIM)

	# Copy the image.

	npix = IM_LEN(im1, 1)
	switch (IM_PIXTYPE(im1)) {
	case TY_SHORT:
	    while (imgnls (im1, buf1, v1) != EOF) {
		junk = impnls (im2, buf2, v2)
		call amovs (Mems[buf1], Mems[buf2], npix)
	    }
	case TY_USHORT, TY_INT, TY_LONG:
	    while (imgnll (im1, buf1, v1) != EOF) {
		junk = impnll (im2, buf2, v2)
		call amovl (Meml[buf1], Meml[buf2], npix)
	    }
	case TY_REAL:
	    while (imgnlr (im1, buf1, v1) != EOF) {
		junk = impnlr (im2, buf2, v2)
		call amovr (Memr[buf1], Memr[buf2], npix)
	    }
	case TY_DOUBLE:
	    while (imgnld (im1, buf1, v1) != EOF) {
		junk = impnld (im2, buf2, v2)
		call amovd (Memd[buf1], Memd[buf2], npix)
	    }
	case TY_COMPLEX:
	    while (imgnlx (im1, buf1, v1) != EOF) {
	        junk = impnlx (im2, buf2, v2)
		call amovx (Memx[buf1], Memx[buf2], npix)
	    }
	default:
	    call error (1, "unknown pixel datatype")
	}

	# Unmap the images.

	call imunmap (im2)
	call imunmap (im1)
	call xt_delimtemp (image2, Memc[imtemp])
	call sfree(sp)
end

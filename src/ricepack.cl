#{ RICEPACK - Wrapper script for the CFITSIO 'fpack' task

procedure ricepack (images)

string	images	    		{prompt="input images"}

bool	keep	= no  		{prompt="keep input images?"}
bool	listonly = no		{prompt="only list file contents?"}
bool	verbose = yes		{prompt="verbose messages?"}

int	quantization = 16	{prompt="q level for floating point scaling"}
#string	flags	= ""		{prompt="explicit CFITSIO flags"}

int	nimages			{prompt="number of images compressed"}

#bool	goahead = no		{prompt="continue?"}
string	*imlist

begin
	string	limages, img, outimg, tmpfile, lflags, date1, date2, junk
	real	tstart, tend, hours, seconds, R
	int	count, filsiz, insize, outsize, len, dsize, benefit
	bool	lgo
	struct	tbuf

	cache sections

	limages = images

	if (listonly) {
	    lflags = "-L "    # will be concatenated, so needs final blank

	    if (verbose) printf ("list contents, no files created or deleted\n")

#	} else if (strlen (flags) > 1 && stridx ('-', flags) > 0) {
#	    lflags = flags // " "
#
#	    if (keep) {
#                printf ("warning: flags may override keep=yes, ")
#                goahead.p_mode="ql"; lgo=goahead; goahead.p_mode="h"
#                if (!lgo) return
#	    }

	} else {
	    lflags = "-q " // quantization // " "
	    if (!keep) lflags += "-D "
	    if (verbose) lflags += "-v "
	}

	tmpfile = mktemp ("tmp$fpack_")
	sections (limages, opt="fullname", > tmpfile)

	if (sections.nimages <= 0) {
	    printf ("no images in input list\n")
	    return
	}

	insize = 0
	imlist = tmpfile
	while (fscan (imlist, img) != EOF) {
	    # preflight check
	    if (! access (img)) {
		printf ("input image %s not found\n", img)
		return
	    }

	    len = strlen (img)
	    if (substr (img, len-2, len) == ".fz") {
		printf ("input image %s is already tile compressed\n", img)
		return
	    }

	    if (substr (img, len-3, len) == ".imh" && !keep) {
		printf ('compressing IRAF ".imh" images requires keep=yes\n')
		return
	    }

	    if (substr (img, len-2, len) == ".gz")
		outimg = substr (img, 1, len-3) // ".fz"
	    else if (substr (img, len-3, len) == ".imh")
		outimg = substr (img, 1, len-4) // "fits.fz"
	    else
		outimg = img // ".fz"

	    if (access (outimg)) {
		printf ("output image %s already exists\n", outimg)
		return
	    }

	    filsiz = 0
	    directory (img, long+) | scan (junk, junk, filsiz)
	    insize += int ((real (filsiz) / 1024.) + 0.5)
	}

	if (verbose) {
	    t_fpack ("-V")

	    time | scan (tbuf); print (tbuf) | scan (junk, tstart, date1)
	    printf ("\n%s\n", tbuf)
	}

	count = 0
	outsize = 0
	imlist = tmpfile
	while (fscan (imlist, img) != EOF) {
	    t_fpack (lflags // osfn(img))

	    len = strlen (img)
	    if (substr (img, len-2, len) == ".gz") {
		outimg = substr (img, 1, len-3) // ".fz"
	    } else if (substr (img, len-3, len) == ".imh") {
		outimg = substr (img, 1, len-4) // "fits.fz"
		if (access (img//".fz"))
		    rename (img//".fz", outimg, field="all")
	    } else
		outimg = img // ".fz"

	    filsiz = 0
	    directory (outimg, long+) | scan (junk, junk, filsiz)
	    outsize += int ((real (filsiz) / 1024.) + 0.5)
	    count += 1
	}

	imlist = ""; delete (tmpfile, ver-, >& "dev$null")

	nimages = count

	if (verbose && count > 0) {
	    time | scan (tbuf); print (tbuf) | scan (junk, tend, date2)
	    printf ("%s\n\n", tbuf)

	    # midnight handling is a kludge, but errors will be few & harmless
	    hours = tend - tstart; if (date2 != date1) hours += 24.0
	    seconds = hours * 3600.0 / count

	    printf ("%d images, %4.2f seconds each, %h elapsed\n\n",
		count, seconds, hours)

	    printf (" input: %11.3f MB\n", real (insize) / 1024.)

	    if (! listonly && insize > 0 && outsize > 0) {
		printf ("output: %11.3f MB\n", real (outsize) / 1024.)

		R = real (insize) / real (outsize)
		dsize = insize - outsize
                benefit = int (100.0 * abs (1.0 - 1/R) + 0.5)

		printf (" saved: %11.3f MB, %d%%\n\n",
		    real (dsize) / 1024., benefit)

		printf ("relative R = %4.2f\n\n", R)
	    }
	}
end

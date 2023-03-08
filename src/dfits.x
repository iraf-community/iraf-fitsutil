include	<ctype.h>
include	"dfits.h"
 
# DFREAD_FORMATS - Read keywords and formats from a file. The keyword and
# the format are extracted from the lines red from the file and are stored
# in the stack. The pointers to the keywords and formats are stored in two
# array in the common block (dfits.com).
# The format strings are converted into FMTIO output format specification
# as they are written into the table.
 
procedure dfread_formats (name)
 
char	name[ARB]	# file name of format file
 
int	ffd, ip, keylen, fmtlen
char	line[SZ_LINE], keyword[SZ_LINE], format[SZ_LINE]
 
bool	check_format()
int	open(), fscan(), strlen(), strext()
include	"dfits.com"
 
begin
	# Open the format file
	ffd = open (name, READ_ONLY, TEXT_FILE)
 
	# Reset counter of keywords (and formats) stored
	nkeywords = 0
 
	# Read the formats (lines) one by one and store it in a
	# table
	while (fscan (ffd) != EOF) {
 
	    # Read line from the file
	    call gargstr (line, SZ_LINE)
 
	    # Extract keyword from line and test it
	    ip =1
	    keylen = strext (line, ip, " ,", YES, keyword, SZ_LINE)
	    if (keylen == 0) {
		call eprintf ("(%s) - Warning: No keyword found (skipped)\n")
		    call pargstr (line)
		next
	    } else if (keylen > SZ_KEYWORD) {
		call eprintf ("(%s) - Warning: Keyword too long (skipped) \n")
		    call pargstr (line)
		next
	    } else
	        call strupr (keyword)
 
	    # Extract format from line and test it
	    fmtlen = strext (line, ip, " \t", YES, format, SZ_LINE)
	    if (check_format (format)) {
		if (strlen (format) > SZ_FORMAT - 1) {
		    call eprintf ("(%s) - Warning: Format too long (skipped)\n")
		        call pargstr (line)
		    next
		}
	    } else {
		call eprintf ( "(%s) - Warning: Bad format (skipped)\n")
		    call pargstr (line)
		next
	    }
 
	    # Do final adjustemnts to keyword and format and store
	    # them into the tables
	    if (nkeywords < MAX_TABLE) {
		nkeywords = nkeywords + 1
		call salloc (key_table[nkeywords], SZ_KEYWORD + 1, TY_CHAR)
		call strcpy (keyword, Memc[key_table[nkeywords]], SZ_KEYWORD)
		call salloc (fmt_table[nkeywords], SZ_FORMAT + 1, TY_CHAR)
		call strcpy ("%", Memc[fmt_table[nkeywords]], 1)
		call strcat (format, Memc[fmt_table[nkeywords]],
			     strlen (format))
		opt_table[nkeywords] = format[strlen (format)]
	    }
 
	    # Debug output (true -> debug active)
	    if (false) {
		call eprintf ("keyword = <%s>  format = <%s>  option = <%c>\n")
		    call pargstr (Memc[key_table[nkeywords]])
		    call pargstr (Memc[fmt_table[nkeywords]])
		    call pargc (opt_table[nkeywords])
	    }
	}
 
	# Close format file
	call close (ffd)
end
 
 
# CHECK_FORMAT - Verify the syntax of a format string. It returns true if
# it's a legal format and false if not. A default format code is appended
# to the format if it's missing.
 
bool procedure check_format (format)
 
char	format[ARB]		# format to parse
 
char	ch			# last character
int	n			# character index
int	state			# parser state
 
begin
	n = 1
	state = 0
	repeat {
	    ch = format[n]
	    switch (state) {
	    case 0:
		if (ch == EOS || ch == '#') {
		    call strcat ("s", format, ARB)
		    return true
		} else if (ch == '.') {
		    state = 2
		    n = n + 1
		} else if (ch == '-') {
		    state = 1
		    n = n + 1
		} else if (IS_DIGIT(ch))
		    state = 1
		else if (IS_FORMAT(ch))
		    return true
		else
		    return false
	    case 1:
		if (ch == EOS || ch == '#') {
		    call strcat ("s", format, ARB)
		    return true
		} else if (ch == '.') {
		    state = 2
		    n = n + 1
		} else if (IS_DIGIT(ch))
		    n = n + 1
		else if (IS_FORMAT(ch)) {
		    state = 3
		    n = n + 1
		} else
		    return false
	    case 2:
		if (ch == EOS || ch == '#') {
		    call strcat ("s", format, ARB)
		    return true
		} else if (IS_DIGIT(ch))
		    n = n + 1
		else if (IS_FORMAT(ch)) {
		   state = 3
		   n = n + 1
		} else
		    return false
	    case 3:
		if (ch == EOS || ch == '#')
		    return true
		else
		    return false
	    default:
		call error (0, "Illegal format parser state")
	    }
	}
end
include	<ctype.h>
 
# STREXT - Extract a word (delimited substring) from a string.
# The input string is scanned from the given initial value until one
# of the delimiters is found. The delimiters are not included in the
# output word.
# Leading white spaces in a word may be optionally skipped. White 
# spaces are skipped before looking at the delimiters string, so it's
# possible to remove leading white spaces and use them as delimiters
# at the same time.
# The value returned is the number of characters in the output string.
# Upon return, the pointer is located at the begining of the next word.
 
int procedure strext (str, ip, dict, skip, outstr, maxch)
 
char	str[ARB]			# input string
int	ip				# pointer into input string
char	dict[ARB]			# dictionary of delimiters
int	skip				# skip leading white spaces ?
char	outstr[ARB]			# extracted word
int	maxch				# max number of chars
 
int	op
int	stridx()
 
begin
	# Skip leading white spaces
	if (skip == YES) {
	    while (IS_WHITE (str[ip]))
		ip = ip + 1
    	}
 
	# Process input string
	for (op=1;  str[ip] != EOS && op <= maxch;  op=op+1)
	    if (stridx (str[ip], dict) == 0) {
		outstr[op] = str[ip]
		ip = ip + 1
	    } else {
		repeat {
		    ip = ip + 1
		} until (stridx (str[ip], dict) == 0 || str[ip] == EOS)
		break
	    }
 
	outstr[op] = EOS
	return (op - 1)
end
 
# PRINT_STRING - Print a quantity as a number or string of characters.
# It first tries to print the quantity with the format code specified
# i.e, string, integer, real or double precission, using the format
# specified. If it fails, it prints the quantity as a string.
# The format is a string of the the form "%W.D" where "W" sets the field
# width and "D" the number of characters or digits to print. It is almost
# an FMTIO specification, except by the format code.
# The format code is the equivalent of the "C" part of an FMTIO format.
# It takes three possible values: "s" for strings, "d" for integers or
# long integers, and "f" for real or double precission numbers.
 
procedure print_string (line, str, format, code)
 
char    line[SZ_LINE]
char	str[SZ_LINE]		# string to print
char	format[SZ_LINE]		# format to use
char	code			# format code
 
char	fmtstr[SZ_LINE]
int	ip, strlen(),pp
long	lval
real	rval
double	dval
 
int	ctol(), ctor(), ctod()
 
begin
	# Build up format string
	call sprintf (fmtstr, SZ_LINE, "%s%c ")
	    call pargstr (format)
	    call pargc (code)
 
 
	# Print according the format specified
	ip = 1
	pp = strlen(line) + 1
	if (IS_STRING(code)) {
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		call pargstr (str)
	} else if (IS_INTEGER(code)) {
	    if (ctol (str, ip, lval) > 0) {
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		    call pargl (lval)
	    } else {
		call sprintf (fmtstr, SZ_LINE, "%ss ")
	    	    call pargstr (format)
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		    call pargstr (str)
	    }
	} else if (IS_FLOAT(code)) {
	    if (ctor (str, ip, rval) > 0) {
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		    call pargr (rval)
	    } else if (ctod (str, ip, dval) > 0) {
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		    call pargd (dval)
	    } else {
		call sprintf (fmtstr, SZ_LINE, "%ss ")
	    	    call pargstr (format)
	    call sprintf (line[pp], SZ_LINE, fmtstr)
		    call pargstr (str)
	    }
	} else
	    call error (0, "Internal error while processing format")
end
 
 
# PRINT_TITLES - Print all the keywords in the table, in the same order they
# have in the table, with the corresponding formats from the format table.
# A newline is printed at the end of the titles (keywords)
 
procedure print_titles
 
int	i, ip, junk
char	width[SZ_LINE], format[SZ_LINE], dict[SZ_LINE]
char    line[SZ_LINE]
 
include	"dfits.com"
int	strext()
 
begin
	# Print all the keywords in the title line
	line[1] = EOS
	do i = 1, nkeywords {
 
	    # Build format
	    ip = 2
	    call sprintf (dict, SZ_LINE, "%s.")
		call pargstr (FORMAT_DICT)
	    junk = strext (Memc[fmt_table[i]], ip, dict, YES, width, SZ_LINE)
	    call sprintf (format, SZ_LINE, "%%%s.%s")
		call pargstr (width)
		call pargstr (width)
 
	    # Print title or debug code (true -> debug active)
	    if (false) {
		call printf ("keyword = <%s>  format = <%s>  title = <")
		    call pargstr (Memc[key_table[i]])
		    call pargstr (format)
		    call print_string (line, Memc[key_table[i]], format, "s")
		    call printf (">\n")
		    call flush (STDOUT)
	    } else
	        call print_string (line, Memc[key_table[i]], format, "s")
	}
 
	# Print a newline at the end of the title line
	call printf ("%80.80s\n")
	    call pargstr (line)
	
end

/* Copyright(c) 1986 Association of Universities for Research in Astronomy Inc.
 */

/*
 * CHECKSUM - Checksum utilities.
 */

/* Explicitly exclude those ASCII characters that fall between the
 * upper and lower case alphanumerics (<=>?@[\]^_`) from the encoding.
 * Which is to say that only the digits 0-9, letters A-Z, and letters
 * a-r should appear in the ASCII coding for the unsigned integers.
 */
#define	NX	13
unsigned exclude[NX] = { 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
			 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60 };

int offset = 0x30;		/* ASCII 0 (zero) character */


void checksum (unsigned char *buf, int length,
               unsigned short *sum16, unsigned int *sum32);

void char_encode (unsigned int value, char *ascii,
                  int nbytes, int permute);
unsigned int add_1s_comp (unsigned int u1, unsigned int u2);
unsigned int addcheck (unsigned int *sum, char *array, int length);
unsigned int addcheck32 (unsigned int *sum, char *array, int length);
unsigned int addcheck1 (unsigned int *sum, char *array, int length);
unsigned int addcheck2 (unsigned int *sum, char *array, int length);


/* CHECKSUM -- Increment the checksum of a character array.  The
 * calling routine must zero the checksum initially.  Shorts are
 * assumed to be 16 bits, ints 32 bits.
 */

/* Internet checksum algorithm, 16/32 bit unsigned integer version:
 */
void
checksum (
    unsigned char	*buf,
    int		        length,
    unsigned short	*sum16,
    unsigned int	*sum32
)
{
	int	 	len, remain, i;
	unsigned int	hi, lo, hicarry, locarry, tmp16;

	len = 4*(length / 4);	/* make sure len is a multiple of 4 */
	remain = length % 4;	/* add remaining bytes below */

	/* Extract the hi and lo words - the 1's complement checksum
	 * is associative and commutative, so it can be accumulated in
	 * any order subject to integer and short integer alignment.
	 * By separating the odd and even short words explicitly, both
	 * the 32 bit and 16 bit checksums are calculated (although the
	 * latter follows directly from the former in any case) and more
	 * importantly, the carry bits can be accumulated efficiently
	 * (subject to short integer overflow - the buffer length should
	 * be restricted to less than 2**17 = 131072).
	 */
	hi = (*sum32 >> 16);
	lo = *sum32 & 0xFFFF;

	for (i=0; i < len; i+=4) {
	    hi += (buf[i]   << 8) + buf[i+1];
	    lo += (buf[i+2] << 8) + buf[i+3];
	}

	/* any remaining bytes are zero filled on the right
	 */
	if (remain) {
	    if (remain >= 1)
		hi += buf[2*len] * 0x100;
	    if (remain >= 2)
		hi += buf[2*len+1];
	    if (remain == 3)
		lo += buf[2*len+2] * 0x100;
	}

	/* fold the carried bits back into the hi and lo words
	 */
	hicarry = hi >> 16;
	locarry = lo >> 16;

	while (hicarry || locarry) {
	    hi = (hi & 0xFFFF) + locarry;
	    lo = (lo & 0xFFFF) + hicarry;
	    hicarry = hi >> 16;
	    locarry = lo >> 16;
	}

	/* simply add the odd and even checksums (with carry) to get the
	 * 16 bit checksum, mask the two to reconstruct the 32 bit sum
	 */
	tmp16 = hi + lo;
	while (tmp16 >> 16)
	    tmp16 = (tmp16 & 0xFFFF) + (tmp16 >> 16);

	*sum16 = tmp16;
	*sum32 = (hi << 16) + lo;
}


/* CHAR_ENCODE -- Encode an unsigned integer into a printable ASCII
 * string.  The input bytes are each represented by four output bytes
 * whose sum is equal to the input integer, offset by 0x30 per byte.
 * The output is restricted to alphanumerics.
 *
 * This is intended to be used to embed the complement of a file checksum
 * within an (originally 0'ed) ASCII field in the file.  The resulting
 * file checksum will then be the 1's complement -0 value (all 1's).
 * This is an additive identity value among other nifty properties.  The
 * embedded ASCII field must be 16 or 32 bit aligned, or the characters
 * can be permuted to compensate.
 *
 * To invert the encoding, simply subtract the offset from each byte
 * and pass the resulting string to checksum.
 */
void
char_encode (
    unsigned int value,
    char	 *ascii,	/* at least 17 characters long */
    int		 nbytes,
    int		 permute
)
{
	int	byte, quotient, remainder, check, i, j, k;
        unsigned int ch[4];
	char	asc[32];

	for (i=0; i < nbytes; i++) {
	    byte = (value << 8*(i+4-nbytes)) >> 24;

	    /* Divide each byte into 4 that are constrained to be printable
	     * ASCII characters.  The four bytes will have the same initial
	     * value (except for the remainder from the division), but will be
	     * shifted higher and lower by pairs to avoid special characters.
	     */
	    quotient = byte / 4 + offset;
	    remainder = byte % 4;

	    for (j=0; j < 4; j++)
		ch[j] = quotient;

	    /* could divide this between the bytes, but the 3 character
	     * slack happens to fit within the ascii alphanumeric range
	     */
	    ch[0] += remainder;

	    /* Any run of adjoining ASCII characters to exclude must be
	     * shorter (including the remainder) than the runs of regular
	     * characters on either side.
	     */
	    check = 1;
	    while (check)
		for (check=0, k=0; k < NX; k++)
		    for (j=0; j < 4; j+=2)
			if (ch[j]==exclude[k] || ch[j+1]==exclude[k]) {
			    ch[j]++;
			    ch[j+1]--;
			    check++;
			}

	    /* ascii[j*nbytes+(i+permute)%nbytes] = ch[j]; */
	    for (j=0; j < 4; j++)
		asc[j*nbytes+i] = ch[j];
	}

	for (i=0; i < 4*nbytes; i++)
	    ascii[i] = asc[(i+4*nbytes-permute)%(4*nbytes)];

	ascii[4*nbytes] = 0;
}


/* ADD_1S_COMP -- add two unsigned integer values using 1's complement
 * addition (wrap the overflow back into the low order bits).  Could do
 * the same thing using checksum(), but this is a little more obvious.
 * To subtract, just complement (~) one of the arguments.
 */
unsigned int
add_1s_comp (
    unsigned int u1,
    unsigned int u2
)
{
	unsigned int	hi, lo, hicarry, locarry;

	hi = (u1 >> 16) + (u2 >> 16);
	lo = ((u1 << 16) >> 16) + ((u2 << 16) >> 16);

	hicarry = hi >> 16;
	locarry = lo >> 16;

	while (hicarry || locarry) {
	    hi = (hi & 0xFFFF) + locarry;
	    lo = (lo & 0xFFFF) + hicarry;
	    hicarry = hi >> 16;
	    locarry = lo >> 16;
	}

	return ((hi << 16) + lo);
}


/*********************************
 *                               *
 *   other checksum algorithms:  *
 *                               *
 *********************************/

/* Internet (1's complement) checksum:
 */
unsigned int
addcheck (
    unsigned int *sum,
    char *array,
    int length
)
{
	register int i;
	unsigned short *iarray;
	int	 len;

	iarray = (unsigned short *) array;
	len = length / 2;

	for (i=0; i<len; i++)
	    *sum += iarray[i];

	/* wrap the carry back into the low bytes (assumes length < 2**17)
	 */
	while (*sum>>16)
	    *sum = (*sum & 0xFFFF) + (*sum>>16);

	return (*sum);
}


/* Internet checksum, 32 bit unsigned integer version:
 */
unsigned int
addcheck32 (
    unsigned int *sum,
    char *array,
    int length
)
{
	register int i;
	unsigned int *iarray, carry=0;
	int	 len, newcarry=0;

	iarray = (unsigned int *) array;
	len = length / 4;

	for (i=0; i<len; i++) {
	    if (iarray[i] > ~ *sum)
		carry++;

	    *sum += iarray[i];
	}

	while (carry) {
	    if (carry > ~ *sum)
		newcarry++;
	    *sum += carry;
	    carry = newcarry;
	    newcarry = 0;
	}

	return (*sum);
}


/* ICE 16 bit microcode checksum:
 */
unsigned int
addcheck1 (
    unsigned int *sum,
    char *array,
    int length
)
{
	register int i;

	for (i = 0; i < length; i++)
	    *sum += (*sum + array[i]);

	return (*sum);
}


/* BSD 16 bit sum algorithm:
 */
unsigned int
addcheck2 (
    unsigned int *sum,
    char *array,
    int length
)
{
	register int i;

	for (i = 0; i < length; i++) {
	    if (*sum & 01)
		*sum = (*sum >> 1) + 0x8000;
	    else
		*sum >>= 1;

	    *sum += (unsigned char) array[i];

	    *sum &= 0xFFFF;
	}

	return (*sum);
}

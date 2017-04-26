include <imio.h>
include <imhdr.h>
include <mef.h>

# MEFCRFITS -- Creates dummy main fits header.

procedure t_fxdummyh()

pointer	sp, path, mef, hdrf
pointer mef_open()


begin
	call smark(sp)
	call salloc (path, SZ_FNAME, TY_CHAR)
	call salloc (hdrf, SZ_FNAME, TY_CHAR)

	call clgstr ("filename", Memc[path], SZ_FNAME)
	call clgstr ("hdr_file", Memc[hdrf], SZ_FNAME)
	mef = mef_open(Memc[path], NEW_FILE, 0)
	call mef_dummyhdr (MEF_FD(mef), Memc[hdrf])
	call mef_close(mef)

	call sfree(sp)
end
	

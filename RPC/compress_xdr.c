/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "compress.h"

bool_t
xdr_image (XDR *xdrs, image *objp)
{
	register int32_t *buf;

	 if (!xdr_int (xdrs, &objp->length))
		 return FALSE;
	 if (!xdr_array (xdrs, (char **)&objp->img.img_val, (u_int *) &objp->img.img_len, ~0,
		sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _COMPRESS_H_RPCGEN
#define _COMPRESS_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif


struct image {
	int length;
	struct {
		u_int img_len;
		char *img_val;
	} img;
};
typedef struct image image;

#define COMPRESS_PROG 0x23451111
#define COMPRESS_VERS 1

#if defined(__STDC__) || defined(__cplusplus)
#define COMPRESS 1
extern  image * compress_1(char **, CLIENT *);
extern  image * compress_1_svc(char **, struct svc_req *);
extern int compress_prog_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define COMPRESS 1
extern  image * compress_1();
extern  image * compress_1_svc();
extern int compress_prog_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_image (XDR *, image*);

#else /* K&R C */
extern bool_t xdr_image ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_COMPRESS_H_RPCGEN */
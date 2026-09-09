#ifndef __CUSTOM_PROTO_CFG_H__
#define __CUSTOM_PROTO_CFG_H__

//#define PROTO_MAHARASHTRA1
//#define PROTO_NIC1
//#define PROTO_CDAC
//#define PROTO_ODISA1
#define PROTO_OG

#if !defined(PROTO_MAHARASHTRA1) && !defined(PROTO_NIC1) && !defined(PROTO_CDAC) && !defined(PROTO_ODISA1) && !defined(PROTO_OG)
#define PROTO_CDAC
#endif

#endif
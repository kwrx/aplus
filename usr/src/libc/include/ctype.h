#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif


#define isalnum(c)							\
	(										\
		((c >= 'a') && (c <= 'z'))		||	\
	 	((c >= 'A') && (c <= 'Z'))		||	\
		((c >= '0') && (c <= '9'))			\
	)

#define isalpha(c)							\
	(										\
		((c >= 'a') && (c <= 'z'))		||	\
	 	((c >= 'A') && (c <= 'Z'))			\
	)

#define islower(c)							\
	(										\
		((c >= 'a') && (c <= 'z'))			\
	)

#define isupper(c)							\
	(										\
		((c >= 'A') && (c <= 'Z'))			\
	)

#define isdigit(c)							\
	(										\
		((c >= '0') && (c <= '9'))			\
	)

#define isxdigit(c)							\
	(										\
		((c >= 'a') && (c <= 'f'))		||	\
	 	((c >= 'A') && (c <= 'F'))		||	\
		((c >= '0') && (c <= '9'))			\
	)

#define iscntrl(c)							\
	(										\
		((c <= 0x1F) || (c == 0x7F))		\
	)

#define isgraph(c)							\
	(										\
		((c >= 0x21) && (c <= 0x2F))	||	\
		((c >= 0x30) && (c <= 0x39))	||	\
		((c >= 0x3A) && (c <= 0x40))	||	\
		((c >= 0x41) && (c <= 0x46))	||	\
		((c >= 0x47) && (c <= 0x5A))	||	\
		((c >= 0x5B) && (c <= 0x60))	||	\
		((c >= 0x61) && (c <= 0x66))	||	\
		((c >= 0x67) && (c <= 0x7A))	||	\
		((c >= 0x7B) && (c <= 0x7E))		\
	)

#define isspace(c)							\
	(										\
		((c == 0x20))					||	\
		((c == 0x0C))					||	\
		((c == 0x0A))					||	\
		((c == 0x0D))					||	\
		((c == 0x09))					||	\
		((c == 0x0B))						\
	)

#define isblank(c)							\
	(										\
		((c == 0x20))					||	\
		((c == 0x09))						\
	)

#define isprint(c)							\
	(										\
		((c == 0x20))					||	\
		((c >= 0x21) && (c <= 0x2F))	||	\
		((c >= 0x30) && (c <= 0x39))	||	\
		((c >= 0x3A) && (c <= 0x40))	||	\
		((c >= 0x41) && (c <= 0x46))	||	\
		((c >= 0x47) && (c <= 0x5A))	||	\
		((c >= 0x5B) && (c <= 0x60))	||	\
		((c >= 0x61) && (c <= 0x66))	||	\
		((c >= 0x67) && (c <= 0x7A))	||	\
		((c >= 0x7B) && (c <= 0x7E))		\
	)

#define ispunct(c)							\
	(										\
		((c >= 0x21) && (c <= 0x2F))	||	\
		((c >= 0x3A) && (c <= 0x40))	||	\
		((c >= 0x5B) && (c <= 0x60))	||	\
		((c >= 0x7B) && (c <= 0x7E))		\
	)			



#define tolower(c)							\
	(										\
		((c >= 'A') && (c <= 'Z')) 	?		\
			(c + 32)				:		\
			(c)								\
	)

#define toupper(c)							\
	(										\
		((c >= 'a') && (c <= 'z')) 	?		\
			(c - 32)				:		\
			(c)								\
	)

#ifdef __cplusplus
}
#endif
#endif

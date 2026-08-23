#line 1 "/sys/src/kryon/src/core/locale.c"
#line 1 "/sys/src/kryon/include/locale.h"



#line 1 "/sys/src/kryon/src/platform/plan9/include/stddef.h"

#line 3 "/sys/src/kryon/src/platform/plan9/include/stddef.h"



#line 1 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"

#line 13 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"




#line 18 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"

#line 1 "/386/include/u.h"

typedef	unsigned short	ushort;
typedef	unsigned char	uchar;
typedef unsigned long	ulong;
typedef unsigned int	uint;
typedef   signed char	schar;
typedef	long long	vlong;
typedef	unsigned long long uvlong;
typedef unsigned long	uintptr;
typedef unsigned long	usize;
typedef	uint		Rune;
typedef union FPdbleword FPdbleword;
typedef long		jmp_buf[2];



typedef unsigned int	mpdigit;
typedef unsigned char	u8int;
typedef unsigned short	u16int;
typedef unsigned int	u32int;
typedef unsigned long long u64int;






















union FPdbleword
{
	double	x;
	struct {
		ulong lo;
		ulong hi;
	};
};

typedef	char*	va_list;

#line 58 "/386/include/u.h"

#line 60 "/386/include/u.h"

#line 66 "/386/include/u.h"
#line 20 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"


#line 1 "/sys/include/libc.h"
#pragma	lib	"libc.a"
#pragma	src	"/sys/src/libc"






#line 11 "/sys/include/libc.h"
extern	void*	memccpy(void*, void*, int, ulong);
extern	void*	memset(void*, int, ulong);
extern	int	memcmp(void*, void*, ulong);
extern	void*	memcpy(void*, void*, ulong);
extern	void*	memmove(void*, void*, ulong);
extern	void*	memchr(void*, int, ulong);


#line 21 "/sys/include/libc.h"
extern	char*	strcat(char*, char*);
extern	char*	strchr(char*, int);
extern	int	strcmp(char*, char*);
extern	char*	strcpy(char*, char*);
extern	char*	strecpy(char*, char*, char*);
extern	char*	strdup(char*);
extern	char*	strncat(char*, char*, long);
extern	char*	strncpy(char*, char*, long);
extern	int	strncmp(char*, char*, long);
extern	char*	strpbrk(char*, char*);
extern	char*	strrchr(char*, int);
extern	char*	strtok(char*, char*);
extern	long	strlen(char*);
extern	long	strspn(char*, char*);
extern	long	strcspn(char*, char*);
extern	char*	strstr(char*, char*);
extern	int	cistrncmp(char*, char*, int);
extern	int	cistrcmp(char*, char*);
extern	char*	cistrstr(char*, char*);
extern	int	tokenize(char*, char**, int);

enum
{
	UTFmax		= 4,
	Runesync	= 0x80,
	Runeself	= 0x80,
	Runeerror	= 0xFFFD,
	Runemax		= 0x10FFFF,
	Runemask	= 0x1FFFFF,
};


#line 55 "/sys/include/libc.h"
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(long);
extern	int	runenlen(Rune*, int);
extern	int	fullrune(char*, int);
extern	int	utflen(char*);
extern	int	utfnlen(char*, long);
extern	char*	utfrune(char*, long);
extern	char*	utfrrune(char*, long);
extern	char*	utfutf(char*, char*);
extern	char*	utfecpy(char*, char*, char*);

extern	Rune*	runestrcat(Rune*, Rune*);
extern	Rune*	runestrchr(Rune*, Rune);
extern	int	runestrcmp(Rune*, Rune*);
extern	Rune*	runestrcpy(Rune*, Rune*);
extern	Rune*	runestrncpy(Rune*, Rune*, long);
extern	Rune*	runestrecpy(Rune*, Rune*, Rune*);
extern	Rune*	runestrdup(Rune*);
extern	Rune*	runestrncat(Rune*, Rune*, long);
extern	int	runestrncmp(Rune*, Rune*, long);
extern	Rune*	runestrrchr(Rune*, Rune);
extern	long	runestrlen(Rune*);
extern	Rune*	runestrstr(Rune*, Rune*);

extern	Rune	tolowerrune(Rune);
extern	Rune	totitlerune(Rune);
extern	Rune	toupperrune(Rune);
extern	Rune	tobaserune(Rune);
extern	int	isalpharune(Rune);
extern	int	isbaserune(Rune);
extern	int	isdigitrune(Rune);
extern	int	islowerrune(Rune);
extern	int	isspacerune(Rune);
extern	int	istitlerune(Rune);
extern	int	isupperrune(Rune);


#line 95 "/sys/include/libc.h"
extern	void*	malloc(ulong);
extern	void*	mallocz(ulong, int);
extern	void	free(void*);
extern	ulong	msize(void*);
extern	void*	mallocalign(ulong, ulong, long, ulong);
extern	void*	calloc(ulong, ulong);
extern	void*	realloc(void*, ulong);
extern	void	setmalloctag(void*, ulong);
extern	void	setrealloctag(void*, ulong);
extern	ulong	getmalloctag(void*);
extern	ulong	getrealloctag(void*);
extern	void*	malloctopoolblock(void*);


#line 111 "/sys/include/libc.h"
typedef struct Fmt	Fmt;
struct Fmt{
	uchar	runes;
	void	*start;
	void	*to;
	void	*stop;
	int	(*flush)(Fmt *);
	void	*farg;
	int	nfmt;
	va_list	args;
	int	r;
	int	width;
	int	prec;
	ulong	flags;
};

enum{
	FmtWidth	= 1,
	FmtLeft		= FmtWidth << 1,
	FmtPrec		= FmtLeft << 1,
	FmtSharp	= FmtPrec << 1,
	FmtSpace	= FmtSharp << 1,
	FmtSign		= FmtSpace << 1,
	FmtZero		= FmtSign << 1,
	FmtUnsigned	= FmtZero << 1,
	FmtShort	= FmtUnsigned << 1,
	FmtLong		= FmtShort << 1,
	FmtVLong	= FmtLong << 1,
	FmtComma	= FmtVLong << 1,
	FmtByte		= FmtComma << 1,

	FmtFlag		= FmtByte << 1
};

extern	int	print(char*, ...);
extern	char*	seprint(char*, char*, char*, ...);
extern	char*	vseprint(char*, char*, char*, va_list);
extern	int	snprint(char*, int, char*, ...);
extern	int	vsnprint(char*, int, char*, va_list);
extern	char*	smprint(char*, ...);
extern	char*	vsmprint(char*, va_list);
extern	int	sprint(char*, char*, ...);
extern	int	fprint(int, char*, ...);
extern	int	vfprint(int, char*, va_list);

extern	int	runesprint(Rune*, char*, ...);
extern	int	runesnprint(Rune*, int, char*, ...);
extern	int	runevsnprint(Rune*, int, char*, va_list);
extern	Rune*	runeseprint(Rune*, Rune*, char*, ...);
extern	Rune*	runevseprint(Rune*, Rune*, char*, va_list);
extern	Rune*	runesmprint(char*, ...);
extern	Rune*	runevsmprint(char*, va_list);

extern	int	fmtfdinit(Fmt*, int, char*, int);
extern	int	fmtfdflush(Fmt*);
extern	int	fmtstrinit(Fmt*);
extern	char*	fmtstrflush(Fmt*);
extern	int	runefmtstrinit(Fmt*);
extern	Rune*	runefmtstrflush(Fmt*);

#pragma	varargck	argpos	fmtprint	2
#pragma	varargck	argpos	fprint		2
#pragma	varargck	argpos	print		1
#pragma	varargck	argpos	runeseprint	3
#pragma	varargck	argpos	runesmprint	1
#pragma	varargck	argpos	runesnprint	3
#pragma	varargck	argpos	runesprint	2
#pragma	varargck	argpos	seprint		3
#pragma	varargck	argpos	smprint		1
#pragma	varargck	argpos	snprint		3
#pragma	varargck	argpos	sprint		2

#pragma	varargck	type	"lld"	vlong
#pragma	varargck	type	"llo"	vlong
#pragma	varargck	type	"llx"	vlong
#pragma	varargck	type	"llb"	vlong
#pragma	varargck	type	"lld"	uvlong
#pragma	varargck	type	"llo"	uvlong
#pragma	varargck	type	"llx"	uvlong
#pragma	varargck	type	"llb"	uvlong
#pragma	varargck	type	"ld"	long
#pragma	varargck	type	"lo"	long
#pragma	varargck	type	"lx"	long
#pragma	varargck	type	"lb"	long
#pragma	varargck	type	"ld"	ulong
#pragma	varargck	type	"lo"	ulong
#pragma	varargck	type	"lx"	ulong
#pragma	varargck	type	"lb"	ulong
#pragma	varargck	type	"d"	int
#pragma	varargck	type	"o"	int
#pragma	varargck	type	"x"	int
#pragma	varargck	type	"c"	int
#pragma	varargck	type	"C"	int
#pragma	varargck	type	"b"	int
#pragma	varargck	type	"d"	uint
#pragma	varargck	type	"x"	uint
#pragma	varargck	type	"c"	uint
#pragma	varargck	type	"C"	uint
#pragma	varargck	type	"b"	uint
#pragma	varargck	type	"f"	double
#pragma	varargck	type	"e"	double
#pragma	varargck	type	"g"	double
#pragma	varargck	type	"s"	char*
#pragma	varargck	type	"q"	char*
#pragma	varargck	type	"S"	Rune*
#pragma	varargck	type	"Q"	Rune*
#pragma	varargck	type	"r"	void
#pragma	varargck	type	"%"	void
#pragma	varargck	type	"n"	int*
#pragma	varargck	type	"p"	uintptr
#pragma	varargck	type	"p"	void*
#pragma	varargck	flag	','
#pragma	varargck	flag	' '
#pragma	varargck	flag	'h'
#pragma varargck	type	"<"	void*
#pragma varargck	type	"["	void*
#pragma varargck	type	"H"	void*
#pragma varargck	type	"lH"	void*

extern	int	fmtinstall(int, int (*)(Fmt*));
extern	int	dofmt(Fmt*, char*);
extern	int	dorfmt(Fmt*, Rune*);
extern	int	fmtprint(Fmt*, char*, ...);
extern	int	fmtvprint(Fmt*, char*, va_list);
extern	int	fmtrune(Fmt*, int);
extern	int	fmtstrcpy(Fmt*, char*);
extern	int	fmtrunestrcpy(Fmt*, Rune*);

#line 242 "/sys/include/libc.h"
extern	int	errfmt(Fmt *f);


#line 247 "/sys/include/libc.h"
extern	char	*unquotestrdup(char*);
extern	Rune	*unquoterunestrdup(Rune*);
extern	char	*quotestrdup(char*);
extern	Rune	*quoterunestrdup(Rune*);
extern	int	quotestrfmt(Fmt*);
extern	int	quoterunestrfmt(Fmt*);
extern	void	quotefmtinstall(void);
extern	int	(*doquote)(int);
extern	int	needsrcquote(int);


#line 260 "/sys/include/libc.h"
extern	void	srand(long);
extern	int	rand(void);
extern	int	nrand(int);
extern	long	lrand(void);
extern	long	lnrand(long);
extern	double	frand(void);
extern	ulong	truerand(void);
extern	ulong	ntruerand(ulong);


#line 272 "/sys/include/libc.h"
extern	ulong	getfcr(void);
extern	void	setfsr(ulong);
extern	ulong	getfsr(void);
extern	void	setfcr(ulong);
extern	double	NaN(void);
extern	double	Inf(int);
extern	int	isNaN(double);
extern	int	isInf(double, int);
extern	ulong	umuldiv(ulong, ulong, ulong);
extern	long	muldiv(long, long, long);

extern	double	pow(double, double);
extern	double	atan2(double, double);
extern	double	fabs(double);
extern	double	atan(double);
extern	double	log(double);
extern	double	log10(double);
extern	double	exp(double);
extern	double	floor(double);
extern	double	ceil(double);
extern	double	hypot(double, double);
extern	double	sin(double);
extern	double	cos(double);
extern	double	tan(double);
extern	double	asin(double);
extern	double	acos(double);
extern	double	sinh(double);
extern	double	cosh(double);
extern	double	tanh(double);
extern	double	sqrt(double);
extern	double	fmod(double, double);






#line 311 "/sys/include/libc.h"

typedef
struct Tm
{
	int	sec;
	int	min;
	int	hour;
	int	mday;
	int	mon;
	int	year;
	int	wday;
	int	yday;
	char	zone[4];
	int	tzoff;
} Tm;

extern	Tm*	gmtime(long);
extern	Tm*	localtime(long);
extern	char*	asctime(Tm*);
extern	char*	ctime(long);
extern	double	cputime(void);
extern	long	times(long*);
extern	long	tm2sec(Tm*);
extern	vlong	nsec(void);

extern	void	cycles(uvlong*);


#line 341 "/sys/include/libc.h"
extern u16int	le16get(uchar *t,  uchar **r);
extern u32int	le24get(uchar *t,  uchar **r);
extern u32int	le32get(uchar *t,  uchar **r);
extern u64int	le64get(uchar *t,  uchar **r);
extern uchar*	le16put(uchar *t, u16int r);
extern uchar*	le24put(uchar *t, u32int r);
extern uchar*	le32put(uchar *t, u32int r);
extern uchar*	le64put(uchar *t, u64int r);
extern u16int	be16get(uchar *t,  uchar **r);
extern u32int	be24get(uchar *t,  uchar **r);
extern u32int	be32get(uchar *t,  uchar **r);
extern u64int	be64get(uchar *t,  uchar **r);
extern uchar*	be16put(uchar *t, u16int r);
extern uchar*	be24put(uchar *t, u32int r);
extern uchar*	be32put(uchar *t, u32int r);
extern uchar*	be64put(uchar *t, u64int r);


#line 361 "/sys/include/libc.h"
enum
{
	PNPROC		= 1,
	PNGROUP		= 2,
};

extern	void	_assert(char*);
extern	int	abs(int);
extern	int	atexit(void(*)(void));
extern	void	atexitdont(void(*)(void));
extern	int	atnotify(int(*)(void*, char*), int);
extern	double	atof(char*);
extern	int	atoi(char*);
extern	long	atol(char*);
extern	vlong	atoll(char*);
extern	double	charstod(int(*)(void*), void*);
extern	char*	cleanname(char*);
extern	int	decrypt(void*, void*, int);
extern	int	encrypt(void*, void*, int);
extern	int	dec64(uchar*, int, char*, int);
extern	int	enc64(char*, int, uchar*, int);
extern	int	dec32(uchar*, int, char*, int);
extern	int	enc32(char*, int, uchar*, int);
extern	int	dec16(uchar*, int, char*, int);
extern	int	enc16(char*, int, uchar*, int);
extern	int	encodefmt(Fmt*);
extern	void	exits(char*);
extern	double	frexp(double, int*);
extern	uintptr	getcallerpc(void*);
extern	char*	getenv(char*);
extern	int	getfields(char*, char**, int, int, char*);
extern	int	gettokens(char *, char **, int, char *);
extern	char*	getuser(void);
extern	char*	getwd(char*, int);
extern	int	iounit(int);
extern	long	labs(long);
extern	double	ldexp(double, int);
extern	void	longjmp(jmp_buf, int);
extern	char*	mktemp(char*);
extern	double	modf(double, double*);
extern	int	netcrypt(void*, void*);
extern	void	notejmp(void*, jmp_buf, int);
extern	void	perror(char*);
extern	int	postnote(int, int, char *);
extern	double	pow10(int);
extern	void	procsetname(char*, ...);
extern	int	putenv(char*, char*);
extern	void	qsort(void*, long, long, int (*)(void*, void*));
extern	int	setjmp(jmp_buf);
extern	double	strtod(char*, char**);
extern	long	strtol(char*, char**, int);
extern	ulong	strtoul(char*, char**, int);
extern	vlong	strtoll(char*, char**, int);
extern	uvlong	strtoull(char*, char**, int);
extern	void	sysfatal(char*, ...);
#pragma	varargck	argpos	sysfatal	1
extern	void	syslog(int, char*, char*, ...);
#pragma	varargck	argpos	syslog	3
extern	ulong	time(long*);
extern	int	tolower(int);
extern	int	toupper(int);


#line 426 "/sys/include/libc.h"
enum {
	Profoff,
	Profuser,
	Profkernel,
	Proftime,
	Profsample,
};
extern	void	prof(void (*fn)(void*), void *arg, int entries, int what);


#line 438 "/sys/include/libc.h"
long	ainc(long*);
long	adec(long*);
int	cas32(u32int*, u32int, u32int);
int	casp(void**, void*, void*);
int	casl(ulong*, ulong, ulong);


#line 447 "/sys/include/libc.h"
typedef
struct Lock {
	long	key;
	long	sem;
} Lock;

extern int	_tas(int*);

extern	void	lock(Lock*);
extern	void	unlock(Lock*);
extern	int	canlock(Lock*);

typedef struct QLp QLp;
struct QLp
{
	int	inuse;
	QLp	*next;
	char	state;
};

typedef
struct QLock
{
	Lock	lock;
	int	locked;
	QLp	*head;
	QLp 	*tail;
} QLock;

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);
extern	void	_qlockinit(void* (*)(void*, void*));

typedef
struct RWLock
{
	Lock	lock;
	int	readers;
	int	writer;
	QLp	*head;
	QLp	*tail;
} RWLock;

extern	void	rlock(RWLock*);
extern	void	runlock(RWLock*);
extern	int	canrlock(RWLock*);
extern	void	wlock(RWLock*);
extern	void	wunlock(RWLock*);
extern	int	canwlock(RWLock*);

typedef
struct Rendez
{
	QLock	*l;
	QLp	*head;
	QLp	*tail;
} Rendez;

extern	void	rsleep(Rendez*);
extern	int	rwakeup(Rendez*);
extern	int	rwakeupall(Rendez*);
extern	void**	privalloc(void);
extern	void	privfree(void**);


#line 515 "/sys/include/libc.h"

extern	int	accept(int, char*);
extern	int	announce(char*, char*);
extern	int	dial(char*, char*, char*, int*);
extern	void	setnetmtpt(char*, int, char*);
extern	int	hangup(int);
extern	int	listen(char*, char*);
extern	char*	netmkaddr(char*, char*, char*);
extern	int	reject(int, char*, char*);


#line 528 "/sys/include/libc.h"
extern	int	pushssl(int, char*, char*, char*, int*);
extern	int	pushtls(int, char*, char*, int, char*, char*);


#line 534 "/sys/include/libc.h"
typedef struct NetConnInfo NetConnInfo;
struct NetConnInfo
{
	char	*dir;
	char	*root;
	char	*spec;
	char	*lsys;
	char	*lserv;
	char	*rsys;
	char	*rserv;
	char	*laddr;
	char	*raddr;
};
extern	NetConnInfo*	getnetconninfo(char*, int);
extern	void		freenetconninfo(NetConnInfo*);


#line 554 "/sys/include/libc.h"

























































enum
{
	RFNAMEG		= (1<<0),
	RFENVG		= (1<<1),
	RFFDG		= (1<<2),
	RFNOTEG		= (1<<3),
	RFPROC		= (1<<4),
	RFMEM		= (1<<5),
	RFNOWAIT	= (1<<6),
	RFCNAMEG	= (1<<10),
	RFCENVG		= (1<<11),
	RFCFDG		= (1<<12),
	RFREND		= (1<<13),
	RFNOMNT		= (1<<14)
};

typedef
struct Qid
{
	uvlong	path;
	ulong	vers;
	uchar	type;
} Qid;

typedef
struct Dir {

	ushort	type;
	uint	dev;

	Qid	qid;
	ulong	mode;
	ulong	atime;
	ulong	mtime;
	vlong	length;
	char	*name;
	char	*uid;
	char	*gid;
	char	*muid;
} Dir;


typedef
struct Waitmsg
{
	int	pid;
	ulong	time[3];
	char	*msg;
} Waitmsg;

typedef
struct IOchunk
{
	void	*addr;
	ulong	len;
} IOchunk;

extern	void	_exits(char*);

extern	void	abort(void);
extern	int	access(char*, int);
extern	long	alarm(ulong);
extern	int	await(char*, int);
extern	int	bind(char*, char*, int);
extern	int	brk(void*);
extern	int	chdir(char*);
extern	int	close(int);
extern	int	create(char*, int, ulong);
extern	int	dup(int, int);
extern	int	errstr(char*, uint);
extern	int	exec(char*, char*[]);
extern	int	execl(char*, ...);
extern	int	fork(void);
extern	int	rfork(int);
extern	int	fauth(int, char*);
extern	int	fstat(int, uchar*, int);
extern	int	fwstat(int, uchar*, int);
extern	int	fversion(int, int, char*, int);
extern	int	mount(int, int, char*, int, char*);
extern	int	unmount(char*, char*);
extern	int	noted(int);
extern	int	notify(void(*)(void*, char*));
extern	int	open(char*, int);
extern	int	fd2path(int, char*, int);

extern	int	pipe(int*);
extern	long	pread(int, void*, long, vlong);
extern	long	preadv(int, IOchunk*, int, vlong);
extern	long	pwrite(int, void*, long, vlong);
extern	long	pwritev(int, IOchunk*, int, vlong);
extern	long	read(int, void*, long);
extern	long	readn(int, void*, long);
extern	long	readv(int, IOchunk*, int);
extern	int	remove(char*);
extern	void*	sbrk(ulong);
extern	long	oseek(int, long, int);
extern	vlong	seek(int, vlong, int);
extern	void*	segattach(int, char*, void*, ulong);
extern	void*	segbrk(void*, void*);
extern	int	segdetach(void*);
extern	int	segflush(void*, ulong);
extern	int	segfree(void*, ulong);
extern	int	semacquire(long*, int);
extern	long	semrelease(long*, long);
extern	int	sleep(long);
extern	int	stat(char*, uchar*, int);
extern	int	tsemacquire(long*, ulong);
extern	Waitmsg*	wait(void);
extern	int	waitpid(void);
extern	long	write(int, void*, long);
extern	long	writev(int, IOchunk*, int);
extern	int	wstat(char*, uchar*, int);
extern	void*	rendezvous(void*, void*);

extern	Dir*	dirstat(char*);
extern	Dir*	dirfstat(int);
extern	int	dirwstat(char*, Dir*);
extern	int	dirfwstat(int, Dir*);
extern	long	dirread(int, Dir**);
extern	void	nulldir(Dir*);
extern	long	dirreadall(int, Dir**);
extern	int	getpid(void);
extern	int	getppid(void);
extern	void	rerrstr(char*, uint);
extern	char*	sysname(void);
extern	void	werrstr(char*, ...);
#pragma	varargck	argpos	werrstr	1

extern char *argv0;

#line 752 "/sys/include/libc.h"


#line 755 "/sys/include/libc.h"

#line 757 "/sys/include/libc.h"




extern	char	end[];
#line 23 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"









typedef ulong size_t;



#line 7 "/sys/src/kryon/src/platform/plan9/include/stddef.h"

typedef long ptrdiff_t;


#line 5 "/sys/src/kryon/include/locale.h"





typedef struct LocaleEntry {
    char *key;
    char *value;
} LocaleEntry;

typedef struct LocaleLanguage {
    char *code;
    char *label;
} LocaleLanguage;

void InitLocale(void);
int SetLocale(const char *code);
const char *GetSystemLocaleCode(void);
const char *GetDefaultLocaleCode(void);
const char *GetLocaleText(const char *key);
void FormatLocaleText(char *dst, size_t dst_size, const char *key, ...);

int GetLocaleCount(void);
const char *GetLocaleCode(int index);
const char *GetLocaleLabel(int index);
int GetLocaleIndex(const char *code);
const char *GetCurrentLocaleCode(void);
int GetCurrentLocaleIndex(void);






#line 2 "/sys/src/kryon/src/core/locale.c"
#line 1 "/sys/src/kryon/include/platform.h"































typedef void *(*KryThreadMain)(void *userdata);

typedef struct KryThread {



    int handle;



} KryThread;

typedef struct KryMutex {



    int lock;



} KryMutex;









int KryThreadStart(KryThread *thread, KryThreadMain fn, void *userdata);
void KryThreadDetach(KryThread *thread);
void KryThreadJoin(KryThread *thread);
void KrySleepSeconds(int seconds);
void KryMutexInit(KryMutex *mutex);
void KryMutexLock(KryMutex *mutex);
void KryMutexUnlock(KryMutex *mutex);


#line 3 "/sys/src/kryon/src/core/locale.c"
#line 1 "/sys/src/kryon/include/embedded_assets.h"







typedef struct EmbeddedAsset {
    const char *path;
    const char *mime;
    const unsigned char *data;
    unsigned int size;
} EmbeddedAsset;

extern const EmbeddedAsset embedded_assets[];
extern const unsigned int embedded_asset_count;

const EmbeddedAsset *GetEmbeddedAsset(const char *path);
char *LoadEmbeddedAssetText(const char *path);
const char *GetEmbeddedAssetExtension(const char *path);






#line 4 "/sys/src/kryon/src/core/locale.c"

#line 1 "/sys/src/kryon/include/kryon.h"



#line 1 "/sys/src/kryon/include/kryon_version.h"









#line 5 "/sys/src/kryon/include/kryon.h"

#line 7 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"




typedef void (*KeyInputPlatformCallback)(void);
typedef int (*KeyPlatformCallback)(int key);

int SetKeyboardInputEnabled(int enabled);
int KeyboardInputEnabled(void);
void SetKeyPlatformCallbacks(KeyInputPlatformCallback update,
                             KeyPlatformCallback key_pressed,
                             KeyPlatformCallback key_down);
void UpdateKeyPlatformState(void);




#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"




#line 1 "/sys/src/kryon/src/platform/plan9/include/stdarg.h"

#line 7 "/sys/src/kryon/src/platform/plan9/include/stdarg.h"



#line 1 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"

#line 13 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"




#line 18 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"


















#line 11 "/sys/src/kryon/src/platform/plan9/include/stdarg.h"


#line 120 "/sys/src/kryon/include/kryon_compat.generated.h"























































































































    typedef enum bool { false = 0, true = !false } bool;









typedef struct Vector2 {
    float x;
    float y;
} Vector2;


typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;


typedef struct Vector4 {
    float x;
    float y;
    float z;
    float w;
} Vector4;


typedef Vector4 Quaternion;


typedef struct Matrix {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;


typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;


typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;


typedef struct Image {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} Image;


typedef struct Texture {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture;


typedef Texture Texture2D;


typedef Texture TextureCubemap;


typedef struct RenderTexture {
    unsigned int id;
    Texture texture;
    Texture depth;
} RenderTexture;


typedef RenderTexture RenderTexture2D;


typedef struct NPatchInfo {
    Rectangle source;
    int left;
    int top;
    int right;
    int bottom;
    int layout;
} NPatchInfo;


typedef struct GlyphInfo {
    int value;
    int offsetX;
    int offsetY;
    int advanceX;
    Image image;
} GlyphInfo;


typedef struct Font {
    int baseSize;
    int glyphCount;
    int glyphPadding;
    Texture2D texture;
    Rectangle *recs;
    GlyphInfo *glyphs;
} Font;


typedef struct Camera3D {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
    int projection;
} Camera3D;

typedef Camera3D Camera;


typedef struct Camera2D {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;


typedef struct Mesh {
    int vertexCount;
    int triangleCount;


    float *vertices;
    float *texcoords;
    float *texcoords2;
    float *normals;
    float *tangents;
    unsigned char *colors;
    unsigned short *indices;


    int boneCount;
    unsigned char *boneIndices;
    float *boneWeights;



    float *animVertices;
    float *animNormals;


    unsigned int vaoId;
    unsigned int *vboId;
} Mesh;


typedef struct Shader {
    unsigned int id;
    int *locs;
} Shader;


typedef struct MaterialMap {
    Texture2D texture;
    Color color;
    float value;
} MaterialMap;


typedef struct Material {
    Shader shader;
    MaterialMap *maps;
    float params[4];
} Material;


typedef struct Transform {
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale;
} Transform;


typedef Transform *ModelAnimPose;


typedef struct BoneInfo {
    char name[32];
    int parent;
} BoneInfo;


typedef struct ModelSkeleton {
    unsigned int boneCount;
    BoneInfo *bones;
    ModelAnimPose bindPose;
} ModelSkeleton;


typedef struct Model {
    Matrix transform;

    int meshCount;
    int materialCount;
    Mesh *meshes;
    Material *materials;
    int *meshMaterial;


    ModelSkeleton skeleton;


    ModelAnimPose currentPose;
    Matrix *boneMatrices;
} Model;


typedef struct ModelAnimation {
    char name[32];

    unsigned int boneCount;
    int keyframeCount;
    ModelAnimPose *keyframePoses;
} ModelAnimation;


typedef struct Ray {
    Vector3 position;
    Vector3 direction;
} Ray;


typedef struct RayCollision {
    bool hit;
    float distance;
    Vector3 point;
    Vector3 normal;
} RayCollision;


typedef struct BoundingBox {
    Vector3 min;
    Vector3 max;
} BoundingBox;


typedef struct Wave {
    unsigned int frameCount;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
    void *data;
} Wave;



typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;


typedef struct AudioStream {
    rAudioBuffer *buffer;
    rAudioProcessor *processor;

    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
} AudioStream;


typedef struct Sound {
    AudioStream stream;
    unsigned int frameCount;
} Sound;


typedef struct Music {
    AudioStream stream;
    unsigned int frameCount;
    bool looping;

    int ctxType;
    void *ctxData;
} Music;


typedef struct VrDeviceInfo {
    int hResolution;
    int vResolution;
    float hScreenSize;
    float vScreenSize;
    float eyeToScreenDistance;
    float lensSeparationDistance;
    float interpupillaryDistance;
    float lensDistortionValues[4];
    float chromaAbCorrection[4];
} VrDeviceInfo;


typedef struct VrStereoConfig {
    Matrix projection[2];
    Matrix viewOffset[2];
    float leftLensCenter[2];
    float rightLensCenter[2];
    float leftScreenCenter[2];
    float rightScreenCenter[2];
    float scale[2];
    float scaleIn[2];
} VrStereoConfig;


typedef struct FilePathList {
    unsigned int count;
    char **paths;
} FilePathList;


typedef struct AutomationEvent {
    unsigned int frame;
    unsigned int type;
    int params[4];
} AutomationEvent;


typedef struct AutomationEventList {
    unsigned int capacity;
    unsigned int count;
    AutomationEvent *events;
} AutomationEventList;







typedef enum {
    FLAG_VSYNC_HINT         = 0x00000040,
    FLAG_FULLSCREEN_MODE    = 0x00000002,
    FLAG_WINDOW_RESIZABLE   = 0x00000004,
    FLAG_WINDOW_UNDECORATED = 0x00000008,
    FLAG_WINDOW_HIDDEN      = 0x00000080,
    FLAG_WINDOW_MINIMIZED   = 0x00000200,
    FLAG_WINDOW_MAXIMIZED   = 0x00000400,
    FLAG_WINDOW_UNFOCUSED   = 0x00000800,
    FLAG_WINDOW_TOPMOST     = 0x00001000,
    FLAG_WINDOW_ALWAYS_RUN  = 0x00000100,
    FLAG_WINDOW_TRANSPARENT = 0x00000010,
    FLAG_WINDOW_HIGHDPI     = 0x00002000,
    FLAG_WINDOW_MOUSE_PASSTHROUGH = 0x00004000,
    FLAG_BORDERLESS_WINDOWED_MODE = 0x00008000,
    FLAG_MSAA_4X_HINT       = 0x00000020,
    FLAG_INTERLACED_HINT    = 0x00010000
} ConfigFlags;



typedef enum {
    LOG_ALL = 0,
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL,
    LOG_NONE
} TraceLogLevel;



typedef enum {
    KEY_NULL            = 0,

    KEY_APOSTROPHE      = 39,
    KEY_COMMA           = 44,
    KEY_MINUS           = 45,
    KEY_PERIOD          = 46,
    KEY_SLASH           = 47,
    KEY_ZERO            = 48,
    KEY_ONE             = 49,
    KEY_TWO             = 50,
    KEY_THREE           = 51,
    KEY_FOUR            = 52,
    KEY_FIVE            = 53,
    KEY_SIX             = 54,
    KEY_SEVEN           = 55,
    KEY_EIGHT           = 56,
    KEY_NINE            = 57,
    KEY_SEMICOLON       = 59,
    KEY_EQUAL           = 61,
    KEY_A               = 65,
    KEY_B               = 66,
    KEY_C               = 67,
    KEY_D               = 68,
    KEY_E               = 69,
    KEY_F               = 70,
    KEY_G               = 71,
    KEY_H               = 72,
    KEY_I               = 73,
    KEY_J               = 74,
    KEY_K               = 75,
    KEY_L               = 76,
    KEY_M               = 77,
    KEY_N               = 78,
    KEY_O               = 79,
    KEY_P               = 80,
    KEY_Q               = 81,
    KEY_R               = 82,
    KEY_S               = 83,
    KEY_T               = 84,
    KEY_U               = 85,
    KEY_V               = 86,
    KEY_W               = 87,
    KEY_X               = 88,
    KEY_Y               = 89,
    KEY_Z               = 90,
    KEY_LEFT_BRACKET    = 91,
    KEY_BACKSLASH       = 92,
    KEY_RIGHT_BRACKET   = 93,
    KEY_GRAVE           = 96,

    KEY_SPACE           = 32,
    KEY_ESCAPE          = 256,
    KEY_ENTER           = 257,
    KEY_TAB             = 258,
    KEY_BACKSPACE       = 259,
    KEY_INSERT          = 260,
    KEY_DELETE          = 261,
    KEY_RIGHT           = 262,
    KEY_LEFT            = 263,
    KEY_DOWN            = 264,
    KEY_UP              = 265,
    KEY_PAGE_UP         = 266,
    KEY_PAGE_DOWN       = 267,
    KEY_HOME            = 268,
    KEY_END             = 269,
    KEY_CAPS_LOCK       = 280,
    KEY_SCROLL_LOCK     = 281,
    KEY_NUM_LOCK        = 282,
    KEY_PRINT_SCREEN    = 283,
    KEY_PAUSE           = 284,
    KEY_F1              = 290,
    KEY_F2              = 291,
    KEY_F3              = 292,
    KEY_F4              = 293,
    KEY_F5              = 294,
    KEY_F6              = 295,
    KEY_F7              = 296,
    KEY_F8              = 297,
    KEY_F9              = 298,
    KEY_F10             = 299,
    KEY_F11             = 300,
    KEY_F12             = 301,
    KEY_LEFT_SHIFT      = 340,
    KEY_LEFT_CONTROL    = 341,
    KEY_LEFT_ALT        = 342,
    KEY_LEFT_SUPER      = 343,
    KEY_RIGHT_SHIFT     = 344,
    KEY_RIGHT_CONTROL   = 345,
    KEY_RIGHT_ALT       = 346,
    KEY_RIGHT_SUPER     = 347,
    KEY_KB_MENU         = 348,

    KEY_KP_0            = 320,
    KEY_KP_1            = 321,
    KEY_KP_2            = 322,
    KEY_KP_3            = 323,
    KEY_KP_4            = 324,
    KEY_KP_5            = 325,
    KEY_KP_6            = 326,
    KEY_KP_7            = 327,
    KEY_KP_8            = 328,
    KEY_KP_9            = 329,
    KEY_KP_DECIMAL      = 330,
    KEY_KP_DIVIDE       = 331,
    KEY_KP_MULTIPLY     = 332,
    KEY_KP_SUBTRACT     = 333,
    KEY_KP_ADD          = 334,
    KEY_KP_ENTER        = 335,
    KEY_KP_EQUAL        = 336,

    KEY_BACK            = 4,
    KEY_MENU            = 5,
    KEY_VOLUME_UP       = 24,
    KEY_VOLUME_DOWN     = 25
} KeyboardKey;







typedef enum {
    MOUSE_BUTTON_LEFT    = 0,
    MOUSE_BUTTON_RIGHT   = 1,
    MOUSE_BUTTON_MIDDLE  = 2,
    MOUSE_BUTTON_SIDE    = 3,
    MOUSE_BUTTON_EXTRA   = 4,
    MOUSE_BUTTON_FORWARD = 5,
    MOUSE_BUTTON_BACK    = 6,
} MouseButton;


typedef enum {
    MOUSE_CURSOR_DEFAULT       = 0,
    MOUSE_CURSOR_ARROW         = 1,
    MOUSE_CURSOR_IBEAM         = 2,
    MOUSE_CURSOR_CROSSHAIR     = 3,
    MOUSE_CURSOR_POINTING_HAND = 4,
    MOUSE_CURSOR_RESIZE_EW     = 5,
    MOUSE_CURSOR_RESIZE_NS     = 6,
    MOUSE_CURSOR_RESIZE_NWSE   = 7,
    MOUSE_CURSOR_RESIZE_NESW   = 8,
    MOUSE_CURSOR_RESIZE_ALL    = 9,
    MOUSE_CURSOR_NOT_ALLOWED   = 10
} MouseCursor;


typedef enum {
    GAMEPAD_BUTTON_UNKNOWN = 0,
    GAMEPAD_BUTTON_LEFT_FACE_UP,
    GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
    GAMEPAD_BUTTON_LEFT_FACE_DOWN,
    GAMEPAD_BUTTON_LEFT_FACE_LEFT,
    GAMEPAD_BUTTON_RIGHT_FACE_UP,
    GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
    GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
    GAMEPAD_BUTTON_LEFT_TRIGGER_1,
    GAMEPAD_BUTTON_LEFT_TRIGGER_2,
    GAMEPAD_BUTTON_RIGHT_TRIGGER_1,
    GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
    GAMEPAD_BUTTON_MIDDLE_LEFT,
    GAMEPAD_BUTTON_MIDDLE,
    GAMEPAD_BUTTON_MIDDLE_RIGHT,
    GAMEPAD_BUTTON_LEFT_THUMB,
    GAMEPAD_BUTTON_RIGHT_THUMB
} GamepadButton;


typedef enum {
    GAMEPAD_AXIS_LEFT_X        = 0,
    GAMEPAD_AXIS_LEFT_Y        = 1,
    GAMEPAD_AXIS_RIGHT_X       = 2,
    GAMEPAD_AXIS_RIGHT_Y       = 3,
    GAMEPAD_AXIS_LEFT_TRIGGER  = 4,
    GAMEPAD_AXIS_RIGHT_TRIGGER = 5
} GamepadAxis;


typedef enum {
    MATERIAL_MAP_ALBEDO = 0,
    MATERIAL_MAP_METALNESS,
    MATERIAL_MAP_NORMAL,
    MATERIAL_MAP_ROUGHNESS,
    MATERIAL_MAP_OCCLUSION,
    MATERIAL_MAP_EMISSION,
    MATERIAL_MAP_HEIGHT,
    MATERIAL_MAP_CUBEMAP,
    MATERIAL_MAP_IRRADIANCE,
    MATERIAL_MAP_PREFILTER,
    MATERIAL_MAP_BRDF
} MaterialMapIndex;







typedef enum {
    SHADER_LOC_VERTEX_POSITION = 0,
    SHADER_LOC_VERTEX_TEXCOORD01,
    SHADER_LOC_VERTEX_TEXCOORD02,
    SHADER_LOC_VERTEX_NORMAL,
    SHADER_LOC_VERTEX_TANGENT,
    SHADER_LOC_VERTEX_COLOR,
    SHADER_LOC_MATRIX_MVP,
    SHADER_LOC_MATRIX_VIEW,
    SHADER_LOC_MATRIX_PROJECTION,
    SHADER_LOC_MATRIX_MODEL,
    SHADER_LOC_MATRIX_NORMAL,
    SHADER_LOC_VECTOR_VIEW,
    SHADER_LOC_COLOR_DIFFUSE,
    SHADER_LOC_COLOR_SPECULAR,
    SHADER_LOC_COLOR_AMBIENT,
    SHADER_LOC_MAP_ALBEDO,
    SHADER_LOC_MAP_METALNESS,
    SHADER_LOC_MAP_NORMAL,
    SHADER_LOC_MAP_ROUGHNESS,
    SHADER_LOC_MAP_OCCLUSION,
    SHADER_LOC_MAP_EMISSION,
    SHADER_LOC_MAP_HEIGHT,
    SHADER_LOC_MAP_CUBEMAP,
    SHADER_LOC_MAP_IRRADIANCE,
    SHADER_LOC_MAP_PREFILTER,
    SHADER_LOC_MAP_BRDF,
    SHADER_LOC_VERTEX_BONEIDS,
    SHADER_LOC_VERTEX_BONEWEIGHTS,
    SHADER_LOC_MATRIX_BONETRANSFORMS,
    SHADER_LOC_VERTEX_INSTANCETRANSFORM
} ShaderLocationIndex;





typedef enum {
    SHADER_UNIFORM_FLOAT = 0,
    SHADER_UNIFORM_VEC2,
    SHADER_UNIFORM_VEC3,
    SHADER_UNIFORM_VEC4,
    SHADER_UNIFORM_INT,
    SHADER_UNIFORM_IVEC2,
    SHADER_UNIFORM_IVEC3,
    SHADER_UNIFORM_IVEC4,
    SHADER_UNIFORM_UINT,
    SHADER_UNIFORM_UIVEC2,
    SHADER_UNIFORM_UIVEC3,
    SHADER_UNIFORM_UIVEC4,
    SHADER_UNIFORM_SAMPLER2D
} ShaderUniformDataType;


typedef enum {
    SHADER_ATTRIB_FLOAT = 0,
    SHADER_ATTRIB_VEC2,
    SHADER_ATTRIB_VEC3,
    SHADER_ATTRIB_VEC4
} ShaderAttributeDataType;



typedef enum {
    PIXELFORMAT_UNCOMPRESSED_GRAYSCALE = 1,
    PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,
    PIXELFORMAT_UNCOMPRESSED_R5G6B5,
    PIXELFORMAT_UNCOMPRESSED_R8G8B8,
    PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,
    PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    PIXELFORMAT_UNCOMPRESSED_R32,
    PIXELFORMAT_UNCOMPRESSED_R32G32B32,
    PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
    PIXELFORMAT_UNCOMPRESSED_R16,
    PIXELFORMAT_UNCOMPRESSED_R16G16B16,
    PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,
    PIXELFORMAT_COMPRESSED_DXT1_RGB,
    PIXELFORMAT_COMPRESSED_DXT1_RGBA,
    PIXELFORMAT_COMPRESSED_DXT3_RGBA,
    PIXELFORMAT_COMPRESSED_DXT5_RGBA,
    PIXELFORMAT_COMPRESSED_ETC1_RGB,
    PIXELFORMAT_COMPRESSED_ETC2_RGB,
    PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA,
    PIXELFORMAT_COMPRESSED_PVRT_RGB,
    PIXELFORMAT_COMPRESSED_PVRT_RGBA,
    PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA,
    PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA
} PixelFormat;




typedef enum {
    TEXTURE_FILTER_POINT = 0,
    TEXTURE_FILTER_BILINEAR,
    TEXTURE_FILTER_TRILINEAR,
    TEXTURE_FILTER_ANISOTROPIC_4X,
    TEXTURE_FILTER_ANISOTROPIC_8X,
    TEXTURE_FILTER_ANISOTROPIC_16X,
} TextureFilter;


typedef enum {
    TEXTURE_WRAP_REPEAT = 0,
    TEXTURE_WRAP_CLAMP,
    TEXTURE_WRAP_MIRROR_REPEAT,
    TEXTURE_WRAP_MIRROR_CLAMP
} TextureWrap;


typedef enum {
    CUBEMAP_LAYOUT_AUTO_DETECT = 0,
    CUBEMAP_LAYOUT_LINE_VERTICAL,
    CUBEMAP_LAYOUT_LINE_HORIZONTAL,
    CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,
    CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE
} CubemapLayout;


typedef enum {
    FONT_DEFAULT = 0,
    FONT_BITMAP,
    FONT_SDF
} FontType;


typedef enum {
    BLEND_ALPHA = 0,
    BLEND_ADDITIVE,
    BLEND_MULTIPLIED,
    BLEND_ADD_COLORS,
    BLEND_SUBTRACT_COLORS,
    BLEND_ALPHA_PREMULTIPLY,
    BLEND_CUSTOM,
    BLEND_CUSTOM_SEPARATE
} BlendMode;



typedef enum {
    GESTURE_NONE        = 0,
    GESTURE_TAP         = 1,
    GESTURE_DOUBLETAP   = 2,
    GESTURE_HOLD        = 4,
    GESTURE_DRAG        = 8,
    GESTURE_SWIPE_RIGHT = 16,
    GESTURE_SWIPE_LEFT  = 32,
    GESTURE_SWIPE_UP    = 64,
    GESTURE_SWIPE_DOWN  = 128,
    GESTURE_PINCH_IN    = 256,
    GESTURE_PINCH_OUT   = 512
} Gesture;


typedef enum {
    CAMERA_CUSTOM = 0,
    CAMERA_FREE,
    CAMERA_ORBITAL,
    CAMERA_FIRST_PERSON,
    CAMERA_THIRD_PERSON
} CameraMode;


typedef enum {
    CAMERA_PERSPECTIVE = 0,
    CAMERA_ORTHOGRAPHIC
} CameraProjection;


typedef enum {
    NPATCH_NINE_PATCH = 0,
    NPATCH_THREE_PATCH_VERTICAL,
    NPATCH_THREE_PATCH_HORIZONTAL
} NPatchLayout;



typedef void (*TraceLogCallback)(int logLevel, const char *text, va_list args);
typedef unsigned char *(*LoadFileDataCallback)(const char *fileName, int *dataSize);
typedef bool (*SaveFileDataCallback)(const char *fileName, const void *data, int dataSize);
typedef char *(*LoadFileTextCallback)(const char *fileName);
typedef bool (*SaveFileTextCallback)(const char *fileName, const char *text);















 void InitWindow(int width, int height, const char *title);
 void CloseWindow(void);
 bool WindowShouldClose(void);
 bool IsWindowReady(void);
 bool IsWindowFullscreen(void);
 bool IsWindowHidden(void);
 bool IsWindowMinimized(void);
 bool IsWindowMaximized(void);
 bool IsWindowFocused(void);
 bool IsWindowResized(void);
 bool IsWindowState(unsigned int flag);
 void SetWindowState(unsigned int flags);
 void ClearWindowState(unsigned int flags);
 void ToggleFullscreen(void);
 void ToggleBorderlessWindowed(void);
 void MaximizeWindow(void);
 void MinimizeWindow(void);
 void RestoreWindow(void);
 void SetWindowIcon(Image image);
 void SetWindowIcons(Image *images, int count);
 void SetWindowTitle(const char *title);
 void SetWindowPosition(int x, int y);
 void SetWindowMonitor(int monitor);
 void SetWindowMinSize(int width, int height);
 void SetWindowMaxSize(int width, int height);
 void SetWindowSize(int width, int height);
 void SetWindowOpacity(float opacity);
 void SetWindowFocused(void);
 void *GetWindowHandle(void);
 int GetScreenWidth(void);
 int GetScreenHeight(void);
 int GetRenderWidth(void);
 int GetRenderHeight(void);
 int GetMonitorCount(void);
 int GetCurrentMonitor(void);
 Vector2 GetMonitorPosition(int monitor);
 int GetMonitorWidth(int monitor);
 int GetMonitorHeight(int monitor);
 int GetMonitorPhysicalWidth(int monitor);
 int GetMonitorPhysicalHeight(int monitor);
 int GetMonitorRefreshRate(int monitor);
 Vector2 GetWindowPosition(void);
 Vector2 GetWindowScaleDPI(void);
 const char *GetMonitorName(int monitor);
 void SetClipboardText(const char *text);
 const char *GetClipboardText(void);
 Image GetClipboardImage(void);
 void EnableEventWaiting(void);
 void DisableEventWaiting(void);


 void ShowCursor(void);
 void HideCursor(void);
 bool IsCursorHidden(void);
 void EnableCursor(void);
 void DisableCursor(void);
 bool IsCursorOnScreen(void);


 void ClearBackground(Color color);
 void BeginDrawing(void);
 void EndDrawing(void);
 void BeginMode2D(Camera2D camera);
 void EndMode2D(void);
 void BeginMode3D(Camera3D camera);
 void EndMode3D(void);
 void BeginTextureMode(RenderTexture2D target);
 void EndTextureMode(void);
 void BeginShaderMode(Shader shader);
 void EndShaderMode(void);
 void BeginBlendMode(int mode);
 void EndBlendMode(void);
 void BeginScissorMode(int x, int y, int width, int height);
 void EndScissorMode(void);
 void BeginVrStereoMode(VrStereoConfig config);
 void EndVrStereoMode(void);


 VrStereoConfig LoadVrStereoConfig(VrDeviceInfo device);
 void UnloadVrStereoConfig(VrStereoConfig config);



 Shader LoadShader(const char *vsFileName, const char *fsFileName);
 Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode);
 bool IsShaderValid(Shader shader);
 int GetShaderLocation(Shader shader, const char *uniformName);
 int GetShaderLocationAttrib(Shader shader, const char *attribName);
 void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType);
 void SetShaderValueV(Shader shader, int locIndex, const void *value, int uniformType, int count);
 void SetShaderValueMatrix(Shader shader, int locIndex, Matrix mat);
 void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture);
 void UnloadShader(Shader shader);


 Ray GetScreenToWorldRay(Vector2 position, Camera camera);
 Ray GetScreenToWorldRayEx(Vector2 position, Camera camera, int width, int height);
 Vector2 GetWorldToScreen(Vector3 position, Camera camera);
 Vector2 GetWorldToScreenEx(Vector3 position, Camera camera, int width, int height);
 Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera);
 Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera);
 Matrix GetCameraMatrix(Camera camera);
 Matrix GetCameraMatrix2D(Camera2D camera);


 void SetTargetFPS(int fps);
 float GetFrameTime(void);
 double GetTime(void);
 int GetFPS(void);





 void SwapScreenBuffer(void);
 void PollInputEvents(void);
 void WaitTime(double seconds);


 void SetRandomSeed(unsigned int seed);
 int GetRandomValue(int min, int max);
 int *LoadRandomSequence(unsigned int count, int min, int max);
 void UnloadRandomSequence(int *sequence);


 void TakeScreenshot(const char *fileName);
 void SetConfigFlags(unsigned int flags);
 void OpenURL(const char *url);


 void SetTraceLogLevel(int logLevel);
 void TraceLog(int logLevel, const char *text, ...);
 void SetTraceLogCallback(TraceLogCallback callback);


 void *MemAlloc(unsigned int size);
 void *MemRealloc(void *ptr, unsigned int size);
 void MemFree(void *ptr);


 unsigned char *LoadFileData(const char *fileName, int *dataSize);
 void UnloadFileData(unsigned char *data);
 bool SaveFileData(const char *fileName, const void *data, int dataSize);
 bool ExportDataAsCode(const unsigned char *data, int dataSize, const char *fileName);
 char *LoadFileText(const char *fileName);
 void UnloadFileText(char *text);
 bool SaveFileText(const char *fileName, const char *text);



 void SetLoadFileDataCallback(LoadFileDataCallback callback);
 void SetSaveFileDataCallback(SaveFileDataCallback callback);
 void SetLoadFileTextCallback(LoadFileTextCallback callback);
 void SetSaveFileTextCallback(SaveFileTextCallback callback);

 int FileRename(const char *fileName, const char *fileRename);
 int FileRemove(const char *fileName);
 int FileCopy(const char *srcPath, const char *dstPath);
 int FileMove(const char *srcPath, const char *dstPath);
 int FileTextReplace(const char *fileName, const char *search, const char *replacement);
 int FileTextFindIndex(const char *fileName, const char *search);
 bool FileExists(const char *fileName);
 bool DirectoryExists(const char *dirPath);
 bool IsFileExtension(const char *fileName, const char *ext);
 int GetFileLength(const char *fileName);
 long GetFileModTime(const char *fileName);
 const char *GetFileExtension(const char *fileName);
 const char *GetFileName(const char *filePath);
 const char *GetFileNameWithoutExt(const char *filePath);
 const char *GetDirectoryPath(const char *filePath);
 const char *GetPrevDirectoryPath(const char *dirPath);
 const char *GetWorkingDirectory(void);
 const char *GetApplicationDirectory(void);
 int MakeDirectory(const char *dirPath);
 int ChangeDirectory(const char *dirPath);
 bool IsPathFile(const char *path);
 bool IsPathDirectory(const char *path);
 bool IsFileNameValid(const char *fileName);
 FilePathList LoadDirectoryFiles(const char *dirPath);
 FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter, bool scanSubdirs);
 void UnloadDirectoryFiles(FilePathList files);
 bool IsFileDropped(void);
 FilePathList LoadDroppedFiles(void);
 void UnloadDroppedFiles(FilePathList files);
 unsigned int GetDirectoryFileCount(const char *dirPath);
 unsigned int GetDirectoryFileCountEx(const char *basePath, const char *filter, bool scanSubdirs);


 unsigned char *CompressData(const unsigned char *data, int dataSize, int *compDataSize);
 unsigned char *DecompressData(const unsigned char *compData, int compDataSize, int *dataSize);
 char *EncodeDataBase64(const unsigned char *data, int dataSize, int *outputSize);
 unsigned char *DecodeDataBase64(const char *text, int *outputSize);
 unsigned int ComputeCRC32(const unsigned char *data, int dataSize);
 unsigned int *ComputeMD5(const unsigned char *data, int dataSize);
 unsigned int *ComputeSHA1(const unsigned char *data, int dataSize);
 unsigned int *ComputeSHA256(const unsigned char *data, int dataSize);


 AutomationEventList LoadAutomationEventList(const char *fileName);
 void UnloadAutomationEventList(AutomationEventList list);
 bool ExportAutomationEventList(AutomationEventList list, const char *fileName);
 void SetAutomationEventList(AutomationEventList *list);
 void SetAutomationEventBaseFrame(int frame);
 void StartAutomationEventRecording(void);
 void StopAutomationEventRecording(void);
 void PlayAutomationEvent(AutomationEvent event);






 bool IsKeyPressed(int key);
 bool IsKeyPressedRepeat(int key);
 bool IsKeyDown(int key);
 bool IsKeyReleased(int key);
 bool IsKeyUp(int key);
 int GetKeyPressed(void);
 int GetCharPressed(void);
 const char *GetKeyName(int key);
 void SetExitKey(int key);


 bool IsGamepadAvailable(int gamepad);
 const char *GetGamepadName(int gamepad);
 bool IsGamepadButtonPressed(int gamepad, int button);
 bool IsGamepadButtonDown(int gamepad, int button);
 bool IsGamepadButtonReleased(int gamepad, int button);
 bool IsGamepadButtonUp(int gamepad, int button);
 int GetGamepadButtonPressed(void);
 int GetGamepadAxisCount(int gamepad);
 float GetGamepadAxisMovement(int gamepad, int axis);
 int SetGamepadMappings(const char *mappings);
 void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration);


 bool IsMouseButtonPressed(int button);
 bool IsMouseButtonDown(int button);
 bool IsMouseButtonReleased(int button);
 bool IsMouseButtonUp(int button);
 int GetMouseX(void);
 int GetMouseY(void);
 Vector2 GetMousePosition(void);
 Vector2 GetMouseDelta(void);
 void SetMousePosition(int x, int y);
 void SetMouseOffset(int offsetX, int offsetY);
 void SetMouseScale(float scaleX, float scaleY);
 float GetMouseWheelMove(void);
 Vector2 GetMouseWheelMoveV(void);
 void SetMouseCursor(int cursor);


 int GetTouchX(void);
 int GetTouchY(void);
 Vector2 GetTouchPosition(int index);
 int GetTouchPointId(int index);
 int GetTouchPointCount(void);




 void SetGesturesEnabled(unsigned int flags);
 bool IsGestureDetected(unsigned int gesture);
 int GetGestureDetected(void);
 float GetGestureHoldDuration(void);
 Vector2 GetGestureDragVector(void);
 float GetGestureDragAngle(void);
 Vector2 GetGesturePinchVector(void);
 float GetGesturePinchAngle(void);




 void UpdateCamera(Camera *camera, int mode);
 void UpdateCameraPro(Camera *camera, Vector3 movement, Vector3 rotation, float zoom);







 void SetShapesTexture(Texture2D texture, Rectangle rec);
 Texture2D GetShapesTexture(void);
 Rectangle GetShapesTextureRectangle(void);


 void DrawPixel(int posX, int posY, Color color);
 void DrawPixelV(Vector2 position, Color color);
 void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
 void DrawLineV(Vector2 startPos, Vector2 endPos, Color color);
 void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color);
 void DrawLineStrip(const Vector2 *points, int pointCount, Color color);
 void DrawLineBezier(Vector2 startPos, Vector2 endPos, float thick, Color color);
 void DrawLineDashed(Vector2 startPos, Vector2 endPos, int dashSize, int spaceSize, Color color);
 void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
 void DrawTriangleGradient(Vector2 v1, Vector2 v2, Vector2 v3, Color c1, Color c2, Color c3);
 void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
 void DrawTriangleFan(const Vector2 *points, int pointCount, Color color);
 void DrawTriangleStrip(const Vector2 *points, int pointCount, Color color);
 void DrawRectangle(int posX, int posY, int width, int height, Color color);
 void DrawRectangleV(Vector2 position, Vector2 size, Color color);
 void DrawRectangleRec(Rectangle rec, Color color);
 void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color);
 void DrawRectangleGradientV(int posX, int posY, int width, int height, Color top, Color bottom);
 void DrawRectangleGradientH(int posX, int posY, int width, int height, Color left, Color right);
 void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4);
 void DrawRectangleLines(int posX, int posY, int width, int height, Color color);
 void DrawRectangleLinesEx(Rectangle rec, float thick, Color color);
 void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color);
 void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, Color color);
 void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float thick, Color color);
 void DrawPoly(Vector2 center, int sides, float radius, float rotation, Color color);
 void DrawPolyLines(Vector2 center, int sides, float radius, float rotation, Color color);
 void DrawPolyLinesEx(Vector2 center, int sides, float radius, float rotation, float thick, Color color);
 void DrawCircle(int centerX, int centerY, float radius, Color color);
 void DrawCircleV(Vector2 center, float radius, Color color);
 void DrawCircleGradient(Vector2 center, float radius, Color inner, Color outer);
 void DrawCircleSector(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color);
 void DrawCircleSectorLines(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color);
 void DrawCircleLines(int centerX, int centerY, float radius, Color color);
 void DrawCircleLinesV(Vector2 center, float radius, Color color);
 void DrawCircleLinesEx(Vector2 center, float radius, float thick, Color color);
 void DrawEllipse(int centerX, int centerY, float radiusH, float radiusV, Color color);
 void DrawEllipseV(Vector2 center, float radiusH, float radiusV, Color color);
 void DrawEllipseLines(int centerX, int centerY, float radiusH, float radiusV, Color color);
 void DrawEllipseLinesV(Vector2 center, float radiusH, float radiusV, Color color);
 void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color);
 void DrawRingLines(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color);


 void DrawSplineLinear(const Vector2 *points, int pointCount, float thick, Color color);
 void DrawSplineBasis(const Vector2 *points, int pointCount, float thick, Color color);
 void DrawSplineCatmullRom(const Vector2 *points, int pointCount, float thick, Color color);
 void DrawSplineBezierQuadratic(const Vector2 *points, int pointCount, float thick, Color color);
 void DrawSplineBezierCubic(const Vector2 *points, int pointCount, float thick, Color color);
 void DrawSplineSegmentLinear(Vector2 p1, Vector2 p2, float thick, Color color);
 void DrawSplineSegmentBasis(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color);
 void DrawSplineSegmentCatmullRom(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color);
 void DrawSplineSegmentBezierQuadratic(Vector2 p1, Vector2 c2, Vector2 p3, float thick, Color color);
 void DrawSplineSegmentBezierCubic(Vector2 p1, Vector2 c2, Vector2 c3, Vector2 p4, float thick, Color color);


 Vector2 GetSplinePointLinear(Vector2 startPos, Vector2 endPos, float t);
 Vector2 GetSplinePointBasis(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float t);
 Vector2 GetSplinePointCatmullRom(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float t);
 Vector2 GetSplinePointBezierQuadratic(Vector2 p1, Vector2 c2, Vector2 p3, float t);
 Vector2 GetSplinePointBezierCubic(Vector2 p1, Vector2 c2, Vector2 c3, Vector2 p4, float t);


 bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2);
 bool CheckCollisionCircles(Vector2 center1, float radius1, Vector2 center2, float radius2);
 bool CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rec);
 bool CheckCollisionCircleLine(Vector2 center, float radius, Vector2 p1, Vector2 p2);
 bool CheckCollisionPointRec(Vector2 point, Rectangle rec);
 bool CheckCollisionPointCircle(Vector2 point, Vector2 center, float radius);
 bool CheckCollisionPointTriangle(Vector2 point, Vector2 p1, Vector2 p2, Vector2 p3);
 bool CheckCollisionPointLine(Vector2 point, Vector2 p1, Vector2 p2, int threshold);
 bool CheckCollisionPointPoly(Vector2 point, const Vector2 *points, int pointCount);
 bool CheckCollisionLines(Vector2 startPos1, Vector2 endPos1, Vector2 startPos2, Vector2 endPos2, Vector2 *collisionPoint);
 Rectangle GetCollisionRec(Rectangle rec1, Rectangle rec2);







 Image LoadImage(const char *fileName);
 Image LoadImageRaw(const char *fileName, int width, int height, int format, int headerSize);
 Image LoadImageAnim(const char *fileName, int *frames);
 Image LoadImageAnimFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int *frames);
 Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize);
 Image LoadImageFromTexture(Texture2D texture);
 Image LoadImageFromScreen(void);
 bool IsImageValid(Image image);
 void UnloadImage(Image image);
 bool ExportImage(Image image, const char *fileName);
 unsigned char *ExportImageToMemory(Image image, const char *fileType, int *fileSize);
 bool ExportImageAsCode(Image image, const char *fileName);


 Image GenImageColor(int width, int height, Color color);
 Image GenImageGradientLinear(int width, int height, int direction, Color start, Color end);
 Image GenImageGradientRadial(int width, int height, float density, Color inner, Color outer);
 Image GenImageGradientSquare(int width, int height, float density, Color inner, Color outer);
 Image GenImageChecked(int width, int height, int checksX, int checksY, Color col1, Color col2);
 Image GenImageWhiteNoise(int width, int height, float factor);
 Image GenImagePerlinNoise(int width, int height, int offsetX, int offsetY, float scale);
 Image GenImageCellular(int width, int height, int tileSize);
 Image GenImageText(int width, int height, const char *text);


 Image ImageCopy(Image image);
 Image ImageFromImage(Image image, Rectangle rec);
 Image ImageFromChannel(Image image, int selectedChannel);
 Image ImageText(const char *text, int fontSize, Color color);
 Image ImageTextEx(Font font, const char *text, float fontSize, float spacing, Color tint);
 void ImageFormat(Image *image, int newFormat);
 void ImageToPOT(Image *image, Color fill);
 void ImageCrop(Image *image, Rectangle crop);
 void ImageAlphaCrop(Image *image, float threshold);
 void ImageAlphaClear(Image *image, Color color, float threshold);
 void ImageAlphaMask(Image *image, Image alphaMask);
 void ImageAlphaPremultiply(Image *image);
 void ImageBlurGaussian(Image *image, int blurSize);
 void ImageKernelConvolution(Image *image, const float *kernel, int kernelSize);
 void ImageResize(Image *image, int newWidth, int newHeight);
 void ImageResizeNN(Image *image, int newWidth, int newHeight);
 void ImageResizeCanvas(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill);
 void ImageMipmaps(Image *image);
 void ImageDither(Image *image, int rBpp, int gBpp, int bBpp, int aBpp);
 void ImageFlipVertical(Image *image);
 void ImageFlipHorizontal(Image *image);
 void ImageRotate(Image *image, int degrees);
 void ImageRotateCW(Image *image);
 void ImageRotateCCW(Image *image);
 void ImageColorTint(Image *image, Color color);
 void ImageColorInvert(Image *image);
 void ImageColorGrayscale(Image *image);
 void ImageColorContrast(Image *image, int contrast);
 void ImageColorBrightness(Image *image, int brightness);
 void ImageColorReplace(Image *image, Color color, Color replace);
 Color *LoadImageColors(Image image);
 Color *LoadImagePalette(Image image, int maxPaletteSize, int *colorCount);
 void UnloadImageColors(Color *colors);
 void UnloadImagePalette(Color *colors);
 Rectangle GetImageAlphaBorder(Image image, float threshold);
 Color GetImageColor(Image image, int x, int y);



 void ImageClearBackground(Image *dst, Color color);
 void ImageDrawPixel(Image *dst, int posX, int posY, Color color);
 void ImageDrawPixelV(Image *dst, Vector2 position, Color color);
 void ImageDrawLine(Image *dst, int startPosX, int startPosY, int endPosX, int endPosY, Color color);
 void ImageDrawLineV(Image *dst, Vector2 start, Vector2 end, Color color);
 void ImageDrawLineEx(Image *dst, Vector2 start, Vector2 end, int thick, Color color);
 void ImageDrawLineStrip(Image *dst, const Vector2 *points, int pointCount, Color color);
 void ImageDrawTriangle(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color color);
 void ImageDrawTriangleGradient(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color c1, Color c2, Color c3);
 void ImageDrawTriangleLines(Image *dst, Vector2 v1, Vector2 v2, Vector2 v3, Color color);
 void ImageDrawTriangleFan(Image *dst, const Vector2 *points, int pointCount, Color color);
 void ImageDrawTriangleStrip(Image *dst, const Vector2 *points, int pointCount, Color color);
 void ImageDrawRectangle(Image *dst, int posX, int posY, int width, int height, Color color);
 void ImageDrawRectangleV(Image *dst, Vector2 position, Vector2 size, Color color);
 void ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color);
 void ImageDrawRectanglePro(Image *dst, Rectangle rec, Vector2 origin, float rotation, Color color);
 void ImageDrawRectangleLines(Image *dst, int posX, int posY, int width, int height, Color color);
 void ImageDrawRectangleLinesEx(Image *dst, Rectangle rec, int thick, Color color);
 void ImageDrawRectangleGradientEx(Image *dst, Rectangle rec, Color col1, Color col2, Color col3, Color col4);
 void ImageDrawCircle(Image *dst, int centerX, int centerY, int radius, Color color);
 void ImageDrawCircleV(Image *dst, Vector2 center, int radius, Color color);
 void ImageDrawCircleLines(Image *dst, int centerX, int centerY, int radius, Color color);
 void ImageDrawCircleLinesV(Image *dst, Vector2 center, int radius, Color color);
 void ImageDrawCircleGradient(Image *dst, Vector2 center, float radius, Color inner, Color outer);

 void ImageDrawImage(Image *dst, Image src, int posX, int posY, Color tint);
 void ImageDrawImageEx(Image *dst, Image src, Vector2 position, float rotation, float scale, Color tint);
 void ImageDrawImageRec(Image *dst, Image src, Rectangle srcRec, Vector2 position, Color tint);
 void ImageDrawImagePro(Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Vector2 origin, float rotation, Color tint);
 void ImageDrawText(Image *dst, const char *text, int posX, int posY, int fontSize, Color color);
 void ImageDrawTextEx(Image *dst, Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint);
 void ImageDrawTextPro(Image *dst, Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint);



 Texture2D LoadTexture(const char *fileName);
 Texture2D LoadTextureFromImage(Image image);
 TextureCubemap LoadTextureCubemap(Image image, int layout);
 RenderTexture2D LoadRenderTexture(int width, int height);
 bool IsTextureValid(Texture2D texture);
 void UnloadTexture(Texture2D texture);
 bool IsRenderTextureValid(RenderTexture2D target);
 void UnloadRenderTexture(RenderTexture2D target);
 void UpdateTexture(Texture2D texture, const void *pixels);
 void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels);


 void GenTextureMipmaps(Texture2D *texture);
 void SetTextureFilter(Texture2D texture, int filter);
 void SetTextureWrap(Texture2D texture, int wrap);


 void DrawTexture(Texture2D texture, int posX, int posY, Color tint);
 void DrawTextureV(Texture2D texture, Vector2 position, Color tint);
 void DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint);
 void DrawTextureRec(Texture2D texture, Rectangle rec, Vector2 position, Color tint);
 void DrawTexturePro(Texture2D texture, Rectangle srcrec, Rectangle dstrec, Vector2 origin, float rotation, Color tint);
 void DrawTextureNPatch(Texture2D texture, NPatchInfo nPatchInfo, Rectangle dstrec, Vector2 origin, float rotation, Color tint);


 bool ColorIsEqual(Color col1, Color col2);
 Color Fade(Color color, float alpha);
 int ColorToInt(Color color);
 Vector4 ColorNormalize(Color color);
 Color ColorFromNormalized(Vector4 normalized);
 Vector3 ColorToHSV(Color color);
 Color ColorFromHSV(float hue, float saturation, float value);
 Color ColorTint(Color color, Color tint);
 Color ColorBrightness(Color color, float factor);
 Color ColorContrast(Color color, float contrast);
 Color ColorAlpha(Color color, float alpha);
 Color ColorAlphaBlend(Color dst, Color src, Color tint);
 Color ColorLerp(Color color1, Color color2, float factor);
 Color GetColor(unsigned int hexValue);
 Color GetPixelColor(const void *srcPtr, int format);
 void SetPixelColor(void *dstPtr, Color color, int format);
 int GetPixelDataSize(int width, int height, int format);






 Font GetFontDefault(void);
 Font LoadFont(const char *fileName);
 Font LoadFontEx(const char *fileName, int fontSize, const int *codepoints, int codepointCount);
 Font LoadFontFromImage(Image image, Color key, int firstChar);
 Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int fontSize, const int *codepoints, int codepointCount);
 bool IsFontValid(Font font);
 GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize, int fontSize, const int *codepoints, int codepointCount, int type, int *glyphCount);
 Image GenImageFontAtlas(const GlyphInfo *glyphs, Rectangle **glyphRecs, int glyphCount, int fontSize, int padding, int packMethod);
 void UnloadFontData(GlyphInfo *glyphs, int glyphCount);
 void UnloadFont(Font font);
 bool ExportFontAsCode(Font font, const char *fileName);


 void DrawFPS(int posX, int posY);
 void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
 void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint);
 void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint);
 void DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize, Color tint);
 void DrawTextCodepoints(Font font, const int *codepoints, int codepointCount, Vector2 position, float fontSize, float spacing, Color tint);


 void SetTextLineSpacing(int spacing);
 int MeasureText(const char *text, int fontSize);
 Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing);
 Vector2 MeasureTextCodepoints(Font font, const int *codepoints, int length, float fontSize, float spacing);
 int GetGlyphIndex(Font font, int codepoint);
 GlyphInfo GetGlyphInfo(Font font, int codepoint);
 Rectangle GetGlyphAtlasRec(Font font, int codepoint);


 char *LoadUTF8(const int *codepoints, int length);
 void UnloadUTF8(char *text);
 int *LoadCodepoints(const char *text, int *count);
 void UnloadCodepoints(int *codepoints);
 int GetCodepointCount(const char *text);
 int GetCodepoint(const char *text, int *codepointSize);
 int GetCodepointNext(const char *text, int *codepointSize);
 int GetCodepointPrevious(const char *text, int *codepointSize);
 const char *CodepointToUTF8(int codepoint, int *utf8Size);




 char **LoadTextLines(const char *text, int *count);
 void UnloadTextLines(char **text, int lineCount);
 int TextCopy(char *dst, const char *src);
 bool TextIsEqual(const char *text1, const char *text2);
 unsigned int TextLength(const char *text);
 const char *TextFormat(const char *text, ...);
 const char *TextSubtext(const char *text, int position, int length);
 const char *TextRemoveSpaces(const char *text);
 char *GetTextBetween(const char *text, const char *begin, const char *end);
 char *TextReplace(const char *text, const char *search, const char *replacement);
 char *TextReplaceAlloc(const char *text, const char *search, const char *replacement);
 char *TextReplaceBetween(const char *text, const char *begin, const char *end, const char *replacement);
 char *TextReplaceBetweenAlloc(const char *text, const char *begin, const char *end, const char *replacement);
 char *TextInsert(const char *text, const char *insert, int position);
 char *TextInsertAlloc(const char *text, const char *insert, int position);
 char *TextJoin(char **textList, int count, const char *delimiter);
 char **TextSplit(const char *text, char delimiter, int *count);
 void TextAppend(char *text, const char *append, int *position);
 int TextFindIndex(const char *text, const char *search);
 char *TextToUpper(const char *text);
 char *TextToLower(const char *text);
 char *TextToPascal(const char *text);
 char *TextToSnake(const char *text);
 char *TextToCamel(const char *text);
 int TextToInteger(const char *text);
 float TextToFloat(const char *text);






 void DrawLine3D(Vector3 startPos, Vector3 endPos, Color color);
 void DrawPoint3D(Vector3 position, Color color);
 void DrawCircle3D(Vector3 center, float radius, Vector3 rotationAxis, float rotationAngle, Color color);
 void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color);
 void DrawTriangleStrip3D(const Vector3 *points, int pointCount, Color color);
 void DrawCube(Vector3 position, float width, float height, float length, Color color);
 void DrawCubeV(Vector3 position, Vector3 size, Color color);
 void DrawCubeWires(Vector3 position, float width, float height, float length, Color color);
 void DrawCubeWiresV(Vector3 position, Vector3 size, Color color);
 void DrawSphere(Vector3 centerPos, float radius, Color color);
 void DrawSphereEx(Vector3 centerPos, float radius, int rings, int slices, Color color);
 void DrawSphereWires(Vector3 centerPos, float radius, int rings, int slices, Color color);
 void DrawCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, int sides, Color color);
 void DrawCylinderEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color);
 void DrawCylinderWires(Vector3 position, float radiusTop, float radiusBottom, float height, int sides, Color color);
 void DrawCylinderWiresEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color);
 void DrawCapsule(Vector3 startPos, Vector3 endPos, float radius, int rings, int slices, Color color);
 void DrawCapsuleWires(Vector3 startPos, Vector3 endPos, float radius, int rings, int slices, Color color);
 void DrawPlane(Vector3 centerPos, Vector2 size, Color color);
 void DrawRay(Ray ray, Color color);
 void DrawGrid(int slices, float spacing);






 Model LoadModel(const char *fileName);
 Model LoadModelFromMesh(Mesh mesh);
 bool IsModelValid(Model model);
 void UnloadModel(Model model);
 BoundingBox GetModelBoundingBox(Model model);


 void DrawModel(Model model, Vector3 position, float scale, Color tint);
 void DrawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);
 void DrawModelWires(Model model, Vector3 position, float scale, Color tint);
 void DrawModelWiresEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);
 void DrawBoundingBox(BoundingBox box, Color color);
 void DrawBillboard(Camera camera, Texture2D texture, Vector3 position, float scale, Color tint);
 void DrawBillboardRec(Camera camera, Texture2D texture, Rectangle rec, Vector3 position, Vector2 size, Color tint);
 void DrawBillboardPro(Camera camera, Texture2D texture, Rectangle rec, Vector3 position, Vector3 up, Vector2 size, Vector2 origin, float rotation, Color tint);


 void UploadMesh(Mesh *mesh, bool dynamic);
 void UpdateMeshBuffer(Mesh mesh, int index, const void *data, int dataSize, int offset);
 void UnloadMesh(Mesh mesh);
 void DrawMesh(Mesh mesh, Material material, Matrix transform);
 void DrawMeshInstanced(Mesh mesh, Material material, const Matrix *transforms, int instances);
 BoundingBox GetMeshBoundingBox(Mesh mesh);
 void GenMeshTangents(Mesh *mesh);
 bool ExportMesh(Mesh mesh, const char *fileName);
 bool ExportMeshAsCode(Mesh mesh, const char *fileName);


 Mesh GenMeshPoly(int sides, float radius);
 Mesh GenMeshPlane(float width, float length, int resX, int resZ);
 Mesh GenMeshCube(float width, float height, float length);
 Mesh GenMeshSphere(float radius, int rings, int slices);
 Mesh GenMeshHemiSphere(float radius, int rings, int slices);
 Mesh GenMeshCylinder(float radius, float height, int slices);
 Mesh GenMeshCone(float radius, float height, int slices);
 Mesh GenMeshTorus(float radius, float size, int radSeg, int sides);
 Mesh GenMeshKnot(float radius, float size, int radSeg, int sides);
 Mesh GenMeshHeightmap(Image heightmap, Vector3 size);
 Mesh GenMeshCubicmap(Image cubicmap, Vector3 cubeSize);


 Material *LoadMaterials(const char *fileName, int *materialCount);
 Material LoadMaterialDefault(void);
 bool IsMaterialValid(Material material);
 void UnloadMaterial(Material material);
 void SetMaterialTexture(Material *material, int mapType, Texture2D texture);
 void SetModelMeshMaterial(Model *model, int meshId, int materialId);


 ModelAnimation *LoadModelAnimations(const char *fileName, int *animCount);
 void UpdateModelAnimation(Model model, ModelAnimation anim, float frame);
 void UpdateModelAnimationEx(Model model, ModelAnimation animA, float frameA, ModelAnimation animB, float frameB, float blend);
 void UnloadModelAnimations(ModelAnimation *animations, int animCount);
 bool IsModelAnimationValid(Model model, ModelAnimation anim);


 bool CheckCollisionSpheres(Vector3 center1, float radius1, Vector3 center2, float radius2);
 bool CheckCollisionBoxes(BoundingBox box1, BoundingBox box2);
 bool CheckCollisionBoxSphere(BoundingBox box, Vector3 center, float radius);
 RayCollision GetRayCollisionSphere(Ray ray, Vector3 center, float radius);
 RayCollision GetRayCollisionBox(Ray ray, BoundingBox box);
 RayCollision GetRayCollisionMesh(Ray ray, Mesh mesh, Matrix transform);
 RayCollision GetRayCollisionTriangle(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3);
 RayCollision GetRayCollisionQuad(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4);




typedef void (*AudioCallback)(void *bufferData, unsigned int frames);


 void InitAudioDevice(void);
 void CloseAudioDevice(void);
 bool IsAudioDeviceReady(void);
 void SetMasterVolume(float volume);
 float GetMasterVolume(void);


 Wave LoadWave(const char *fileName);
 Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData, int dataSize);
 bool IsWaveValid(Wave wave);
 Sound LoadSound(const char *fileName);
 Sound LoadSoundFromWave(Wave wave);
 Sound LoadSoundAlias(Sound source);
 bool IsSoundValid(Sound sound);
 void UpdateSound(Sound sound, const void *data, int frameCount);
 void UnloadWave(Wave wave);
 void UnloadSound(Sound sound);
 void UnloadSoundAlias(Sound alias);
 bool ExportWave(Wave wave, const char *fileName);
 bool ExportWaveAsCode(Wave wave, const char *fileName);


 void PlaySound(Sound sound);
 void StopSound(Sound sound);
 void PauseSound(Sound sound);
 void ResumeSound(Sound sound);
 bool IsSoundPlaying(Sound sound);
 void SetSoundVolume(Sound sound, float volume);
 void SetSoundPitch(Sound sound, float pitch);
 void SetSoundPan(Sound sound, float pan);
 Wave WaveCopy(Wave wave);
 void WaveCrop(Wave *wave, int initFrame, int finalFrame);
 void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels);
 float *LoadWaveSamples(Wave wave);
 void UnloadWaveSamples(float *samples);


 Music LoadMusicStream(const char *fileName);
 Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data, int dataSize);
 bool IsMusicValid(Music music);
 void UnloadMusicStream(Music music);
 void PlayMusicStream(Music music);
 bool IsMusicStreamPlaying(Music music);
 void UpdateMusicStream(Music music);
 void StopMusicStream(Music music);
 void PauseMusicStream(Music music);
 void ResumeMusicStream(Music music);
 void SeekMusicStream(Music music, float position);
 void SetMusicVolume(Music music, float volume);
 void SetMusicPitch(Music music, float pitch);
 void SetMusicPan(Music music, float pan);
 float GetMusicTimeLength(Music music);
 float GetMusicTimePlayed(Music music);


 AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels);
 bool IsAudioStreamValid(AudioStream stream);
 void UnloadAudioStream(AudioStream stream);
 void UpdateAudioStream(AudioStream stream, const void *data, int frameCount);
 bool IsAudioStreamProcessed(AudioStream stream);
 void PlayAudioStream(AudioStream stream);
 void PauseAudioStream(AudioStream stream);
 void ResumeAudioStream(AudioStream stream);
 bool IsAudioStreamPlaying(AudioStream stream);
 void StopAudioStream(AudioStream stream);
 void SetAudioStreamVolume(AudioStream stream, float volume);
 void SetAudioStreamPitch(AudioStream stream, float pitch);
 void SetAudioStreamPan(AudioStream stream, float pan);
 void SetAudioStreamBufferSizeDefault(int size);
 void SetAudioStreamCallback(AudioStream stream, AudioCallback callback);

 void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor);
 void DetachAudioStreamProcessor(AudioStream stream, AudioCallback processor);

 void AttachAudioMixedProcessor(AudioCallback processor);
 void DetachAudioMixedProcessor(AudioCallback processor);






#line 8 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/kryon_frame.h"



void BeginFrame(void);
void EndFrame(void);


#line 9 "/sys/src/kryon/include/kryon.h"

#line 1 "/sys/src/kryon/include/ui_color.h"



#line 1 "/sys/src/kryon/include/kryon.h"





#line 7 "/sys/src/kryon/include/kryon.h"

























































#line 65 "/sys/src/kryon/include/kryon.h"











#line 81 "/sys/src/kryon/include/kryon.h"




#line 5 "/sys/src/kryon/include/ui_color.h"


Color LightenUIColor(Color c, int amount);


Color DarkenUIColor(Color c, int amount);


#line 11 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_scaling.h"



#line 1 "/sys/src/kryon/src/platform/plan9/include/stdint.h"




#line 1 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"

#line 13 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"




#line 18 "/sys/src/kryon/src/platform/plan9/include/kryon_plan9_libc.h"


















#line 6 "/sys/src/kryon/src/platform/plan9/include/stdint.h"

typedef schar int8_t;
typedef uchar uint8_t;
typedef short int16_t;
typedef ushort uint16_t;
typedef int int32_t;
typedef uint uint32_t;
typedef vlong int64_t;
typedef uvlong uint64_t;
typedef long intptr_t;
typedef ulong uintptr_t;
typedef vlong intmax_t;
typedef uvlong uintmax_t;


#line 5 "/sys/src/kryon/include/ui_scaling.h"


void SetUIScale(float scale);


float GetUIScale(void);


int ScaleUIPx(int px);


int ClampUIPx(int px, int min_px, int max_px);


#line 12 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_dpi.h"



#line 1 "/sys/src/kryon/include/kryon.h"





#line 7 "/sys/src/kryon/include/kryon.h"

























































#line 65 "/sys/src/kryon/include/kryon.h"











#line 81 "/sys/src/kryon/include/kryon.h"




#line 5 "/sys/src/kryon/include/ui_dpi.h"




typedef struct UIDPIState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} UIDPIState;

extern UIDPIState ui_dpi_state;

void InitUIDPI(void);
void FixUIDPIFramebufferColor(void);
void InvalidateUIDPI(void);
void SetUIDeviceDensity(float density);
void UpdateUIDPI(int view_width, int view_height);
int IsUIDPIDirty(void);
static inline float GetUIDPIScale(void) { return ui_dpi_state.ui_scale_clamped; }
static inline int GetUIDPIViewWidth(void) { return ui_dpi_state.view_width; }
static inline int GetUIDPIViewHeight(void) { return ui_dpi_state.view_height; }
static inline float GetUIDPICameraZoom(void) { return ui_dpi_state.camera_zoom; }


#line 13 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_layout.h"



#line 1 "/sys/src/kryon/src/platform/plan9/include/stdint.h"




















#line 5 "/sys/src/kryon/include/ui_layout.h"


void SetUIViewSize(int width, int height);


int GetUIViewWidth(void);


int GetUIViewHeight(void);



void GetUICenteredColumn(int max_w, int side_pad, int *x, int *w);


int GetUIPageSidePadding(void);


#line 14 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_clip.h"



#line 1 "/sys/src/kryon/include/kryon.h"





#line 7 "/sys/src/kryon/include/kryon.h"

























































#line 65 "/sys/src/kryon/include/kryon.h"











#line 81 "/sys/src/kryon/include/kryon.h"




#line 5 "/sys/src/kryon/include/ui_clip.h"

Rectangle GetUIClipIntersection(Rectangle a, Rectangle b);
Rectangle GetUIClipEffective(Rectangle bounds);
void BeginUIClip(int x, int y, int w, int h);
void EndUIClip(void);
void ResetUIClip(void);


#line 15 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_core.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_core.h"

typedef void (*TextInputPlatformCallback)(int active);

typedef struct UIFrameState {
    int view_width;
    int view_height;
    Camera2D camera;
    int input_clip_count;
    Rectangle input_clips[16];
    int input_capture_count;
    struct {
        Rectangle bounds;
        int allow_inside;
    } input_captures[16];
    int cursor_priority;
    int cursor_had_intent;
    int pointer_down;
    int pointer_dragging;
    int pointer_dragged_this_click;
    int pointer_start_x;
    int pointer_start_y;
    int pointer_owner;
    int release_consumed;
    int focus_active_id;
    int focus_ids[256];
    int focus_count;
    int focus_tab_dir;
    int focus_frame_open;
    int focus_text_input_active;
    int text_input_requested;
    int mouse_world_override_enabled;
    Vector2 mouse_world_override;
    unsigned long frame_serial;
    float ui_scale;
} UIFrameState;

void InitUI(int width, int height, float dpi);
void SetUIDefaultFontAutoLoad(int enabled);
void SetUILinkColor(Color link);
void ApplyCurrentUITheme(void);
int IsUIDesktopMode(void);
Camera2D GetUIDefaultCamera(void);
void BeginUIFrame(int width, int height, float dpi);
void EndUIFrame(void);
void SetUIFrame(Camera2D camera);
UIFrameState SaveUIFrameState(void);
void RestoreUIFrameState(UIFrameState state);
void SetUIMouseWorldOverride(int enabled, Vector2 position);
int SetUIKeyboardInputEnabled(int enabled);
int UIKeyboardInputEnabled(void);

void ClearUIInputCaptures(void);
void PushUIInputCapture(Rectangle bounds, int allow_inside);
void BeginUIModalLayer(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
void SetUIModalCapture(Rectangle bounds);

void SetTextInputPlatformCallback(TextInputPlatformCallback callback);
void SetUICursorClickable(int *cursor_clickable);
void SetUICursorDisabled(int *cursor_disabled);
void MarkUICursor(int cursor);
void MarkUIClickable(void);
void MarkUIDisabled(void);

int GetUIMouseCursor(void);
void SetUIIcons(Texture2D gear_icon, Texture2D x_icon);

int UIHandleClick(Rectangle bounds, int disabled, int *hover);
int UIInputCapturesClick(Vector2 point);
int UIReleaseConsumed(void);
void UIConsumeRelease(void);
int UIPointerReleaseConsumed(void);
void UIConsumePointerRelease(void);
int UIPointerReleaseAvailable(Vector2 point);
int UIPointerReleaseOutside(Rectangle bounds);
int UIHoverEffectsEnabled(void);
void SetUITransitionCuesEnabled(int enabled);
int UITransitionCuesEnabled(void);

void BeginUIFocus(void);
void EndUIFocus(void);
int UIFocusFrameOpen(void);
int RegisterUIFocus(int id, Rectangle bounds);
int IsUIFocusActive(int id);
int IsUIFocusActivatePressed(int id);
void SetUIFocus(int id);
int GetUIFocus(void);
void ClearUIFocus(void);
void SetUIFocusTextInputActive(int active);

extern int ui_view_height;
extern int ui_view_width;


#line 16 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_controls.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_controls.h"
#line 1 "/sys/src/kryon/include/theme_style.h"



typedef enum ThemeStyle {
    THEME_STYLE_SYSTEM = 0,
    THEME_STYLE_RETRO,
    THEME_STYLE_MATERIAL
} ThemeStyle;


#line 6 "/sys/src/kryon/include/ui_controls.h"
#line 1 "/sys/src/kryon/include/ui_icon_types.h"




typedef enum {
    UI_ICON_TYPE_NONE = 0,

    UI_ICON_TYPE_ACTIVITY,
    UI_ICON_TYPE_AMEN,
    UI_ICON_TYPE_APP,
    UI_ICON_TYPE_BACKWARD,
    UI_ICON_TYPE_C,
    UI_ICON_TYPE_CALENDAR,
    UI_ICON_TYPE_CHECK,
    UI_ICON_TYPE_EDIT,
    UI_ICON_TYPE_FINGERPRINT,
    UI_ICON_TYPE_FORWARD,
    UI_ICON_TYPE_GEAR,
    UI_ICON_TYPE_GLOBE,
    UI_ICON_TYPE_HOME,
    UI_ICON_TYPE_JUPITER,
    UI_ICON_TYPE_KRYON,
    UI_ICON_TYPE_LEADERCAT,
    UI_ICON_TYPE_LEFT,
    UI_ICON_TYPE_LIGHTOFF,
    UI_ICON_TYPE_LIGHTON,
    UI_ICON_TYPE_LINK,
    UI_ICON_TYPE_MANUAL,
    UI_ICON_TYPE_MARS,
    UI_ICON_TYPE_MERCURY,
    UI_ICON_TYPE_MOON,
    UI_ICON_TYPE_MUSIC,
    UI_ICON_TYPE_MUTE,
    UI_ICON_TYPE_PAUSE,
    UI_ICON_TYPE_PENCIL,
    UI_ICON_TYPE_PET,
    UI_ICON_TYPE_PLAY,
    UI_ICON_TYPE_PLUS,
    UI_ICON_TYPE_PROFILE,
    UI_ICON_TYPE_QUEST,
    UI_ICON_TYPE_RETURN,
    UI_ICON_TYPE_RIGHT,
    UI_ICON_TYPE_ROCKET,
    UI_ICON_TYPE_ROUTINE,
    UI_ICON_TYPE_SATURN,
    UI_ICON_TYPE_SAVE,
    UI_ICON_TYPE_SOUND,
    UI_ICON_TYPE_SOUND0,
    UI_ICON_TYPE_SOUND1,
    UI_ICON_TYPE_SOUND2,
    UI_ICON_TYPE_SOUND3,
    UI_ICON_TYPE_STACK,
    UI_ICON_TYPE_STAT,
    UI_ICON_TYPE_SUN,
    UI_ICON_TYPE_TEXT,
    UI_ICON_TYPE_TIMELINE,
    UI_ICON_TYPE_TODOS,
    UI_ICON_TYPE_TRASH,
    UI_ICON_TYPE_VENUS,
    UI_ICON_TYPE_WAO,
    UI_ICON_TYPE_WEEKLY,
    UI_ICON_TYPE_WRENCH,
    UI_ICON_TYPE_X,
    UI_ICON_TYPE_LANGUAGE_RAY,
    UI_ICON_TYPE_LANGUAGE_TCL,
    UI_ICON_TYPE_LANGUAGE_UXN,
    UI_ICON_TYPE_LANGUAGE_WASM,
    UI_ICON_TYPE_LANGUAGE_WASM4,
    UI_ICON_TYPE_PAYMENTS_BTC,
    UI_ICON_TYPE_PAYMENTS_MONERO,
    UI_ICON_TYPE_PAYMENTS_STRIPE,
    UI_ICON_TYPE_PFP_BAMBUS,
    UI_ICON_TYPE_PFP_BIRD,
    UI_ICON_TYPE_PFP_BOWL,
    UI_ICON_TYPE_PFP_BUSH,
    UI_ICON_TYPE_PFP_BUTTERFLY,
    UI_ICON_TYPE_PFP_CACTUS,
    UI_ICON_TYPE_PFP_COFFEE,
    UI_ICON_TYPE_PFP_DRAGONFLY,
    UI_ICON_TYPE_PFP_FIREPLACE,
    UI_ICON_TYPE_PFP_FLOWER1,
    UI_ICON_TYPE_PFP_FLOWER2,
    UI_ICON_TYPE_PFP_FOX,
    UI_ICON_TYPE_PFP_HEART,
    UI_ICON_TYPE_PFP_INCENSE,
    UI_ICON_TYPE_PFP_LOTUS,
    UI_ICON_TYPE_PFP_MOUNTAIN,
    UI_ICON_TYPE_PFP_MUSHROOM,
    UI_ICON_TYPE_PFP_PALM,
    UI_ICON_TYPE_PFP_PERSON1,
    UI_ICON_TYPE_PFP_RAINBOW,
    UI_ICON_TYPE_PFP_TENT,
    UI_ICON_TYPE_PFP_TREE1,
    UI_ICON_TYPE_PFP_TREE2,
    UI_ICON_TYPE_PFP_TREE3,
    UI_ICON_TYPE_PFP_TREE4,
    UI_ICON_TYPE_PLATFORMS_BROWSER,
    UI_ICON_TYPE_PLATFORMS_DISCORD,
    UI_ICON_TYPE_PLATFORMS_DROID,
    UI_ICON_TYPE_PLATFORMS_ESP32,
    UI_ICON_TYPE_PLATFORMS_FDROID,
    UI_ICON_TYPE_PLATFORMS_FREEBSD,
    UI_ICON_TYPE_PLATFORMS_GITHUB,
    UI_ICON_TYPE_PLATFORMS_GLENDA,
    UI_ICON_TYPE_PLATFORMS_IOS,
    UI_ICON_TYPE_PLATFORMS_ITCH,
    UI_ICON_TYPE_PLATFORMS_MACOS,
    UI_ICON_TYPE_PLATFORMS_MICROCONTROLLER,
    UI_ICON_TYPE_PLATFORMS_PLAYSTORE,
    UI_ICON_TYPE_PLATFORMS_SRHT,
    UI_ICON_TYPE_PLATFORMS_TELEGRAM,
    UI_ICON_TYPE_PLATFORMS_TUX,
    UI_ICON_TYPE_PLATFORMS_WIN,
    UI_ICON_TYPE_TILES_TILE,
    UI_ICON_TYPE_TILES_TILE2,
    UI_ICON_TYPE_TILES_TILE3,
    UI_ICON_TYPE_TILES_TILE4,

    UI_ICON_TYPE_COUNT = 111
} UIIconType;


#line 7 "/sys/src/kryon/include/ui_controls.h"
#line 1 "/sys/src/kryon/src/platform/plan9/include/stddef.h"

#line 3 "/sys/src/kryon/src/platform/plan9/include/stddef.h"








#line 8 "/sys/src/kryon/include/ui_controls.h"

typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;

typedef enum {
    ButtonStylePrimary,
    ButtonStyleSecondary,
    ButtonStyleDanger,
    ButtonStyleTab,
    ButtonStyleTabSelected
} ButtonStyle;

typedef enum {
    SyntaxNone,
    SyntaxKry,
    SyntaxC,
    SyntaxMake
} SyntaxMode;

typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
    int padding_y;
} TextInputStyle;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color text;
    Color border;
    float radius;
} ButtonSpec;

typedef struct {
    Rectangle bounds;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int icon_padding;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color icon_color;
    Color border;
    float radius;
} IconButtonProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    const char *href;
    int font;
    int focus_id;
    int disabled;
    Color color;
    Color hover_color;
} HrefProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    TextInputStyle style;
} TextInputProps;

typedef int (*TextInputFilter)(int codepoint, void *user_data);

typedef struct {
    char *text;
    size_t text_size;
    int *cursor_position;
    int max_codepoints;
    TextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} TextEdit;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int font;
    int focus_id;
    TextInputStyle style;
    TextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
    int secure;
    int read_only;
} TextFieldProps;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int *scroll_y;
    int max_codepoints;
    int font;
    int line_gap;
    int focus_id;
    const char *placeholder;
    SyntaxMode syntax;
    TextInputStyle style;
    TextInputFilter filter;
    void *filter_user_data;
    int content_version;
    int read_only;
    int wrap;
} TextAreaProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    TextInputStyle style;
    int line_gap;
} ReadonlyTextBoxProps;


#line 152 "/sys/src/kryon/include/ui_controls.h"
typedef struct UIStyleTokens {

#line 156 "/sys/src/kryon/include/ui_controls.h"
    float control_radius;
    float panel_radius;
    unsigned char control_alpha;
    unsigned char panel_alpha;
    unsigned char border_alpha;
    unsigned char shadow_alpha;
    unsigned char shine_alpha;
    int bevel_enabled;
    int touch_target_min;
    int shadow_offset_y;
} UIStyleTokens;

typedef struct UIMaterialScheme {
    Color primary;
    Color on_primary;
    Color secondary;
    Color on_secondary;
    Color surface;
    Color on_surface;
    Color surface_container;
    Color surface_variant;
    Color on_surface_variant;
    Color outline;
    Color error;
    Color on_error;
    Color disabled_container;
    Color disabled_content;
} UIMaterialScheme;

typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y,
                                             int h, int min, int max, int value);

typedef struct {
    const char *label;
    const char *font_name;
} UIDropdownOption;

UIStyleTokens GetUIStyleTokens(void);
UIStyleTokens GetUIStyleTokensForThemeStyle(ThemeStyle style);
UIMaterialScheme GetUIMaterialScheme(void);
void SetUIStyleTokens(UIStyleTokens tokens);
void ClearUIStyleTokensOverride(void);

int EditText(TextEdit edit);
void QueueTextInputCodepoint(int codepoint);
void QueueTextInputBackspace(void);
void QueueTextInputEnter(void);
int GetTextAreaSelection(int focus_id, int *start, int *end);
void SetTextAreaSelection(int focus_id, int anchor, int cursor);

int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);

void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);


#line 17 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_tk.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_tk.h"



typedef enum {
    SideTop,
    SideBottom,
    SideLeft,
    SideRight
} Side;

typedef struct {
    Rectangle bounds;
    int pad_x;
    int pad_y;
    int gap;
    int cursor_x;
    int cursor_y;
} FrameBox;

typedef struct {
    Rectangle bounds;
    int rows;
    int cols;
    int gap_x;
    int gap_y;
    int pad_x;
    int pad_y;
} Grid;

typedef enum {
    MenuCommand,
    MenuCheck,
    MenuRadio,
    MenuSeparator,
    MenuSubmenu
} MenuItemKind;

typedef struct MenuItem {
    MenuItemKind kind;
    const char *label;
    const char *accelerator;
    int id;
    int disabled;
    int checked;
    const struct MenuItem *submenu;
    int submenu_count;
} MenuItem;

typedef struct {
    Rectangle bounds;
    const char *label;
    const MenuItem *items;
    int item_count;
} Menu;

typedef struct {
    int activated_id;
    int open_index;
} MenuBarResult;

typedef struct {
    char text[4096];
    int pending;
} UIClipboardBuffer;

typedef enum {
    UI_CLIPBOARD_SOURCE_CLIPBOARD,
    UI_CLIPBOARD_SOURCE_PRIMARY,
    UI_CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD
} UIClipboardSource;

typedef int (*UIClipboardOSC52WriteFn)(void *userdata, const char *text);
typedef int (*UIClipboardPasteWriteFn)(void *userdata, const char *text,
                                       int size);

typedef struct {
    int id;
    Rectangle trigger;
    const MenuItem *items;
    int item_count;
    int *open;
    int *x;
    int *y;
} ContextMenuProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    int id;
    int checked;
    int disabled;
} RadioButtonProps;

typedef struct {
    Rectangle bounds;
    int min;
    int max;
    int value;
    const char *label;
} ProgressBarProps;

typedef struct {
    Rectangle bounds;
    int id;
    int min;
    int max;
    int step;
    int *value;
    int disabled;
    const char *value_text;
    int wrap;
} SpinboxProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
} ComboboxProps;

typedef struct {
    Rectangle bounds;
    const char *title;
} LabelFrameProps;

typedef struct {
    Rectangle bounds;
    Texture2D texture;
    Color tint;
} ImageBoxProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char **items;
    int item_count;
    int *selected_index;
    int *scroll_offset;
    int row_height;
} ListBoxProps;

typedef struct {
    const char *label;
    int depth;
    int id;
    int expanded;
    int selectable;
} UITreeItem;

typedef struct {
    Rectangle bounds;
    int id;
    const UITreeItem *items;
    int item_count;
    int *selected_id;
    int *scroll_offset;
    int row_height;
} TreeViewProps;

typedef struct {
    const char *label;
    int depth;
    int id;
    int is_dir;
    int selectable;
} UICascadingTreeItem;

typedef struct {
    int *ids;
    int *count;
    int capacity;
} UICascadingTreeExpansion;

typedef struct {
    Rectangle bounds;
    int id;
    const UICascadingTreeItem *items;
    int item_count;
    int *selected_id;
    int *activated_id;
    UICascadingTreeExpansion expanded;
    int *scroll_offset;
    int row_height;
} CascadingTreeViewProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int *scroll_x;
    int *scroll_y;
    int font_size;
    int line_height;
    int show_line_numbers;
} SourceViewProps;

typedef struct {
    const char **cells;
    int cell_count;
} UITableRow;

typedef struct {
    Rectangle bounds;
    int id;
    const char **columns;
    int column_count;
    const UITableRow *rows;
    int row_count;
    const int *column_widths;
    int *selected_row;
    int *selected_column;
    int *activated_row;
    int *activated_column;
    int *right_clicked_row;
    int *right_clicked_column;
    int *sort_column;
    int *scroll_offset;
    int row_height;
} TableViewProps;

typedef struct {
    Rectangle bounds;
    int *scroll_x;
    int *scroll_y;
    float *zoom;
} Canvas;

typedef struct {
    int active;
    int dragging;
    int selected_index;
    Vector2 world;
} CanvasResult;

typedef struct {
    Rectangle bounds;
    const char **tabs;
    int tab_count;
    int *selected_index;
} NotebookProps;

typedef struct {
    Rectangle bounds;
    int id;
    int vertical;
    int *split;
    int min_first;
    int min_second;
} PanedViewProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    int *open;
} CollapsibleProps;

typedef struct {
    const char *title;
    const char *message;
    const char *ok_label;
} MessageDialogProps;

typedef struct {
    const char *title;
    const char **labels;
    Texture2D *icons;
    int option_count;
    const char *cancel_label;
    int max_width;
} PickerDialogProps;

typedef struct {
    const char *title;
    const char *message;
    const char *cancel_label;
    const char *confirm_label;
} ConfirmDialogProps;

typedef struct {
    const char *title;
    char *text;
    int text_size;
    int *cursor_position;
    int *focused;
    const char *cancel_label;
    const char *confirm_label;
} PromptDialogProps;

typedef struct {
    int key;
    int ctrl;
    int shift;
    int alt;
    int id;
} Accelerator;

typedef struct {
    Rectangle bounds;
    const char *role;
    const char *label;
    int focused;
    int disabled;
    int checked;
} UIAccessibilityNode;

FrameBox BeginFrameBox(Rectangle bounds, int pad_x, int pad_y, int gap);
Rectangle FramePack(FrameBox *frame, Side side, int size);
Rectangle GridCell(Grid grid, int row, int col, int row_span, int col_span);
Rectangle Place(Rectangle parent, int x, int y, int w, int h);
CanvasResult BeginCanvas(Canvas canvas);
void EndCanvas(Canvas canvas);
int CanvasHitTest(Vector2 point, Rectangle *items, int item_count);
Vector2 CanvasToScreen(Canvas canvas, Vector2 point);
Rectangle CanvasRectToScreen(Canvas canvas, Rectangle rect);

int AcceleratorPressed(Accelerator accelerator);
int DispatchAccelerators(const Accelerator *accelerators, int count);
int ContextMenu(ContextMenuProps menu);
int SetUIClipboardTextValue(const char *text);
const char *GetUIClipboardTextValue(void);
int SetUIPrimarySelectionTextValue(const char *text);
const char *GetUIPrimarySelectionTextValue(void);
int UIClipboardSourceHasText(UIClipboardSource source);
const char *GetUIClipboardSourceText(const UIClipboardBuffer *clipboard,
                                     UIClipboardSource source);
int SetUIPrimarySelectionFromText(const char *text);
int CopyUISelectionTextToClipboard(UIClipboardBuffer *clipboard,
                                   const char *text);
int UIClipboardTargetIncludes(const char *target, char wanted);
int UIClipboardTargetUsesPrimary(const char *target);
const char *GetUIClipboardTargetText(const UIClipboardBuffer *clipboard,
                                     const char *target);
int RequestUIClipboardTargetWrite(UIClipboardBuffer *clipboard,
                                  const char *target, const char *text);
int HandleUIClipboardOSC52(UIClipboardBuffer *clipboard, const char *payload,
                           UIClipboardOSC52WriteFn write_response,
                           void *userdata);
int WriteUIClipboardPaste(const char *text, int bracketed,
                          UIClipboardPasteWriteFn write_text,
                          void *userdata);
int WriteUIClipboardTextPaste(UIClipboardBuffer *clipboard, const char *text,
                              int bracketed, UIClipboardPasteWriteFn write_text,
                              void *userdata);
int WriteUIClipboardSourcePaste(UIClipboardBuffer *clipboard,
                                UIClipboardSource source, int bracketed,
                                UIClipboardPasteWriteFn write_text,
                                void *userdata);
void InitUIClipboardBuffer(UIClipboardBuffer *buffer, const char *text);
int SetUIClipboardBufferText(UIClipboardBuffer *buffer, const char *text);
int RequestUIClipboardBufferWrite(UIClipboardBuffer *buffer, const char *text);
const char *GetUIClipboardBufferText(const UIClipboardBuffer *buffer);
int UIClipboardBufferHasPendingWrite(const UIClipboardBuffer *buffer);
int SyncUIClipboardBufferFromHost(UIClipboardBuffer *buffer);
int FlushUIClipboardBufferToHost(UIClipboardBuffer *buffer);

#line 18 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_draw.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_draw.h"
#line 1 "/sys/src/kryon/include/ui_icon_types.h"


























































































































#line 6 "/sys/src/kryon/include/ui_draw.h"

typedef struct {
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int width;
    int font;
    int line_gap;
    Color color;
} ParagraphSpec;

int GetUIFontSize(void);
int GetUISmallFontSize(void);
int GetUITitleFontSize(const char *title, int max_width);
int GetUIControlTextY(const char *text, int box_y, int box_h, int font);

void DrawCenteredUIControlText(const char *text, int center_x, int center_y,
                               int font, Color color);
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect,
                                 int font_size, Color color);
void DrawFittedTextInRect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color);

#line 19 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_icons.h"



#line 1 "/sys/src/kryon/include/kryon.h"





#line 7 "/sys/src/kryon/include/kryon.h"

























































#line 65 "/sys/src/kryon/include/kryon.h"











#line 81 "/sys/src/kryon/include/kryon.h"




#line 5 "/sys/src/kryon/include/ui_icons.h"
#line 1 "/sys/src/kryon/include/ui_icon_types.h"


























































































































#line 6 "/sys/src/kryon/include/ui_icons.h"

typedef struct UIIconAsset {
    UIIconType type;
    const char *name;
    const unsigned char *data;
    unsigned int size;
} UIIconAsset;

const UIIconAsset *GetUIIconAsset(UIIconType type);
const UIIconAsset *GetUIIconAssetByName(const char *name);
Texture2D LoadUIIconTexture(UIIconType type);
Texture2D LoadUIIconTextureByName(const char *name);
void LoadAllUIIconTextures(Texture2D *icons);
void UnloadAllUIIconTextures(Texture2D *icons);


extern const char *ui_icon_names[];


#line 20 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_modal.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_modal.h"
#line 1 "/sys/src/kryon/include/ui_controls.h"






















































































































































#line 152 "/sys/src/kryon/include/ui_controls.h"


#line 156 "/sys/src/kryon/include/ui_controls.h"

























































#line 6 "/sys/src/kryon/include/ui_modal.h"

typedef struct {
    const char *message;
    int width;
    int header_h;
    int button_h;
    int line_gap;
    int extra_lines;
    int min_height;
    int font;
} ParagraphModalMeasureProps;

typedef struct {
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    int min_width;
    int height;
} UITitleBarDropdown;

typedef struct {
    const char *label;
    ButtonStyle style;
    int disabled;
} ModalAction;

typedef struct {
    const char *title;
    const char *message;
    const ModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} ModalProps;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} UIPanelFrame;


#line 21 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_nav.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_nav.h"
#line 1 "/sys/src/kryon/include/ui_controls.h"






















































































































































#line 152 "/sys/src/kryon/include/ui_controls.h"


#line 156 "/sys/src/kryon/include/ui_controls.h"

























































#line 6 "/sys/src/kryon/include/ui_nav.h"
#line 1 "/sys/src/kryon/include/ui_icon_types.h"


























































































































#line 7 "/sys/src/kryon/include/ui_nav.h"

typedef struct {
    int id;
    int x;
    int y;
    int icon_size;
    int icon_padding;
    Texture2D icon;
    int *open;
    int *value;
    int min;
    int max;
    int popup_width;
    int popup_height;
} IconSliderPopupProps;

typedef struct {
    Texture2D icon;
    int disabled;
} IconRowItem;

typedef struct {
    int center_x;
    int view_width;
    int view_height;
    int count;
    const IconRowItem *items;
    int icon_size;
    int icon_padding;
    int gap;
    int side_margin;
    int bottom_margin;
    int max_button_width;
    int min_icon_size;
    int min_icon_padding;
    int min_gap;
} BottomIconRowProps;

typedef struct {
    int clicked_index;
    int y;
    int button_width;
} IconRowResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} BottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const BottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} BottomNavProps;

typedef struct {
    int clicked_index;
    int clicked_route;
    int y;
    int height;
} BottomNavResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} BottomNavOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const BottomNavOption *options;
    int option_count;
    const char *add_label;
    const char *cancel_label;
    const char *save_label;
    const char *reset_label;
    Texture2D close_icon;
} BottomNavConfigProps;

typedef struct {
    int action;
    int changed;
} BottomNavConfigResult;

typedef struct {
    Texture2D icon;
    int disabled;
} ToolbarAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    int dropdown_min_width;
    int dropdown_max_width;
    int dropdown_height;
    const ToolbarAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} ToolbarProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} ToolbarResult;

typedef struct {
    ToolbarProps toolbar;
    Texture2D leading_icon;
    int leading_width;
    int leading_icon_size;
    int leading_icon_padding;
} ToolbarHeaderProps;

typedef struct {
    ToolbarResult toolbar;
    int leading_clicked;
} ToolbarHeaderResult;

typedef struct {
    Texture2D icon;
    int disabled;
} TopNavAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    const char *title;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    int dropdown_min_width;
    int dropdown_height;
    const TopNavAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} TopNavProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} TopNavResult;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} Subtab;

typedef struct {
    Rectangle bounds;
    const Subtab *tabs;
    int count;
    int selected_index;
    int font;
} SubtabBarProps;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
    int italic;
    int closeable;
} Tab;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
    int *closed_index;
    int *double_clicked_index;
} TabBarProps;

typedef enum {
    PaneDropNone,
    PaneDropCenter,
    PaneDropLeft,
    PaneDropRight,
    PaneDropTop,
    PaneDropBottom
} PaneDropZone;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int *dragged_index;
} PaneTabBar;

typedef struct {
    int clicked_index;
    int dragged_index;
} PaneTabBarResult;

PaneDropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
int GetTabBarHeight(void);
int TabBarHeight(void);


#line 22 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_overlay.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_overlay.h"

typedef struct {
    Rectangle anchor;
    const char *text;
} UIGuideStep;

typedef struct {
    const UIGuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} GuideOverlayProps;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} UIGuideResult;

typedef struct {
    int draw_source_menu;
    int draw_mode_menu;
    int draw_palette_menu;
    int draw_style_menu;
    int palette_index;
} UIThemeSettingsState;

typedef struct {
    int id_base;
    int x;
    int y;
    int w;
    int *theme_source;
    int *theme_mode;
    int *theme_id;
    int *theme_style;
    int allow_system_source;
    int allow_system_mode;
    const char *theme_label;
    const char *source_app_label;
    const char *source_system_label;
    const char *mode_label;
    const char *mode_system_label;
    const char *mode_light_label;
    const char *mode_dark_label;
    const char *palette_label;
    const char *style_label;
    const char *style_system_label;
    const char *style_retro_label;
    const char *style_material_label;
    const char *style_fluent_label;
    const char *style_adwaita_label;
    const char *style_liquid_glass_label;
    const char *system_theme_label;
} ThemeSettingsProps;

typedef struct {
    int changed;
    int source_changed;
    int mode_changed;
    int palette_changed;
    int style_changed;
} UIThemeSettingsResult;


#line 23 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_profile.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_profile.h"
#line 1 "/sys/src/kryon/include/ui_icon_types.h"


























































































































#line 6 "/sys/src/kryon/include/ui_profile.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char *username;
    const char *subtitle;
    const char *friends_text;
    Texture2D pfp_icon;
    const Texture2D *icons;
    UIIconType pfp_icon_type;
    int content_padding_x;
    int current_frame;
    int block_click_frame;
} SidebarAccountHeaderProps;

typedef struct {
    int pfp_clicked;
    int username_clicked;
    int friends_clicked;
    int height;
} SidebarAccountHeaderResult;

typedef struct {
    const char *title;
    const Texture2D *icons;
    UIIconType *selected_icon_type;
    Texture2D close_icon;
    int max_width;
    int *scroll_offset;
} ProfilePicturePickerProps;

typedef struct {
    int closed;
    int changed;
    int selected_index;
    UIIconType selected_icon_type;
} ProfilePicturePickerResult;

typedef enum {
    UI_KSYNC_PROFILE_ICON_NONE = 0,
    UI_KSYNC_PROFILE_ICON_BIRD = 1,
    UI_KSYNC_PROFILE_ICON_BOWL = 2,
    UI_KSYNC_PROFILE_ICON_CACTUS = 3,
    UI_KSYNC_PROFILE_ICON_HEART = 4,
    UI_KSYNC_PROFILE_ICON_INCENSE = 5,
    UI_KSYNC_PROFILE_ICON_LOTUS = 6,
    UI_KSYNC_PROFILE_ICON_TREE1 = 7,
    UI_KSYNC_PROFILE_ICON_TREE2 = 8,
    UI_KSYNC_PROFILE_ICON_TREE3 = 9,
    UI_KSYNC_PROFILE_ICON_TREE4 = 10,
    UI_KSYNC_PROFILE_ICON_TREE5 = 11,
    UI_KSYNC_PROFILE_ICON_BAMBUS = 12,
    UI_KSYNC_PROFILE_ICON_BUSH = 13,
    UI_KSYNC_PROFILE_ICON_COFFEE = 14,
    UI_KSYNC_PROFILE_ICON_FLOWER1 = 15,
    UI_KSYNC_PROFILE_ICON_FLOWER2 = 16,
    UI_KSYNC_PROFILE_ICON_MOUNTAIN = 17,
    UI_KSYNC_PROFILE_ICON_MUSHROOM = 18,
    UI_KSYNC_PROFILE_ICON_PERSON1 = 19,
    UI_KSYNC_PROFILE_ICON_RAINBOW = 20,
    UI_KSYNC_PROFILE_ICON_TENT = 21,
    UI_KSYNC_PROFILE_ICON_BUTTERFLY = 22,
    UI_KSYNC_PROFILE_ICON_DRAGONFLY = 23,
    UI_KSYNC_PROFILE_ICON_FIREPLACE = 24,
    UI_KSYNC_PROFILE_ICON_FOX = 25,
    UI_KSYNC_PROFILE_ICON_PALM = 26
} UIKsyncProfileIcon;

int GetUIProfilePictureIconCount(void);
UIIconType GetUIProfilePictureIconType(int index);
const char *GetUIProfilePictureIconName(int index);
UIIconType GetUIProfilePictureIconTypeForKsyncID(int ksync_id);
int GetUIKsyncIDForProfilePictureIconType(UIIconType type);


#line 24 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_reorder.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_reorder.h"

typedef struct {
    int id;
    Rectangle bounds;
    int disabled;
} UIReorderItem;

typedef struct {
    int id;
    Rectangle bounds;
    const UIReorderItem *items;
    int item_count;
    int handle_width;
    int drag_threshold;
    int *scroll_offset;
    int max_scroll;
    int viewport_top;
    int viewport_bottom;
    int auto_scroll_margin;
    int auto_scroll_step;
} UIReorderList;

typedef struct {
    int active;
    int dragging;
    int committed;
    int from_index;
    int to_index;
    int active_index;
    int target_index;
    int active_id;
    int pointer_y;
    int drag_delta_y;
} UIReorderListResult;

UIReorderListResult UpdateUIReorderList(UIReorderList list);


#line 25 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_rows.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_rows.h"
#line 1 "/sys/src/kryon/include/ui_controls.h"






















































































































































#line 152 "/sys/src/kryon/include/ui_controls.h"


#line 156 "/sys/src/kryon/include/ui_controls.h"

























































#line 6 "/sys/src/kryon/include/ui_rows.h"

typedef struct {
    const char *text;
    int font;
    Color color;
} UIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const UIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} InfoRowsProps;

typedef struct {
    const char *label;
    ButtonStyle style;
    int disabled;
} UIButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const UIButtonRowItem *items;
    int count;
} ButtonRowProps;

typedef struct {
    const char *label;
    TextFieldProps field;
    int label_font;
    int label_h;
    int field_h;
    int gap;
    int bottom_gap;
    Color label_color;
} LabelTextFieldProps;

typedef struct {
    const char *label;
    int font;
    int info_button;
    int icon_diameter;
    int height;
    Color color;
} SectionLabelProps;

typedef struct {
    const char *label;
    int *value;
    int height;
    int disabled;
} CheckboxRowProps;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int disabled;
    Color background;
    Color hover_background;
    Color border;
    Color hover_border;
    Color text;
} OverlayButtonProps;

int GetUILabelTextFieldHeight(LabelTextFieldProps row);
int GetUIButtonRowHeight(ButtonRowProps row);


#line 26 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_scroll.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"












































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































































#line 5 "/sys/src/kryon/include/ui_scroll.h"

typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} UIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} UIScrollView;

typedef int (*UIScrollPageHeightFn)(int content_width, void *user_data);

typedef struct {
    int y;
    int height;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    UIScrollPageHeightFn content_height;
    void *user_data;
} UIScrollPageSpec;

typedef struct {
    UIScrollArea area;
    UIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} UIScrollPage;

int GetUIScrollbarReservedWidth(int max_scroll);
int GetUIScrollbarContentWidth(int content_width, int max_scroll);
int GetUIScrollbarSafeContentWidth(int content_x, int content_width,
                                   int scrollbar_x, int max_scroll);
UIScrollView MeasureUIScrollContainer(UIScrollArea area);
UIScrollView BeginUIScrollContainer(UIScrollArea area);
void EndUIScrollContainer(UIScrollArea area, UIScrollView view);
void EnsureUIScrollRectVisible(UIScrollArea area, Rectangle rect, int margin);
UIScrollPage BeginUIScrollPage(UIScrollPageSpec spec);
void EndUIScrollPage(UIScrollPage page);


#line 27 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_text.h"



#line 1 "/sys/src/kryon/include/kryon.h"





#line 7 "/sys/src/kryon/include/kryon.h"

























































#line 65 "/sys/src/kryon/include/kryon.h"











#line 81 "/sys/src/kryon/include/kryon.h"




#line 5 "/sys/src/kryon/include/ui_text.h"












typedef struct {
    int font_size;
    Color color;
    int italic;
    int selectable;
} TextStyle;

typedef struct {
    int id;
    const char *text;
    Rectangle bounds;
    int font_size;
    int line_gap;
    Color color;
} SelectableTextBlock;

Font GetUIFont(void);
int EnsureUIDefaultFont(void);
int RegisterUIFont(const char *name, Font font);
int RegisterUISmallFont(const char *name, Font font);
int RegisterUIFontSource(const char *name, const char *file_type,
                         const unsigned char *font_data, unsigned int font_size,
                         const int *codepoints, int codepoint_count);
int RegisterUIFixedFontSource(const char *name, const char *file_type,
                              const unsigned char *font_data,
                              unsigned int font_size,
                              const int *codepoints, int codepoint_count);
int RegisterUIFontFileSource(const char *name, const char *path,
                             const int *codepoints, int codepoint_count);
int UseUIFont(const char *name);
int PushUIFont(const char *name);
void PopUIFont(int token);
int UIFontHasGlyph(Font font, int codepoint);
Font LoadUIFontFromMemory(const char *file_type, const unsigned char *font_data, unsigned int font_size, int base_size);
Font LoadUIFontAsset(const char *path, int base_size);
void UnloadUIFont(Font *font);
void ClearUIFonts(void);

#line 56 "/sys/src/kryon/include/ui_text.h"
void UIFontMemoryReport(const char *tag);
int TextWidth(const char *text, int font_size);
int TextHeight(const char *text, int font_size);
int TextLineHeight(int font_size);
int ScaledTextWidth(const char *text, int scale);
Font GetUIFontForCodepoint(int codepoint, int font_size);
float GetUIFontScale(Font font, int font_size);
int PushTextSelectable(int selectable);
void PopTextSelectable(int token);
int TextBaselineY(const char *text, int box_y, int box_h, int font_size);
int ScaledTextBaselineY(const char *text, int box_y, int box_h, int scale);


#line 28 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui_toast.h"



void ShowToast(const char *message);
void ShowToastFor(const char *message, double seconds);
void ClearToast(void);
void DrawToast(void);


#line 29 "/sys/src/kryon/include/kryon.h"
#line 1 "/sys/src/kryon/include/ui.h"




#line 8 "/sys/src/kryon/include/ui.h"

#line 1 "/sys/src/kryon/include/ui_core.h"



































































































#line 10 "/sys/src/kryon/include/ui.h"
#line 1 "/sys/src/kryon/include/ui_controls.h"






















































































































































#line 152 "/sys/src/kryon/include/ui_controls.h"


#line 156 "/sys/src/kryon/include/ui_controls.h"

























































#line 11 "/sys/src/kryon/include/ui.h"
#line 1 "/sys/src/kryon/include/ui_tk.h"








































































































































































































































































































































































#line 12 "/sys/src/kryon/include/ui.h"
#line 1 "/sys/src/kryon/include/ui_draw.h"





























#line 13 "/sys/src/kryon/include/ui.h"
#line 1 "/sys/src/kryon/include/ui_inspect.h"



#line 1 "/sys/src/kryon/include/ui_widget.h"



#line 1 "/sys/src/kryon/include/kryon_compat.generated.h"


#line 17 "/sys/src/kryon/include/kryon_compat.generated.h"

















#line 115 "/sys/src/kryon/include/kryon_compat.generated.h"


















































































































































































































































































































































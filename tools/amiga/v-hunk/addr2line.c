/*
 * A portable addr2line-like utility for AmigaDOS LINE DEBUG hunks and symbols.
 * Written in 2026 by Frank Wille.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "doshunks.h"

#define VERSION 0
#define REVISION 2

struct Symbol {
  uint32_t offset;
  const char *name;
};

struct SymbolNode {
  struct SymbolNode *next;
  struct Symbol sym;
};

struct LineInfo {
  uint32_t offset;
  const char *path;
  int line;
};

struct LineInfoNode {
  struct LineInfoNode *next;
  struct LineInfo li;
};

struct Section {
  struct Section *next;
  int id;
  const char *name;
  uint32_t vaddr;
  uint32_t size;
  uint32_t filesize;
  int num_symbols;
  union {
    struct SymbolNode *first;
    struct Symbol *list;
  } sym;
  int num_lines;
  union {
    struct LineInfoNode *first;
    struct LineInfo *list;
  } li;
};

/* options */
static int show_addr;  /* print address to output */
static int show_func;  /* print last function name to output */
static int pretty;     /* pretty-print */
static int basename;   /* strip the path before the base file name */

/* parser */
static int num_secs;
static struct Section *first_sec,*last_sec,**secs;

static const char unknown[] = "??";


static void *mymalloc(size_t n)
{
  void *p;

  if (n == 0)
    n = sizeof(void *);
  p = malloc(n);
  if (p == NULL) {
    fprintf(stderr,"out of memory!\n");
    exit(EXIT_FAILURE);
  }
  return p;
}


static void read_err(FILE *f)
{
  if (feof(f))
    fprintf(stderr,"end of file while still expecting data!\n");
  else
    fprintf(stderr,"read error!\n");
  exit(EXIT_FAILURE);
}


static uint8_t fr8(FILE *f)
{
  uint8_t dat;

  if (fread(&dat,1,1,f) != 1)
    read_err(f);
  return dat;
}


static uint16_t fr16(FILE *f)
{
  uint8_t dat[2];

  if (fread(dat,sizeof(dat),1,f) != 1)
    read_err(f);
  return (dat[0]<<8) | dat[1];
}


static uint32_t fr32(FILE *f)
{
  uint8_t dat[4];

  if (fread(dat,sizeof(dat),1,f) != 1)
    read_err(f);
  return (dat[0]<<24) | (dat[1]<<16) | (dat[2]<<8) | dat[3];
}


static const char *frstr32(FILE *f,int len)
{
  char *str;

  if (len == 0)
    return "";
  len <<= 2;
  str = mymalloc(len+1);
  if (fread(str,1,len,f) != (size_t)len)
    read_err(f);
  str[len] = '\0';
  return str;
}


static void fskip(FILE *f,long nbytes)
{
  if (fseek(f,nbytes,SEEK_CUR) != 0)
    read_err(f);
}


static void falign32(FILE *f)
{
  long offs = ftell(f);

  if (offs >= 0) {
    if (offs & 3)
      fskip(f,4-(offs&3));
  }
  else
    read_err(f);
}


static void fskip_longrelocs(FILE *f)
{
  int n;

  while ((n = fr32(f)) != 0)
    fskip(f,(n+1)<<2);
}


static void fskip_shortrelocs(FILE *f)
{
  int n;

  while ((n = fr16(f)) != 0)
    fskip(f,(n+1)<<1);
  falign32(f);
}


static int frhcln(FILE *f)
{
  int val;

  /* read HiSoft Compressed Line Number */
  if (!(val = fr8(f)))
    if (!(val = fr16(f)))
      val = fr32(f);
  return val;
}


static uint32_t frsecsize(FILE *f)
{
  uint32_t sz;

  sz = fr32(f);
  if ((sz & HUNKF_MEMTYPE) == (uint32_t)(HUNKF_FAST|HUNKF_CHIP))
    fskip(f,4);  /* ignore extended memory attributes */
  return (sz & ~HUNKF_MEMTYPE) << 2;  /* return section size in bytes */
}


static void insert_symbol(struct Section *sec,const char *name,uint32_t offs)
{
  struct SymbolNode *sym,*last,*new;

  new = mymalloc(sizeof(struct SymbolNode));
  new->sym.name = name;
  new->sym.offset = offs;

  /* insert into section's symbol list, sorted by offset */
  for (sym=sec->sym.first,last=NULL; sym!=NULL; sym=sym->next) {
    if (offs < sym->sym.offset)
      break;
    last = sym;
  }
  if (last)
    last->next = new;
  else
    sec->sym.first = new;
  new->next = sym;
  sec->num_symbols++;
}


static void insert_line_offs(struct Section *sec,const char *path,
                             int line,uint32_t offs)
{
  struct LineInfoNode *li,*last,*new;

  new = mymalloc(sizeof(struct LineInfoNode));
  new->li.path = path;
  new->li.line = line;
  new->li.offset = offs;

  /* insert into section's line-info list, sorted by offset */
  for (li=sec->li.first,last=NULL; li!=NULL; li=li->next) {
    if (offs == li->li.offset) {
      fprintf(stderr,"double line info for section #%d at offset %#lx ignored\n",
              sec->id,(unsigned long)offs);
      free(new);
      return;
    }
    if (offs < li->li.offset)
      break;
    last = li;
  }
  if (last)
    last->next = new;
  else
    sec->li.first = new;
  new->next = li;
  sec->num_lines++;
}


static void finalize_section_lists(struct Section *sec)
{
  struct Symbol *symlist = mymalloc(sizeof(struct Symbol) * sec->num_symbols);
  struct LineInfo *lilist = mymalloc(sizeof(struct LineInfo) * sec->num_lines);
  struct SymbolNode *sn,*nextsn;
  struct Symbol *sym;
  struct LineInfoNode *ln,*nextln;
  struct LineInfo *li;
  int i;

  for (sn=sec->sym.first,sym=symlist,i=0; sn!=NULL && i<sec->num_symbols; i++) {
    nextsn = sn->next;
    *sym++ = sn->sym;
    free(sn);
    sn = nextsn;
  }
  if (sn!=NULL || i!=sec->num_symbols) {
    fprintf(stderr,"Internal error detected! Corrupted symbol list!\n");
    exit(EXIT_FAILURE);
  }
  sec->sym.list = symlist;

  for (ln=sec->li.first,li=lilist,i=0; ln!=NULL && i<sec->num_lines; i++) {
    nextln = ln->next;
    *li++ = ln->li;
    free(ln);
    ln = nextln;
  }
  if (ln!=NULL || i!=sec->num_lines) {
    fprintf(stderr,"Internal error detected! Corrupted line-info list!\n");
    exit(EXIT_FAILURE);
  }
  sec->li.list = lilist;
}


static struct Section *new_section(void)
{
  struct Section *s = mymalloc(sizeof(struct Section));

  s->next = NULL;
  s->id = num_secs++;
  s->name = NULL;
  s->size = 0;
  s->num_symbols = 0;
  s->sym.first = NULL;
  s->num_lines = 0;
  s->li.first = NULL;
  if (last_sec)
    last_sec = last_sec->next = s;
  else
    first_sec = last_sec = s;
  return s;
}


static void make_section_array(void)
{
  struct Section *s;
  int i;

  secs = mymalloc(num_secs*sizeof(struct Section *));
  for (s=first_sec,i=0; s!=NULL && i<num_secs; s=s->next,i++) {
    if (i != s->id)
      break;
    secs[i] = s;
  }

  if (s!=NULL || i<num_secs) {
    fprintf(stderr,"Internal error allocating section array!\n");
    exit(EXIT_FAILURE);
  }
}


static void debug_hunk(const char *fname,FILE *f,struct Section *sec)
{
  (void)fname;
  uint32_t base,line,offs,type;
  const char *name;
  int n,len;

  n = fr32(f);  /* DEBUG hunk size */
  if (n >= 3) {
    base = fr32(f);  /* base for all offsets */
    type = fr32(f);  /* DEBUG hunk type */
    n -= 2;

    switch (type) {
      case 0x4c494e45:  /* LINE */
        len = fr32(f);
        name = frstr32(f,len);  /* source file name */
        for (n-=len+1; n>0; n-=2) {
          line = fr32(f);
          offs = fr32(f);
          insert_line_offs(sec,name,line,base+offs);
        }
        return;

      case 0x48434c4e:  /* HCLN */
        len = fr32(f);
        name = frstr32(f,len);  /* source file name */
        if (n-len-1 > 0) {
          len = fr32(f);  /* number of lines */
          line = 0;
          offs = base;
          while (len--) {
            line += frhcln(f);
            offs += frhcln(f);
            insert_line_offs(sec,name,line,offs);
          }
          falign32(f);
        }
        return;
    }
  }

  fskip(f,n<<2);  /* skip unknown debug type */
}


static void parse_executable(const char *fname,FILE *f)
{
  int i,n;

  if (fr32(f) != 0) {
    fprintf(stderr,"\"%s\": resident library names are not supported\n",fname);
    exit(EXIT_FAILURE);
  }
  n = fr32(f);
  i = fr32(f);
  if (i!=0 || (int)fr32(f)!=n-1) {
    fprintf(stderr,"\"%s\": overlays are not supported\n",fname);
    exit(EXIT_FAILURE);
  }

  /* allocate and initialize sections, store their memory size */
  for (i=0; i<n; i++) {
    struct Section *s;

    s = new_section();
    s->size = frsecsize(f);
  }
  make_section_array();  /* make num_secs secs[] pointers */

  /* hunk loop */
  for (i=0; i<num_secs; i++) {
    const char *name;
    uint32_t htype;

    while ((htype = fr32(f)) != HUNK_END) {
      switch (htype &= ~HUNKF_MEMTYPE) {
        case HUNK_CODE:
        case HUNK_DATA:
        case HUNK_BSS:
          secs[i]->filesize = frsecsize(f);
          if (htype != HUNK_BSS)
            fskip(f,secs[i]->filesize);
          break;

        case HUNK_RELOC32:
          fskip_longrelocs(f);
          break;

        case HUNK_RELOC32SHORT:
        case HUNK_DREL32:  /* V37 RELOC32SHORT */
        case HUNK_RELRELOC32:
          fskip_shortrelocs(f);
          break;

        case HUNK_SYMBOL:
          while ((n = fr32(f)) != 0) {
            name = frstr32(f,n);
            insert_symbol(secs[i],name,fr32(f));
          }
          break;

        case HUNK_DEBUG:
          debug_hunk(fname,f,secs[i]);
          break;

        default:
          fprintf(stderr,"\"%s\": unexpected hunk type %#lx\n",
                  fname,(unsigned long)htype);
          exit(EXIT_FAILURE);
      }
    }
    /* calculate a virtual section base address, assuming all sections are
       loaded in a single contiguous block in memory */
    secs[i]->vaddr = i ? secs[i-1]->vaddr + secs[i-1]->size : 0;
  }
}


static void parse_object(const char *fname,FILE *f)
{
  int c;

  (void)frstr32(f,fr32(f));  /* skip unit name */

  while ((c = fgetc(f)) != EOF) {
    struct Section *s;
    uint32_t type;

    ungetc(c,f);
    s = new_section();

    while ((type = fr32(f)) != HUNK_END) {
      switch (type &= ~HUNKF_MEMTYPE) {
        case HUNK_UNIT:
        case HUNK_LIB:
        case HUNK_INDEX:
          fprintf(stderr,"\"%s\" is a library.\n",fname);
          exit(EXIT_FAILURE);

        case HUNK_NAME:
          s->name = frstr32(f,fr32(f));
          break;

        case HUNK_CODE:
        case HUNK_PPC_CODE:
        case HUNK_DATA:
        case HUNK_BSS:
          s->filesize = s->size = frsecsize(f);
          if (type != HUNK_BSS)
            fskip(f,s->filesize);
          break;

        case HUNK_ABSRELOC32:
        case HUNK_ABSRELOC16:
        case HUNK_RELRELOC32:
        case HUNK_RELRELOC26:
        case HUNK_RELRELOC16:
        case HUNK_RELRELOC8:
        case HUNK_DREL32:
        case HUNK_DREL16:
        case HUNK_DREL8:
          fskip_longrelocs(f);
          break;

        case HUNK_RELOC32SHORT:
          fskip_shortrelocs(f);
          break;

        case HUNK_EXT:
        case HUNK_SYMBOL:
          while ((type = fr32(f)) != 0) {
            const char *name = frstr32(f,type&0xffffff);
            uint8_t stype = type >> 24;

            if (stype & 0x80) {  /* external references */
              if (stype==EXT_ABSCOMMON || stype==EXT_RELCOMMON)
                fskip(f,4);  /* skip common size */
              fskip(f,fr32(f)<<2);  /* skip ext. reference offsets */
            }
            else switch(stype) {  /* external definition */
              case EXT_SYMB:
              case EXT_DEF:
                insert_symbol(s,name,fr32(f));
                break;
              case EXT_ABS:  /* absolute symbols are ignored */
                fskip(f,4);
                break;
              default:
                fprintf(stderr,"\"%s\": illegal symbol type %u\n",
                        fname,(unsigned)stype);
                exit(EXIT_FAILURE);
            }
          }
          break;

        case HUNK_DEBUG:
          debug_hunk(fname,f,s);
          break;

        default:
          fprintf(stderr,"\"%s\": unexpected hunk type %#lx\n",
                  fname,(unsigned long)type);
          exit(EXIT_FAILURE);
      }
    }
  }
  make_section_array();  /* make num_secs secs[] pointers */
}


static void parse_hunk_format(const char *fname)
{
  FILE *f = fopen(fname,"rb");
  int i;

  if (f == NULL) {
    fprintf(stderr,"cannot open input file \"%s\" for reading!\n",fname);
    exit(EXIT_FAILURE);
  }
  i = (int)fr32(f);
  if (i == HUNK_HEADER)
    parse_executable(fname,f);
  else if (i == HUNK_UNIT)
    parse_object(fname,f);
  else {
    fprintf(stderr,"\"%s\" is not a hunk-format file!\n",fname);
    exit(EXIT_FAILURE);
  }
  fclose(f);

  /* convert symbol- and line lists into arrays for faster access */
  for (i=0; i<num_secs; i++)
    finalize_section_lists(secs[i]);
}


struct Symbol *get_symbol(int secno,uint32_t offs)
{
  static struct Symbol none = { 0, unknown };
  struct Symbol *syms;
  int l,m,r;

  if (secno<0 || secno>=num_secs)
    return &none;
  syms = secs[secno]->sym.list;
  if (offs>secs[secno]->size || secs[secno]->num_symbols==0 || offs<syms[0].offset)
    return &none;

  /* binary search for the nearest offset <= offs */
  l = 0;
  r = secs[secno]->num_symbols - 1;
  while (l != r) {
    m = (r + l + 1) >> 1;
    if (syms[m].offset > offs)
      r = m - 1;
    else
      l = m;
  }
  return &syms[l];
}


struct LineInfo *get_line_info(int secno,uint32_t offs)
{
  static struct LineInfo none = { 0,unknown,-1 };
  struct LineInfo *li;
  int l,m,r;

  if (secno<0 || secno>=num_secs)
    return &none;
  li = secs[secno]->li.list;
  if (offs>secs[secno]->size || secs[secno]->num_lines==0 || offs<li[0].offset)
    return &none;

  /* binary search for the nearest offset <= offs */
  l = 0;
  r = secs[secno]->num_lines - 1;
  while (l != r) {
    m = (r + l + 1) >> 1;
    if (li[m].offset > offs)
      r = m - 1;
    else
      l = m;
  }
  return &li[l];
}


static const char *transform_path(const char *path)
{
  if (basename) {
    const char *fname;

    if ((fname = strrchr(path,'/')) != NULL ||
        (fname = strrchr(path,'\\')) != NULL ||
        (fname = strrchr(path,':')) != NULL)
      return ++fname;
  }
  return path;
}


static void print_line_info(int secno,char *addrstr)
{
  struct LineInfo *li;
  char *end = NULL;
  unsigned long addr = 0;
  unsigned long offs = 0;
  char *colon = strchr(addrstr,':');

  if (colon) {
    unsigned long parsed_sec;

    /* syntax "section-id:address" overwrites secno (-j) */
    *colon = '\0';
    parsed_sec = strtoul(addrstr,&end,0);
    if (end != addrstr && *end == '\0') {
      secno = (int)parsed_sec;
    }
    *colon = ':';
    addr = strtoul(colon+1,&end,16);
    if (end == colon+1) {
      addr = 0;
    }
  }
  else {
    addr = strtoul(addrstr,&end,16);
    if (end == addrstr) {
      addr = 0;
    }
  }

  if (secno < 0) {
    /* not a section-offset, find matching section */
    for (secno=0; secno<num_secs; secno++) {
      if (addr>=secs[secno]->vaddr &&
          addr<secs[secno]->vaddr+secs[secno]->size) {
        offs = addr - secs[secno]->vaddr;  /* turn into offset */
        break;
      }
    }
  }
  else
    offs = addr;

  li = get_line_info(secno,offs);
  if (show_addr) {
    printf(pretty?"0x%08lx: ":"0x%08lx\n",addr);
  }
  if (show_func) {
    struct Symbol *sym = get_symbol(secno,offs);
    printf(pretty?"%s at ":"%s\n",sym->name);
  }
  if (li->line >= 0) {
    printf("%s:%d\n",transform_path(li->path),li->line);
  }
  else {
    printf("%s:?\n",transform_path(li->path));
  }
}


static void arg_missing(const char *optstr)
{
  fprintf(stderr,"%s: argument missing!\n",optstr);
  exit(EXIT_FAILURE);
}


int main(int argc,char *argv[])
{
  char *fname = "a.out";
  char *secname = NULL;
  int secid = -1;
  int i;

  setvbuf(stdout,NULL,_IOLBF,0);

  /* read options */
  for (i=1; i<argc; i++) {
    if (argv[i][0] != '-')
      break;
    switch (argv[i][1]) {
      case 'a':
        show_addr = 1;
        break;
      case 'e':
        if (i+1 < argc)
          fname = argv[++i];
        else
          arg_missing(argv[i]);
        break;
      case 'f':
        show_func = 1;
        break;
      case 'h':
        printf("Usage: %s [option(s)] [addr(s)]\n"
               "Options:\n"
               "  -a              show addresses\n"
               "  -C              demangle C++ function names\n"
               "  -e <name>       input file name (default: a.out)\n"
               "  -f              show function names\n"
               "  -h              display help\n"
               "  -j <name>/<id>  read addresses as offsets relative to a section\n"
               "  -p              pretty printing in a single line\n"
               "  -s              show the file name without full path\n"
               "  -v              display version information\n",
               argv[0]);
        exit(EXIT_SUCCESS);
      case 'j':
        if (i+1 < argc) {
          if (isdigit((unsigned char)argv[++i][0]))
            secid = atoi(argv[i]);
          else
            secname = argv[i];
        }
        else
          arg_missing(argv[i]);
        break;
      case 'p':
        pretty = 1;
        break;
      case 's':
        basename = 1;
        break;
      case 'C':
        /* ignored */
        break;
      case 'v':
        printf("%s %d.%d Amiga hunk-format\n"
               "Written by Frank Wille 2026.\n"
               "Program and source text is Public Domain.\n",
               argv[0],VERSION,REVISION);
        exit(EXIT_SUCCESS);
      default:
        fprintf(stderr,"unknown option %s ignored\n",argv[i]);
        break;
    }
  }

  parse_hunk_format(fname);

  /* find id for a named section */
  if (secname) {
    int j;

    for (j=0; j<num_secs; j++) {
      if (secs[j]->name!=NULL && strcmp(secs[j]->name,secname)==0) {
        secid = j;
        break;
      }
    }
    if (secid < 0) {
      fprintf(stderr,"no section with name \"%s\" found\n",secname);
      exit(EXIT_FAILURE);
    }
  }

  /* output */
  if (i < argc) {
    for (; i<argc; i++)
      print_line_info(secid,argv[i]);
  }
  else {  /* interactive mode */
    char buf[80];

    while (fgets(buf,80,stdin))
      print_line_info(secid,buf);
  }

  return EXIT_SUCCESS;
}

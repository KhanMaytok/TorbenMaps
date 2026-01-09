/* planet.c */
/* planet generating program */
/* Copyright 1988--2014 Torben AE. Mogensen */

/* version of November 2014 */

/* The program generates planet maps based on recursive spatial subdivision */
/* of a tetrahedron containing the globe. The output is a colour PPM bitmap. */

/* The colours may optionally be modified according to latitude to move the */
/* icecaps lower closer to the poles, with a corresponding change in land colours. */

/* The Mercator map at magnification 1 is scaled to fit the Width */
/* it uses the full height (it could extend infinitely) */
/* The orthographic projections are scaled so the full view would use the */
/* full Height. Areas outside the globe are coloured black. */
/* Stereographic and gnomonic projections use the same scale as orthographic */
/* in the center of the picture, but distorts scale away from the center. */

/* It is assumed that pixels are square */
/* I have included procedures to print the maps as bmp (Windows) or */
/* ppm(portable pixel map) bitmaps  on standard output or specified files. */

/* I have tried to avoid using machine specific features, so it should */
/* be easy to port the program to any machine. Beware, though that due */
/* to different precision on different machines, the same seed numbers */
/* can yield very different planets. */

/* The primitive user interface is primarily a result of portability concerns */

#ifdef THINK_C
#define macintosh 1
#endif

#ifdef macintosh
#include <console.h>
#include <unix.h>
#endif

#define O_BINARY 0x8000

#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

int BLACK = 0;
int WHITE = 1;
int BACK = 2;
int GRID = 3;
int OUTLINE1 = 4;
int OUTLINE2 = 5;
int LOWEST = 6;
int SEA = 7;
int LAND = 8;
int HIGHEST = 9;

int debug = 0;

int nonLinear = 0;

char view;

int nocols = 65536;

int rtable[65536], gtable[65536], btable[65536];

/* Supported output file types:
    BMP - Windows Bit MaPs
    PPM - Portable Pix Maps
    XPM - X-windows Pix Maps
 */

typedef enum ftype
    {
	bmp,
	ppm,
	xpm,
    heightfield
    }
    ftype;

ftype file_type = bmp;

char* file_ext(ftype file_type)
{
  switch (file_type)
  {
    case bmp:
      return (".bmp");
    case ppm:
      return (".ppm");
    case xpm:
      return (".xpm");
    case heightfield:
      return (".heightfield");
    default:
      return ("");
  }
}

/* Character table for XPM output */

char letters[64] = {
	'@','$','.',',',':',';','-','+','=','#','*','&','A','B','C','D',
	'E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T',
	'U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j',
	'k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};

#define PI 3.14159265358979
#define DEG2RAD 0.0174532918661 /* pi/180 */

/* these three values can be changed to change world characteristica */

double M  = -.02;   /* initial altitude (slightly below sea level) */
double dd1 = 0.45;  /* weight for altitude difference */
double POWA = 1.0; /* power for altitude difference */
double dd2 = 0.035; /* weight for distance */
double POW = 0.47;  /* power for distance function */

int Depth; /* depth of subdivisions */
double r1,r2,r3,r4; /* seeds */
double longi,lat,scale;
double vgrid, hgrid;

int latic = 0; /* flag for latitude based colour */

int Width = 800, Height = 600; /* default map size */

unsigned short **col;  /* colour array */
int **heights;         /* heightfield array */
double **xxx, **yyy, **zzz; /* x,y,z arrays  (used for gridlines */
int cl0[60][30]; /* search map */

int do_outline = 0;  /* if 1, draw coastal outline */
int do_bw = 0;       /* if 1, reduce map to black outline on white */
int contourstep = 0; /* if >0, # of colour steps between contour lines */
int *outx, *outy;

int doshade = 0;
int shade;
unsigned short **shades; /* shade array */
double shade_angle = 150.0; /* angle of "light" on bumpmap */
double shade_angle2 = 20.0; /* with daylight shading, these two are
			       longitude/latitude */

double cla, sla, clo, slo;

double rseed, increment = 0.0000001;

int best = 500000;
int weight[30];

int min(x,y)
int x,y;
{ return(x<y ? x : y); }

int max(x,y)
int x,y;
{ return(x<y ? y : x); }

double fmin(x,y)
double x,y;
{ return(x<y ? x : y); }

double fmax(x,y)
double x,y;
{ return(x<y ? y : x); }

int main(ac,av)
int ac;
char **av;
{
  void printppm(), printppmBW(), printbmp(), printbmpBW(),
       printxpm(), printxpmBW(), printheights(), print_error();
  void mercator(), peter(), squarep(), mollweide(), sinusoid(), stereo(),
    orthographic(), gnomonic(), icosahedral(), azimuth(), conical(),
    search();
  int i;
  double rand2(), log_2(), planet1();
  void readcolors();
  void readmap(), makeoutline(), smoothshades();
  FILE *outfile, *colfile = NULL;
  char filename[256] = "planet-map";
  char colorsname[256] = "Olsson.col";
  int do_file = 0;


#ifdef macintosh
  _ftype = 'TEXT';
  _fcreator ='ttxt';

  ac = ccommand (&av);
  debug = 1;
  do_file = 1;
#endif

  longi = 0.0;
  lat = 0.0;
  scale = 1.0;
  rseed = 0.123;
  view = 'm';
  vgrid = hgrid = 0.0;
  outfile = stdout;

#ifdef WIN32
  setmode(fileno(outfile), O_BINARY);
#elif _WIN32
  setmode(fileno(outfile), O_BINARY);
#endif

  for (i = 1; i<ac; i++) {
    if (av[i][0] == '-') {
      switch (av[i][1]) {
	case 'X' : debug = 1;
		   break;
	case 'V' : sscanf(av[++i],"%lf",&dd2);
		   break;
	case 'v' : sscanf(av[++i],"%lf",&dd1);
		   break;
	case 's' : sscanf(av[++i],"%lf",&rseed);
		   break;
	case 'w' : sscanf(av[++i],"%d",&Width);
		   break;
	case 'h' : sscanf(av[++i],"%d",&Height);
		   break;
	case 'm' : sscanf(av[++i],"%lf",&scale);
		   break;
	case 'o' : sscanf(av[++i],"%s",filename);
		   do_file = 1;
		   break;
	case 'x' : file_type =xpm;
		   break;
	case 'C' : sscanf(av[++i],"%s",colorsname);
		   break;
	case 'l' : sscanf(av[++i],"%lf",&longi);
		   break;
	case 'L' : sscanf(av[++i],"%lf",&lat);
		   break;
	case 'g' : sscanf(av[++i],"%lf",&vgrid);
		   break;
	case 'G' : sscanf(av[++i],"%lf",&hgrid);
		   break;
	case 'c' : latic += 1;
		   break;
      case 'S' : dd1 /= 2.0; POWA = 0.75;
		   break;
	case 'n' : nonLinear = 1;
		   break;
	case 'O' : do_outline = 1;
		   do_bw = 1;
		   if (strlen(av[i])>2)
		     sscanf(av[i],"-O%d",&contourstep);
		   break;
	case 'E' : do_outline = 1;
		   if (strlen(av[i])>2)
		     sscanf(av[i],"-E%d",&contourstep);
		   break;
	case 'B' : doshade = 1;
		   break;
	case 'b' : doshade = 2;
		   break;
	case 'd' : doshade = 3;
		   break;
	case 'P' : file_type = ppm;
		   break;
	case 'H' : file_type = heightfield;
		   break;
	case 'a' : sscanf(av[++i],"%lf",&shade_angle);
		   break;
	case 'A' : sscanf(av[++i],"%lf",&shade_angle2);
		   break;
	case 'i' : sscanf(av[++i],"%lf",&M);
		   break;
	case 'p' : if (strlen(av[i])>2) view = av[i][2];
	           else view = av[++i][0];
	           switch (view) {
		     case 'm' :
		     case 'p' :
		     case 'q' :
		     case 's' :
		     case 'o' :
		     case 'g' :
		     case 'a' :
		     case 'c' :
		     case 'M' :
		     case 'S' :
		     case 'i' :
		     case 'f' : break;
             case 'h' : file_type = heightfield; break;
		     default: fprintf(stderr,"Unknown projection: %s\n",av[i]);
			      print_error(do_file ? filename : "standard output",
					 !do_file ? "" : file_ext(file_type));
		   }
		   break;
	default: fprintf(stderr,"Unknown option: %s\n",av[i]);
		 print_error(do_file ? filename : "standard output",
			    !do_file ? "" : file_ext(file_type));
      }
    }
    else {
      fprintf(stderr,"Unknown option: %s\n\n",av[i]);
      print_error(do_file ? filename : "standard output",
		 !do_file ? "" : file_ext(file_type));
    }
  }

  readcolors(colfile, colorsname);

  if (do_file &&'\0' != filename[0]) {
    if (strchr (filename, '.') == 0)
      strcpy(&(filename[strlen(filename)]), file_ext(file_type));

#ifdef macintosh
    switch (file_type)
    {
      case bmp:
	_ftype = 'BMPf';
	break;
      case ppm:
	_ftype = 'PPGM';
	break;
      case xpm:
	_ftype = 'TEXT';
	break;
    }

    _fcreator ='GKON';
#endif

    outfile = fopen(filename,"wb");

#ifdef macintosh
    _ftype = 'TEXT';
    _fcreator ='ttxt';
#endif

    if (outfile == NULL) {
      fprintf(stderr,
	      "Could not open output file %s, error code = %d\n",
	      filename, errno);
      exit(0);
    }
  }
  else {
    outfile = stdout;
#ifdef WIN32
    setmode(fileno(outfile), O_BINARY);
#elif _WIN32
    setmode(fileno(outfile), O_BINARY);
#endif
  }

  if (longi>180) longi -= 360;
  longi = longi*DEG2RAD;
  lat = lat*DEG2RAD;

  sla = sin(lat); cla = cos(lat);
  slo = sin(longi); clo = cos(longi);

  if (view == 'f') readmap();

  if (file_type == heightfield) {
    heights = (int**)calloc(Width,sizeof(int*));
    if (heights == 0) {
      fprintf(stderr, "Memory allocation failed.");
      exit(1);
    }
    for (i=0; i<Width; i++) {
      heights[i] = (int*)calloc(Height,sizeof(int));
      if (heights[i] == 0) {
	fprintf(stderr,
		"Memory allocation failed at %d out of %d heights\n",
		i+1,Width);
	exit(1);
      }
    }
  }

  col = (unsigned short**)calloc(Width,sizeof(unsigned short*));
  if (col == 0) {
    fprintf(stderr, "Memory allocation failed.");
    exit(1);
  }
  for (i=0; i<Width; i++) {
    col[i] = (unsigned short*)calloc(Height,sizeof(unsigned short));
    if (col[i] == 0) {
      fprintf(stderr,
	      "Memory allocation failed at %d out of %d cols\n",
	      i+1,Width);
      exit(1);
    }
  }

  if (doshade>0) {
    shades = (unsigned short**)calloc(Width,sizeof(unsigned short*));
    if (shades == 0) {
      fprintf(stderr, "Memory allocation failed.");
      exit(1);
    }
    for (i=0; i<Width; i++) {
      shades[i] = (unsigned short*)calloc(Height,sizeof(unsigned short));
      if (shades[i] == 0) {
	fprintf(stderr,
		"Memory allocation failed at %d out of %d shades\n",
		i,Width);
	exit(1);
      }
    }
  }

  if (vgrid != 0.0) {
    xxx = (double**)calloc(Width,sizeof(double*));
    if (xxx == 0) {
      fprintf(stderr, "Memory allocation failed xxx.");
      exit(1);
    }
    for (i=0; i<Width; i++) {
      xxx[i] = (double*)calloc(Height,sizeof(double));
      if (xxx[i] == 0) {
	fprintf(stderr,
		"Memory allocation failed at %d out of %d xxx\n",
		i+1,Width);
	exit(1);
      }
    }

    zzz = (double**)calloc(Width,sizeof(double*));
    if (zzz == 0) {
      fprintf(stderr, "Memory allocation failed zzz.");
      exit(1);
    }
    for (i=0; i<Width; i++) {
      zzz[i] = (double*)calloc(Height,sizeof(double));
      if (zzz[i] == 0) {
	fprintf(stderr,
		"Memory allocation failed at %d out of %d zzz\n",
		i+1,Width);
	exit(1);
      }
    }
  }

  if (hgrid != 0.0 || vgrid != 0.0) {
    yyy = (double**)calloc(Width,sizeof(double*));
    if (yyy == 0) {
      fprintf(stderr, "Memory allocation failed yyy.");
      exit(1);
    }
    for (i=0; i<Width; i++) {
      yyy[i] = (double*)calloc(Height,sizeof(double));
      if (yyy[i] == 0) {
	fprintf(stderr,
		"Memory allocation failed at %d out of %d yyy\n",
		i+1,Width);
	exit(1);
      }
    }
  }

  if (view == 'c') {
    if (lat == 0) view = 'm';
	/* Conical approaches mercator when lat -> 0 */
    if (abs(lat) >= PI - 0.000001) view = 's';
	/* Conical approaches stereo when lat -> +/- 90 */
  }

  Depth = 3*((int)(log_2(scale*Height)))+6;

  r1 = rseed;

  r1 = rand2(r1,r1);
  r2 = rand2(r1,r1);
  r3 = rand2(r1,r2);
  r4 = rand2(r2,r3);

  if (debug && (view != 'f'))
    fprintf(stderr, "+----+----+----+----+----+\n");

  switch (view) {

    case 'm': /* Mercator projection */
      mercator();
      break;

    case 'p': /* Peters projection (area preserving cylindrical) */
      peter();
      break;

    case 'q': /* Square projection (equidistant latitudes) */
      squarep();
      break;

    case 'M': /* Mollweide projection (area preserving) */
      mollweide();
      break;

    case 'S': /* Sinusoid projection (area preserving) */
      sinusoid();
      break;

    case 's': /* Stereographic projection */
      stereo();
      break;

    case 'o': /* Orthographic projection */
      orthographic();
      break;

    case 'g': /* Gnomonic projection */
      gnomonic();
      break;

    case 'i': /* Icosahedral projection */
      icosahedral();
      break;

    case 'a': /* Area preserving azimuthal projection */
      azimuth();
      break;

    case 'c': /* Conical projection (conformal) */
      conical();
      break;

    case 'h': /* heightfield (obsolete) */
      orthographic();
      break;

    case 'f': /* Search */
      while (1) {
	search();
	rseed += increment;
	r1 = rseed;
	r1 = rand2(r1,r1);
	r2 = rand2(r1,r1);
	r3 = rand2(r1,r2);
	r4 = rand2(r2,r3);
      }
  }

  if (do_outline) makeoutline(do_bw);

  if (vgrid != 0.0) { /* draw longitudes */
    int i,j;
    for (i=0; i<Width-1; i++)
      for (j=0; j<Height-1; j++) {
	double t;
	int g = 0;
	if (fabs(yyy[i][j])==1) g=1;
	else {
	  t = floor((atan2(xxx[i][j],zzz[i][j])*180/PI+360)/vgrid);
	  if (t != floor((atan2(xxx[i+1][j],zzz[i+1][j])*180/PI+360)/vgrid))
	    g=1;
	  if (t != floor((atan2(xxx[i][j+1],zzz[i][j+1])*180/PI+360)/vgrid))
	    g=1;
	}
	if (g) {
	  col[i][j] = GRID;
	  if (doshade>0) shades[i][j] = 255;
	}
      }
  }

  if (hgrid != 0.0) { /* draw latitudes */
    int i,j;
    for (i=0; i<Width-1; i++)
      for (j=0; j<Height-1; j++) {
	double t;
	int g = 0;
	t = floor((asin(yyy[i][j])*180/PI+360)/hgrid);
	if (t != floor((asin(yyy[i+1][j])*180/PI+360)/hgrid))
	  g=1;
	if (t != floor((asin(yyy[i][j+1])*180/PI+360)/hgrid))
	  g=1;
	if (g) {
	  col[i][j] = GRID;
	  if (doshade>0) shades[i][j] = 255;
	}
      }
  }

  if (doshade>0) smoothshades();

  if (debug)
    fprintf(stderr, "\n");

  /* plot picture */
  switch (file_type)
  {
    case ppm:
      if (do_bw) printppmBW(outfile);
      else printppm(outfile);
      break;
    case xpm:
      if (do_bw) printxpmBW(outfile);
      else printxpm(outfile);
      break;
    case bmp:
      if (do_bw) printbmpBW(outfile);
      else printbmp(outfile);
      break;
    case heightfield:
      printheights(outfile);
      break;
  }

  return(0);
}

void readcolors(FILE *colfile, char* colorsname)
{
  int crow, cNum = 0, oldcNum, i;

  if (NULL == (colfile = fopen(colorsname, "r")))
    {
      fprintf(stderr,
	      "Cannot open %s\n",
	      colorsname);
      exit(1);
    }


  /* Format of colour file is a sequence of lines       */
  /* each consisting of four integers:                  */
  /*   colour_number red green blue                     */
  /* where 0 <= colour_number <= 65535                  */
  /* and 0<= red, green, blue <= 255                    */
  /* The colour numbers must be increasing              */
  /* The first colours have special uses:               */
  /* 0 is usually black (0,0,0)                         */
  /* 1 is usually white (255,255,255)                   */
  /* 2 is the background colour                         */
  /* 3 is used for latitude/longitude grid lines        */
  /* 4 and 5 are used for outlines and contour lines    */
  /* 6 upwards are used for altitudes                   */
  /* Halfway between 6 and the max colour is sea level  */
  /* Shallowest sea is (max+6)/2 and land is above this */
  /* With 65536 colours, (max+6)/2 = 32770              */
  /* Colours between specified are interpolated         */

  for (crow = 0; !feof(colfile); crow++)
    {
      int	rValue, 
		gValue, 
		bValue,
		result = 0;

      oldcNum = cNum;  /* remember last colour number */
      result = fscanf(colfile, " %d %d %d %d",
		      &cNum, &rValue, &gValue, &bValue);

      if (result > 0)
	{
	  if (cNum < oldcNum) cNum = oldcNum;
	  if (cNum > 65535) cNum = 65535;
	  rtable[cNum] = rValue;
	  gtable[cNum] = gValue;
	  btable[cNum] = bValue;
	  /* interpolate colours between oldcNum and cNum */
	  for (i = oldcNum+1; i<cNum; i++) {
	    rtable[i] = (rtable[oldcNum]*(cNum-i)+rtable[cNum]*(i-oldcNum))
	                / (cNum-oldcNum+1);
	    gtable[i] = (gtable[oldcNum]*(cNum-i)+gtable[cNum]*(i-oldcNum))
	                / (cNum-oldcNum+1);
	    btable[i] = (btable[oldcNum]*(cNum-i)+btable[cNum]*(i-oldcNum))
	                / (cNum-oldcNum+1);
	  }
	}
    }

  nocols = cNum+1;
  if (nocols < 10) nocols = 10;

  HIGHEST = nocols - 1;
  SEA = (HIGHEST+LOWEST)/2;
  LAND = SEA+1;

  for (i = cNum+1; i<nocols; i++) {
    /* fill up rest of colour table with last read colour */
    rtable[i] = rtable[cNum];
    gtable[i] = gtable[cNum];
    btable[i] = btable[cNum];
  }
}

void makeoutline(int do_bw)
{
  int i,j,k,t;

  outx = (int*)calloc(Width*Height,sizeof(int));
  outy = (int*)calloc(Width*Height,sizeof(int));
  k=0;
  for (i=1; i<Width-1; i++)
    for (j=1; j<Height-1; j++)
      if ((col[i][j] >= LOWEST && col[i][j] <= SEA) &&
	  (col[i-1][j] >= LAND || col[i+1][j] >= LAND ||
	   col[i][j-1] >= LAND || col[i][j+1] >= LAND ||
	   col[i-1][j-1] >= LAND || col[i-1][j+1] >= LAND ||
	   col[i+1][j-1] >= LAND || col[i+1][j+1] >= LAND)) {
	/* if point is sea and any neighbour is not, add to outline */
	outx[k] = i; outy[k++] = j;
      }

  if (contourstep>0) {

  for (i=1; i<Width-1; i++)
    for (j=1; j<Height-1; j++) {
      t = (col[i][j] - LAND) / contourstep;
      if (t>=0 &&
          ((col[i-1][j]-LAND) / contourstep > t ||
	   (col[i+1][j]-LAND) / contourstep > t ||
	   (col[i][j-1]-LAND) / contourstep > t ||
	   (col[i][j+1]-LAND) / contourstep > t)) {
	/* if point is at countour line and any neighbour is higher */
	outx[k] = i; outy[k++] = j;
      }
    }
  }
  if (do_bw) /* if outline only, clear colours */
    for (i=0; i<Width; i++)
      for (j=0; j<Height; j++) {
	if (col[i][j] >= LOWEST)
	  col[i][j] = WHITE;
	else col[i][j] = BLACK;
      }
  /* draw outline (in black if outline only) */
  while (k-->0) {
    if (do_bw) t = BLACK;
    else if (contourstep == 0 || col[outx[k]][outy[k]]<LAND ||
             ((col[outx[k]][outy[k]]-LAND)/contourstep)%2 == 1)
      t = OUTLINE1;
    else t = OUTLINE2;
    col[outx[k]][outy[k]] = t;
  }
}

void readmap()
{
  int i,j;
  double y;
  char c;

  Width = 47; Height = 21;
  for (j = 0; j < Height; j++) {
    y = 0.5*7.5*(2.0*j-Height+1);
    y = cos(DEG2RAD*y);
    weight[j] = (int)(100.0*y+0.5);
  }
  for (j = 0; j < Height; j+=2) {
    for(i = 0; i < Width ; i+=2) {
      c = getchar();
      switch (c) {
      case '.': cl0[i][j] = -8;
		break;
      case ',': cl0[i][j] = -4;
		break;
      case ':': cl0[i][j] = -2;
		break;
      case ';': cl0[i][j] = -1;
		break;
      case '-': cl0[i][j] = 0;
		break;
      case '*': cl0[i][j] = 1;
		break;
      case 'o': cl0[i][j] = 2;
		break;
      case 'O': cl0[i][j] = 4;
		break;
      case '@': cl0[i][j] = 16;
		break;
      default: printf("Wrong map symbol: %c\n",c);
      }
      if (i>0) cl0[i-1][j] = (cl0[i][j]+cl0[i-2][j])/2;
    }
    c = getchar(); if (c!='\n') printf("Wrong map format: %c\n",c);
  }
  for (j = 1; j < Height; j+=2)
    for(i = 0; i < Width ; i++)
      cl0[i][j] = (cl0[i][j-1]+cl0[i][j+1])/2;
}


void smoothshades()
{
  int i,j;

  for (i=0; i<Width-2; i++)
    for (j=0; j<Height-2; j++)
      shades[i][j] = (4*shades[i][j]+2*shades[i][j+1]
		      +2*shades[i+1][j]+shades[i+1][j+1]+4)/9;
}

void mercator()
{
  double y,scale1,cos2,theta1, log_2();
  int i,j,k;
  void planet0();

  y = sin(lat);
  y = (1.0+y)/(1.0-y);
  y = 0.5*log(y);
  k = (int)(0.5*y*Width*scale/PI);
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    y = PI*(2.0*(j-k)-Height)/Width/scale;
    y = exp(2.*y);
    y = (y-1.)/(y+1.);
    scale1 = scale*Width/Height/sqrt(1.0-y*y)/PI;
    cos2 = sqrt(1.0-y*y);
    Depth = 3*((int)(log_2(scale1*Height)))+3;
    for (i = 0; i < Width ; i++) {
      theta1 = longi-0.5*PI+PI*(2.0*i-Width)/Width/scale;
      planet0(cos(theta1)*cos2,y,-sin(theta1)*cos2, i,j);
    }
  }
}

void peter()
{
  double y,cos2,theta1,scale1, log_2();
  int k,i,j,water,land;
  void planet0();

  y = 2.0*sin(lat);
  k = (int)(0.5*y*Width*scale/PI);
  water = land = 0;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    y = 0.5*PI*(2.0*(j-k)-Height)/Width/scale;
    if (fabs(y)>1.0)
      for (i = 0; i < Width ; i++) {
	col[i][j] = BACK;
	if (doshade>0) shades[i][j] = 255;
      }
    else {
      cos2 = sqrt(1.0-y*y);
      if (cos2>0.0) {
	scale1 = scale*Width/Height/cos2/PI;
	Depth = 3*((int)(log_2(scale1*Height)))+3;
	for (i = 0; i < Width ; i++) {
	  theta1 = longi-0.5*PI+PI*(2.0*i-Width)/Width/scale;
	  planet0(cos(theta1)*cos2,y,-sin(theta1)*cos2, i,j);
	  if (col[i][j] < LAND) water++; else land++;
	}
      }
    }
  }
  if (debug)
    fprintf(stderr,"\n");
  fprintf(stderr,"water percentage: %d\n",100*water/(water+land));
}

void squarep()
{
  double y,scale1,theta1,cos2, log_2();
  int k,i,j;
  void planet0();

  k = (int)(0.5*lat*Width*scale/PI);
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    y = (2.0*(j-k)-Height)/Width/scale*PI;
    if (fabs(y)>=0.5*PI) for (i = 0; i < Width ; i++) {
      col[i][j] = BACK;
      if (doshade>0) shades[i][j] = 255;
    } else {
      cos2 = cos(y);
      if (cos2>0.0) {
        scale1 = scale*Width/Height/cos2/PI;
        Depth = 3*((int)(log_2(scale1*Height)))+3;
        for (i = 0; i < Width ; i++) {
          theta1 = longi-0.5*PI+PI*(2.0*i-Width)/Width/scale;
          planet0(cos(theta1)*cos2,sin(y),-sin(theta1)*cos2, i,j);
        }
      }
    }
  }
}

void mollweide()
{
  double y,y1,zz,scale1,cos2,theta1,theta2, log_2();
  int i,j,i1=1,k;
  void planet0();

  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    y1 = 2*(2.0*j-Height)/Width/scale;
    if (fabs(y1)>=1.0) for (i = 0; i < Width ; i++) {
      col[i][j] = BACK;
      if (doshade>0) shades[i][j] = 255;
    } else {
      zz = sqrt(1.0-y1*y1);
      y = 2.0/PI*(y1*zz+asin(y1));
      cos2 = sqrt(1.0-y*y);
      if (cos2>0.0) {
        scale1 = scale*Width/Height/cos2/PI;
        Depth = 3*((int)(log_2(scale1*Height)))+3;
        for (i = 0; i < Width ; i++) {
          theta1 = PI/zz*(2.0*i-Width)/Width/scale;
          if (fabs(theta1)>PI) {
            col[i][j] = BACK;
            if (doshade>0) shades[i][j] = 255;
          } else {
            double x2,y2,z2, x3,y3,z3;
            theta1 += -0.5*PI;
            x2 = cos(theta1)*cos2;
            y2 = y;
            z2 = -sin(theta1)*cos2;
            x3 = clo*x2+slo*sla*y2+slo*cla*z2;
            y3 = cla*y2-sla*z2;
            z3 = -slo*x2+clo*sla*y2+clo*cla*z2;

            planet0(x3,y3,z3, i,j);
          }
        }
      }
    }
  }
}

void sinusoid()
{
  double y,theta1,theta2,cos2,l1,i1,scale1, log_2();
  int k,i,j,l,c;
  void planet0();

  k = (int)(lat*Width*scale/PI);
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    y = (2.0*(j-k)-Height)/Width/scale*PI;
    if (fabs(y)>=0.5*PI) for (i = 0; i < Width ; i++) {
      col[i][j] = BACK;
      if (doshade>0) shades[i][j] = 255;
    } else {
      cos2 = cos(y);
      if (cos2>0.0) {
        scale1 = scale*Width/Height/cos2/PI;
        Depth = 3*((int)(log_2(scale1*Height)))+3;
        for (i = 0; i<Width; i++) {
          l = i*12/Width;
          l1 = l*Width/12.0;
          i1 = i-l1;
          theta2 = longi-0.5*PI+PI*(2.0*l1-Width)/Width/scale;
          theta1 = (PI*(2.0*i1-Width/12)/Width/scale)/cos2;
          if (fabs(theta1)>PI/12.0) {
            col[i][j] = BACK;
            if (doshade>0) shades[i][j] = 255;
          } else {
            planet0(cos(theta1+theta2)*cos2,sin(y),-sin(theta1+theta2)*cos2,
                    i,j);
          }
        }
      }
    }
  }
}

void stereo()
{
  double x,y,ymin,ymax,z,zz,x1,y1,z1,theta1,theta2;
  int i,j;
  void planet0();

  ymin = 2.0;
  ymax = -2.0;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    for (i = 0; i < Width ; i++) {
      x = (2.0*i-Width)/Height/scale;
      y = (2.0*j-Height)/Height/scale;
      z = x*x+y*y;
      zz = 0.25*(4.0+z);
      x = x/zz;
      y = y/zz;
      z = (1.0-0.25*z)/zz;
      x1 = clo*x+slo*sla*y+slo*cla*z;
      y1 = cla*y-sla*z;
      z1 = -slo*x+clo*sla*y+clo*cla*z;
      if (y1 < ymin) ymin = y1;
      if (y1 > ymax) ymax = y1;

      /* for level-of-detail effect:
         Depth = 3*((int)(log_2(scale*Height)/(1.0+x1*x1+y1*y1)))+6; */

      planet0(x1,y1,z1, i,j);
    }
  }
}

void orthographic()
{
  double x,y,z,x1,y1,z1,ymin,ymax,theta1,theta2,zz;
  int i,j;
  void planet0();

  ymin = 2.0;
  ymax = -2.0;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    for (i = 0; i < Width ; i++) {
      x = (2.0*i-Width)/Height/scale;
      y = (2.0*j-Height)/Height/scale;
      if (x*x+y*y>1.0) {
        col[i][j] = BACK;
        if (doshade>0) shades[i][j] = 255;
      } else {
        z = sqrt(1.0-x*x-y*y);
        x1 = clo*x+slo*sla*y+slo*cla*z;
        y1 = cla*y-sla*z;
        z1 = -slo*x+clo*sla*y+clo*cla*z;
        if (y1 < ymin) ymin = y1;
        if (y1 > ymax) ymax = y1;
        planet0(x1,y1,z1, i,j);
      }
    }
  }
}

void icosahedral() /* modified version of gnomonic */
{
  double x,y,z,x1,y1,z1,zz,theta1,theta2,ymin,ymax;
  int i,j;
  void planet0();
  double lat1, longi1, sla, cla, slo, clo, x0, y0, sq3_4, sq3;
  double L1, L2, S;

  ymin = 2.0;
  ymax = -2.0;
  sq3 = sqrt(3.0);
  L1 = 10.812317;
  L2 = -52.622632;
  S = 55.6;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    for (i = 0; i < Width ; i++) {

      x0 = 198.0*(2.0*i-Width)/Width/scale-36;
      y0 = 198.0*(2.0*j-Height)/Width/scale - lat/DEG2RAD;

      longi1 = 0.0;
      lat1 = 500.0;
      if (y0/sq3 <= 18.0 && y0/sq3 >= -18.0) { /* middle row of triangles */
	/* upward triangles */
	if (x0-y0/sq3 < 144.0 && x0+y0/sq3 >= 108.0) {
	  lat1 = -L1;
	  longi1 = 126.0;
	}
	else if (x0-y0/sq3 < 72.0 && x0+y0/sq3 >= 36.0) {
	  lat1 = -L1;
	  longi1 = 54.0;
	}
	else if (x0-y0/sq3 < 0.0 && x0+y0/sq3 >= -36.0) {
	  lat1 = -L1;
	  longi1 = -18.0;
	}
	else if (x0-y0/sq3 < -72.0 && x0+y0/sq3 >= -108.0) {
	  lat1 = -L1;
	  longi1 = -90.0;
	}
	else if (x0-y0/sq3 < -144.0 && x0+y0/sq3 >= -180.0) {
	  lat1 = -L1;
	  longi1 = -162.0;
	}

	/* downward triangles */
	else if (x0+y0/sq3 < 108.0 && x0-y0/sq3 >= 72.0) {
	  lat1 = L1;
	  longi1 = 90.0;
	}
	else if (x0+y0/sq3 < 36.0 && x0-y0/sq3 >= 0.0) {
	  lat1 = L1;
	  longi1 = 18.0;
	}
	else if (x0+y0/sq3 < -36.0 && x0-y0/sq3 >= -72.0) {
	  lat1 = L1;
	  longi1 = -54.0;
	}
	else if (x0+y0/sq3 < -108.0 && x0-y0/sq3 >= -144.0) {
	  lat1 = L1;
	  longi1 = -126.0;
	}
	else if (x0+y0/sq3 < -180.0 && x0-y0/sq3 >= -216.0) {
	  lat1 = L1;
	  longi1 = -198.0;
	}
      }

      if (y0/sq3 > 18.0) { /* bottom row of triangles */
	if (x0+y0/sq3 < 180.0 && x0-y0/sq3 >= 72.0) {
	  lat1 = L2;
	  longi1 = 126.0;
	}
	else if (x0+y0/sq3 < 108.0 && x0-y0/sq3 >= 0.0) {
	  lat1 = L2;
	  longi1 = 54.0;
	}
	else if (x0+y0/sq3 < 36.0 && x0-y0/sq3 >= -72.0) {
	  lat1 = L2;
	  longi1 = -18.0;
	}
	else if (x0+y0/sq3 < -36.0 && x0-y0/sq3 >= -144.0) {
	  lat1 = L2;
	  longi1 = -90.0;
	}
	else if (x0+y0/sq3 < -108.0 && x0-y0/sq3 >= -216.0) {
	  lat1 = L2;
	  longi1 = -162.0;
	}
      }
      if (y0/sq3 < -18.0) { /* top row of triangles */
	if (x0-y0/sq3 < 144.0 && x0+y0/sq3 >= 36.0) {
	  lat1 = -L2;
	  longi1 = 90.0;
	}
	else if (x0-y0/sq3 < 72.0 && x0+y0/sq3 >= -36.0) {
	  lat1 = -L2;
	  longi1 = 18.0;
	}
	else if (x0-y0/sq3 < 0.0 && x0+y0/sq3 >= -108.0) {
	  lat1 = -L2;
	  longi1 = -54.0;
	}
	else if (x0-y0/sq3 < -72.0 && x0+y0/sq3 >= -180.0) {
	  lat1 = -L2;
	  longi1 = -126.0;
	}
	else if (x0-y0/sq3 < -144.0 && x0+y0/sq3 >= -252.0) {
	  lat1 = -L2;
	  longi1 = -198.0;
	}
      }

      if (lat1 > 400.0) {
        col[i][j] = BACK;
        if (doshade>0) shades[i][j] = 255;
      } else {
        x = (x0 - longi1)/S;
        y = (y0 + lat1)/S;

        longi1 = longi1*DEG2RAD - longi;
        lat1 = lat1*DEG2RAD;

        sla = sin(lat1); cla = cos(lat1);
        slo = sin(longi1); clo = cos(longi1);

        zz = sqrt(1.0/(1.0+x*x+y*y));
        x = x*zz;
        y = y*zz;
        z = sqrt(1.0-x*x-y*y);
        x1 = clo*x+slo*sla*y+slo*cla*z;
        y1 = cla*y-sla*z;
        z1 = -slo*x+clo*sla*y+clo*cla*z;

        /*
          if (y0/sq3 < 18.05 && y0/sq3 > 17.95
          && x0+y0/sq3 > -18.05 && x0+y0/sq3 < -17.95)
          fprintf(stderr, "%lf, %lf: %lf, %lf, %lf\n",x0+y0/sq3, y0/sq3, x1,y1,z1);
        */
        if (y1 < ymin) ymin = y1;
        if (y1 > ymax) ymax = y1;
        planet0(x1,y1,z1, i,j);
      }
    }
  }
}

void gnomonic()
{
  double x,y,z,x1,y1,z1,zz,theta1,theta2,ymin,ymax;
  int i,j;
  void planet0();

  ymin = 2.0;
  ymax = -2.0;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    for (i = 0; i < Width ; i++) {
      x = (2.0*i-Width)/Height/scale;
      y = (2.0*j-Height)/Height/scale;
      zz = sqrt(1.0/(1.0+x*x+y*y));
      x = x*zz;
      y = y*zz;
      z = sqrt(1.0-x*x-y*y);
      x1 = clo*x+slo*sla*y+slo*cla*z;
      y1 = cla*y-sla*z;
      z1 = -slo*x+clo*sla*y+clo*cla*z;
      if (y1 < ymin) ymin = y1;
      if (y1 > ymax) ymax = y1;
      planet0(x1,y1,z1, i,j);
    }
  }
}

void azimuth()
{
  double x,y,z,x1,y1,z1,zz,theta1,theta2,ymin,ymax;
  int i,j;
  void planet0();

  ymin = 2.0;
  ymax = -2.0;
  for (j = 0; j < Height; j++) {
    if (debug && ((j % (Height/25)) == 0))
      {fprintf (stderr, "%c", view); fflush(stderr);}
    for (i = 0; i < Width ; i++) {
      x = (2.0*i-Width)/Height/scale;
      y = (2.0*j-Height)/Height/scale;
      zz = x*x+y*y;
      z = 1.0-0.5*zz;
      if (z<-1.0) {
        col[i][j] = BACK;
        if (doshade>0) shades[i][j] = 255;
      } else {
        zz = sqrt(1.0-0.25*zz);
        x = x*zz;
        y = y*zz;
        x1 = clo*x+slo*sla*y+slo*cla*z;
        y1 = cla*y-sla*z;
        z1 = -slo*x+clo*sla*y+clo*cla*z;
        if (y1 < ymin) ymin = y1;
        if (y1 > ymax) ymax = y1;
        planet0(x1,y1,z1, i,j);
      }
    }
  }
}

void conical()
{
  double k1,c,y2,x,y,zz,x1,y1,z1,theta1,theta2,ymin,ymax,cos2;
  int i,j;
  void planet0();

  ymin = 2.0;
  ymax = -2.0;
  if (lat>0) {
    k1 = 1.0/sin(lat);
    c = k1*k1;
    y2 = sqrt(c*(1.0-sin(lat/k1))/(1.0+sin(lat/k1)));
    for (j = 0; j < Height; j++) {
      if (debug && ((j % (Height/25)) == 0))
        {fprintf (stderr, "%c", view); fflush(stderr);}
      for (i = 0; i < Width ; i++) {
        x = (2.0*i-Width)/Height/scale;
        y = (2.0*j-Height)/Height/scale+y2;
        zz = x*x+y*y;
        if (zz==0.0) theta1 = 0.0; else theta1 = k1*atan2(x,y);
        if (theta1<-PI || theta1>PI) {
          col[i][j] = BACK;
          if (doshade>0) shades[i][j] = 255;
        } else {
          theta1 += longi-0.5*PI; /* theta1 is longitude */
          theta2 = k1*asin((zz-c)/(zz+c));
          /* theta2 is latitude */
          if (theta2 > 0.5*PI || theta2 < -0.5*PI) {
            col[i][j] = BACK;
            if (doshade>0) shades[i][j] = 255;
          } else {
            cos2 = cos(theta2);
            y = sin(theta2);
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
            planet0(cos(theta1)*cos2,y,-sin(theta1)*cos2, i, j);
          }
        }
      }
    }
  }
  else {
    k1 = 1.0/sin(lat);
    c = k1*k1;
    y2 = sqrt(c*(1.0-sin(lat/k1))/(1.0+sin(lat/k1)));
    for (j = 0; j < Height; j++) {
      if (debug && ((j % (Height/25)) == 0))
        {fprintf (stderr, "%c", view); fflush(stderr);}
      for (i = 0; i < Width ; i++) {
        x = (2.0*i-Width)/Height/scale;
        y = (2.0*j-Height)/Height/scale-y2;
        zz = x*x+y*y;
        if (zz==0.0) theta1 = 0.0; else theta1 = -k1*atan2(x,-y);
        if (theta1<-PI || theta1>PI) {
          col[i][j] = BACK;
          if (doshade>0) shades[i][j] = 255;
        } else {
          theta1 += longi-0.5*PI; /* theta1 is longitude */
          theta2 = k1*asin((zz-c)/(zz+c));
          /* theta2 is latitude */
          if (theta2 > 0.5*PI || theta2 < -0.5*PI) {
            col[i][j] = BACK;
            if (doshade>0) shades[i][j] = 255;
          } else {
            cos2 = cos(theta2);
            y = sin(theta2);
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
            planet0(cos(theta1)*cos2,y,-sin(theta1)*cos2, i, j);
          }
        }
      }
    }
  }
}


void search()
{
  double y,cos2,theta1,scale1, planet1(), log_2();
  double y2,cos22,theta12;
  int i,j,k,l,c,c1,c2,c3, errcount, errcount1;

  for (j = 0; j < Height; j++) {
    y = 0.5*7.5*(2.0*j-Height+1);
    y = sin(DEG2RAD*y);
    scale1 = Width/Height/sqrt(1.0-y*y)/PI;
    cos2 = sqrt(1.0-y*y);
    y2 = 0.5*7.5*(2.0*j-Height+1.5);
    y2 = sin(DEG2RAD*y2);
    cos22 = sqrt(1.0-y2*y2);
    Depth = 3*((int)(log_2(scale1*Height)))+6;
    for (i = 0; i < Width ; i++) {
      theta1 = -0.5*PI+PI*(2.0*i-Width)/Width;
      theta12 = -0.5*PI+PI*(2.0*i+0.5-Width)/Width;
      c = 128+1000*planet1(cos(theta1)*cos2,y,-sin(theta1)*cos2);
      c1 = 128+1000*planet1(cos(theta12)*cos2,y,-sin(theta12)*cos2);
      c2 = 128+1000*planet1(cos(theta1)*cos22,y2,-sin(theta1)*cos22);
      c3 = 128+1000*planet1(cos(theta12)*cos22,y2,-sin(theta12)*cos22);
      c = (c+c1+c2+c3)/4.0;
      if (c<0) c = 0;
      if (c>255) c = 255;
      col[i][j] = c;
    }
  }
  for (k=0; k<Width; k++) {
    for (l=-20; l<=20; l+=2) {
      errcount = 0;
      for (j = 0; j < Height; j++) {
	errcount1 = 0;
	for(i = 0; i < Width ; i++) {
	  if (cl0[i][j]<0 && col[(i+k)%Width][j] > 128-l)
	    errcount1-=cl0[i][j];
	  if (cl0[i][j]>0 && col[(i+k)%Width][j] <= 128-l)
	    errcount1+=cl0[i][j];
	}
	errcount += weight[j]*errcount1;
      }

      if (errcount < best) {
        printf("Errors: %d, parameters: -s %.12f -l %.1f -i %.3f\n",
               errcount,rseed,(360.0*k)/(Width+1),M+l/1000.0);
        best = errcount;
        for (j = 0; j < Height; j++) {
          for(i = 0; i < Width ; i++)
            if (col[(i+k)%Width][j] <= 128-l) putchar('.');
            else putchar('O');
          putchar('\n');
        }
        fflush(stdout);
      }
    }
  }
}

void planet0(x,y,z, i, j)
double x,y,z;
int i, j;
{
  double alt, planet1(), y2;
  int colour;

  alt = planet1(x,y,z);
  if (nonLinear) {
    /* non-linear scaling to make flatter near sea level */
    alt = alt*alt*alt * 300;
  }

  /* store height for heightfield */
  if (file_type == heightfield) heights[i][j] = 10000000*alt;

  y2 = y*y; y2 = y2*y2; y2 = y2*y2;

  /* calculate colour */
  if (alt <=0.) { /* if below sea level then */
    if (latic > 0 && y2+alt >= 1.0 - 0.02*latic*latic)
      colour = HIGHEST;	 /* icecap if close to poles */
    else {
      colour = SEA+(int)((SEA-LOWEST+1)*(10*alt));
      if (colour<LOWEST) colour = LOWEST;
    }
  }
  else {
    if (latic) alt += 0.1*latic*y2;  /* altitude adjusted with latitude */
    if (alt >= 0.1) /* if high then */
      colour = HIGHEST;
    else {
      colour = LAND+(int)((HIGHEST-LAND+1)*(10*alt));
      if (colour>HIGHEST) colour = HIGHEST;
    }
  }

  /* store colour */
  col[i][j] = colour;

  /* store (x,y,z) coordinates for grid drawing */
  if (vgrid != 0.0) {
    xxx[i][j] = x;
    zzz[i][j] = z;
  }
  if (hgrid != 0.0 || vgrid != 0.0) yyy[i][j] = y;

  /* store shading info */
  if (doshade>0) shades[i][j] = shade;

  return;
}

double ssa,ssb,ssc,ssd, ssas,ssbs,sscs,ssds,
  ssax,ssay,ssaz, ssbx,ssby,ssbz, sscx,sscy,sscz, ssdx,ssdy,ssdz;

double planet(a,b,c,d, as,bs,cs,ds,
	      ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz,
	      x,y,z, level)
double a,b,c,d;		    /* altitudes of the 4 verticess */
double as,bs,cs,ds;	    /* seeds of the 4 verticess */
double ax,ay,az, bx,by,bz,  /* vertex coordinates */
  cx,cy,cz, dx,dy,dz;
double x,y,z;		    /* goal point */
int level;		    /* levels to go */
{
  double rand2();
  double abx,aby,abz, acx,acy,acz, adx,ady,adz;
  double bcx,bcy,bcz, bdx,bdy,bdz, cdx,cdy,cdz;
  double lab, lac, lad, lbc, lbd, lcd;
  double ex, ey, ez, e, es, es1, es2, es3;
  double eax,eay,eaz, epx,epy,epz;
  double ecx,ecy,ecz, edx,edy,edz;
  double x1,y1,z1,x2,y2,z2,l1,tmp;

  if (level>0) {
    if (level==11) {
      ssa=a; ssb=b; ssc=c; ssd=d; ssas=as; ssbs=bs; sscs=cs; ssds=ds;
      ssax=ax; ssay=ay; ssaz=az; ssbx=bx; ssby=by; ssbz=bz;
      sscx=cx; sscy=cy; sscz=cz; ssdx=dx; ssdy=dy; ssdz=dz;
    }
    abx = ax-bx; aby = ay-by; abz = az-bz;
    acx = ax-cx; acy = ay-cy; acz = az-cz;
    lab = abx*abx+aby*aby+abz*abz;
    lac = acx*acx+acy*acy+acz*acz;

    /* reorder vertices so ab is longest edge */
    if (lab<lac)
      return(planet(a,c,b,d, as,cs,bs,ds,
		    ax,ay,az, cx,cy,cz, bx,by,bz, dx,dy,dz,
		    x,y,z, level));
    else {
      adx = ax-dx; ady = ay-dy; adz = az-dz;
      lad = adx*adx+ady*ady+adz*adz;
      if (lab<lad)
	return(planet(a,d,b,c, as,ds,bs,cs,
		      ax,ay,az, dx,dy,dz, bx,by,bz, cx,cy,cz,
		      x,y,z, level));
      else {
	bcx = bx-cx; bcy = by-cy; bcz = bz-cz;
	lbc = bcx*bcx+bcy*bcy+bcz*bcz;
	if (lab<lbc)
	  return(planet(b,c,a,d, bs,cs,as,ds,
			bx,by,bz, cx,cy,cz, ax,ay,az, dx,dy,dz,
			x,y,z, level));
	else {
	  bdx = bx-dx; bdy = by-dy; bdz = bz-dz;
	  lbd = bdx*bdx+bdy*bdy+bdz*bdz;
	  if (lab<lbd)
	    return(planet(b,d,a,c, bs,ds,as,cs,
			  bx,by,bz, dx,dy,dz, ax,ay,az, cx,cy,cz,
			  x,y,z, level));
	  else {
	    cdx = cx-dx; cdy = cy-dy; cdz = cz-dz;
	    lcd = cdx*cdx+cdy*cdy+cdz*cdz;
	    if (lab<lcd)
	      return(planet(c,d,a,b, cs,ds,as,bs,
			    cx,cy,cz, dx,dy,dz, ax,ay,az, bx,by,bz,
			    x,y,z, level));
	    else { /* ab is longest, so cut ab */
	      es = rand2(as,bs);
	      es1 = rand2(es,es);
	      es2 = 0.5+0.1*rand2(es1,es1);
	      es3 = 1.0-es2;
	      if (ax<bx) {
		ex = es2*ax+es3*bx; ey = es2*ay+es3*by; ez = es2*az+es3*bz;
	      } else if (ax>bx) {
		ex = es3*ax+es2*bx; ey = es3*ay+es2*by; ez = es3*az+es2*bz;
	      } else { /* ax==bx, very unlikely to ever happen */
		ex = 0.5*ax+0.5*bx; ey = 0.5*ay+0.5*by; ez = 0.5*az+0.5*bz;
	      }
	      if (lab>1.0) lab = pow(lab,0.5);
	      /* decrease contribution for very long distances */

              /* new altitude is: */
	      e = 0.5*(a+b) /* average of end points */
		+ es*dd1*pow(fabs(a-b),POWA)
		/* plus contribution for altitude diff */
                + es1*dd2*pow(lab,POW); /* plus contribution for distance */
	      eax = ax-ex; eay = ay-ey; eaz = az-ez;
	      epx =  x-ex; epy =  y-ey; epz =  z-ez;
	      ecx = cx-ex; ecy = cy-ey; ecz = cz-ez;
	      edx = dx-ex; edy = dy-ey; edz = dz-ez;
	      if ((eax*ecy*edz+eay*ecz*edx+eaz*ecx*edy
		   -eaz*ecy*edx-eay*ecx*edz-eax*ecz*edy)*
		  (epx*ecy*edz+epy*ecz*edx+epz*ecx*edy
		   -epz*ecy*edx-epy*ecx*edz-epx*ecz*edy)>0.0)
		return(planet(c,d,a,e, cs,ds,as,es,
			      cx,cy,cz, dx,dy,dz, ax,ay,az, ex,ey,ez,
			      x,y,z, level-1));
	      else
		return(planet(c,d,b,e, cs,ds,bs,es,
			      cx,cy,cz, dx,dy,dz, bx,by,bz, ex,ey,ez,
			      x,y,z, level-1));
	    }
	  }
	}
      }
    }
  }
  else { /* level == 0 */
    if (doshade==1 || doshade==2) {
      x1 = 0.25*(ax+bx+cx+dx);
      x1 = a*(x1-ax)+b*(x1-bx)+c*(x1-cx)+d*(x1-dx);
      y1 = 0.25*(ay+by+cy+dy);
      y1 = a*(y1-ay)+b*(y1-by)+c*(y1-cy)+d*(y1-dy);
      z1 = 0.25*(az+bz+cz+dz);
      z1 = a*(z1-az)+b*(z1-bz)+c*(z1-cz)+d*(z1-dz);
      l1 = sqrt(x1*x1+y1*y1+z1*z1);
      if (l1==0.0) l1 = 1.0;
      tmp = sqrt(1.0-y*y);
      if (tmp<0.0001) tmp = 0.0001;
      x2 = x*x1+y*y1+z*z1;
      y2 = -x*y/tmp*x1+tmp*y1-z*y/tmp*z1;
      z2 = -z/tmp*x1+x/tmp*z1;
      shade =
	(int)((-sin(PI*shade_angle/180.0)*y2-cos(PI*shade_angle/180.0)*z2)
	      /l1*48.0+128.0);
      if (shade<10) shade = 10;
      if (shade>255) shade = 255;
      if (doshade==2 && (a+b+c+d)<0.0) shade = 150;
    }
    else if (doshade==3) {
      if ((a+b+c+d)<0.0) {
	x1 = x; y1 = y; z1 = z;
      } else {
        l1 = 50.0/
             sqrt((ax-bx)*(ax-bx)+(ay-by)*(ay-by)+(az-bz)*(az-bz)+
		  (ax-cx)*(ax-cx)+(ay-cy)*(ay-cy)+(az-cz)*(az-cz)+
		  (ax-dx)*(ax-dx)+(ay-dy)*(ay-dy)+(az-dz)*(az-dz)+
		  (bx-cx)*(bx-cx)+(by-cy)*(by-cy)+(bz-cz)*(bz-cz)+
		  (bx-dx)*(bx-dx)+(by-dy)*(by-dy)+(bz-dz)*(bz-dz)+
		  (cx-dx)*(cx-dx)+(cy-dy)*(cy-dy)+(cz-dz)*(cz-dz));
	x1 = 0.25*(ax+bx+cx+dx);
	x1 = l1*(a*(x1-ax)+b*(x1-bx)+c*(x1-cx)+d*(x1-dx)) + x;
	y1 = 0.25*(ay+by+cy+dy);
	y1 = l1*(a*(y1-ay)+b*(y1-by)+c*(y1-cy)+d*(y1-dy)) + y;
	z1 = 0.25*(az+bz+cz+dz);
	z1 = l1*(a*(z1-az)+b*(z1-bz)+c*(z1-cz)+d*(z1-dz)) + z;
      }
      l1 = sqrt(x1*x1+y1*y1+z1*z1);
      if (l1==0.0) l1 = 1.0;
      x2 = cos(PI*shade_angle/180.0-0.5*PI)*cos(PI*shade_angle2/180.0);
      y2 = -sin(PI*shade_angle2/180.0);
      z2 = -sin(PI*shade_angle/180.0-0.5*PI)*cos(PI*shade_angle2/180.0);
      shade = (int)((x1*x2+y1*y2+z1*z2)/l1*170.0+10);
      if (shade<10) shade = 10;
      if (shade>255) shade = 255;
    }
    return((a+b+c+d)/4);
  }
}

double planet1(x,y,z)
double x,y,z;
{
  double abx,aby,abz, acx,acy,acz, adx,ady,adz, apx,apy,apz;
  double bax,bay,baz, bcx,bcy,bcz, bdx,bdy,bdz, bpx,bpy,bpz;

  abx = ssbx-ssax; aby = ssby-ssay; abz = ssbz-ssaz;
  acx = sscx-ssax; acy = sscy-ssay; acz = sscz-ssaz;
  adx = ssdx-ssax; ady = ssdy-ssay; adz = ssdz-ssaz;
  apx = x-ssax; apy = y-ssay; apz = z-ssaz;
  if ((adx*aby*acz+ady*abz*acx+adz*abx*acy
       -adz*aby*acx-ady*abx*acz-adx*abz*acy)*
      (apx*aby*acz+apy*abz*acx+apz*abx*acy
       -apz*aby*acx-apy*abx*acz-apx*abz*acy)>0.0){
    /* p is on same side of abc as d */
    if ((acx*aby*adz+acy*abz*adx+acz*abx*ady
	 -acz*aby*adx-acy*abx*adz-acx*abz*ady)*
	(apx*aby*adz+apy*abz*adx+apz*abx*ady
	 -apz*aby*adx-apy*abx*adz-apx*abz*ady)>0.0){
      /* p is on same side of abd as c */
      if ((abx*ady*acz+aby*adz*acx+abz*adx*acy
	   -abz*ady*acx-aby*adx*acz-abx*adz*acy)*
	  (apx*ady*acz+apy*adz*acx+apz*adx*acy
	   -apz*ady*acx-apy*adx*acz-apx*adz*acy)>0.0){
	/* p is on same side of acd as b */
	bax = -abx; bay = -aby; baz = -abz;
	bcx = sscx-ssbx; bcy = sscy-ssby; bcz = sscz-ssbz;
	bdx = ssdx-ssbx; bdy = ssdy-ssby; bdz = ssdz-ssbz;
	bpx = x-ssbx; bpy = y-ssby; bpz = z-ssbz;
	if ((bax*bcy*bdz+bay*bcz*bdx+baz*bcx*bdy
	     -baz*bcy*bdx-bay*bcx*bdz-bax*bcz*bdy)*
	    (bpx*bcy*bdz+bpy*bcz*bdx+bpz*bcx*bdy
	     -bpz*bcy*bdx-bpy*bcx*bdz-bpx*bcz*bdy)>0.0){
	  /* p is on same side of bcd as a */
	  /* Hence, p is inside tetrahedron */
	  return(planet(ssa,ssb,ssc,ssd, ssas,ssbs,sscs,ssds,
			ssax,ssay,ssaz, ssbx,ssby,ssbz,
			sscx,sscy,sscz, ssdx,ssdy,ssdz,
			x,y,z, 11));
	}
      }
    }
  } /* otherwise */
  return(planet(M,M,M,M,
		/* initial altitude is M on all corners of tetrahedron */
		r1,r2,r3,r4,
		/* same seed set is used in every call */
		-sqrt(3.0)-0.20, -sqrt(3.0)-0.22, -sqrt(3.0)-0.23,
		-sqrt(3.0)-0.19,  sqrt(3.0)+0.18,  sqrt(3.0)+0.17,
		 sqrt(3.0)+0.21, -sqrt(3.0)-0.24,  sqrt(3.0)+0.15,
		 sqrt(3.0)+0.24,  sqrt(3.0)+0.22, -sqrt(3.0)-0.25,
		/* coordinates of vertices of tetrahedron*/
		x,y,z,
		/* coordinates of point we want colour of */
		Depth));
		/* subdivision depth */

}


double rand2(p,q) /* random number generator taking two seeds */
double p,q;	  /* rand2(p,q) = rand2(q,p) is important     */
{
  double r;
  r = (p+3.14159265)*(q+3.14159265);
  return(2.*(r-(int)r)-1.);
}

void printppm(outfile) /* prints picture in PPM (portable pixel map) format */
FILE *outfile;
{
  int i,j,c,s;

  fprintf(outfile,"P6\n");
  fprintf(outfile,"#fractal planet image\n");
  fprintf(outfile,"%d %d 255\n",Width,Height);

  if (doshade) {
    for (j=0; j<Height; j++) {
      for (i=0; i<Width; i++) {
	s =shades[i][j];
	c = s*rtable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
	c = s*gtable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
	c = s*btable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
      }
    }
  } else {
    for (j=0; j<Height; j++)
      for (i=0; i<Width; i++) {
	putc(rtable[col[i][j]],outfile);
	putc(gtable[col[i][j]],outfile);
	putc(btable[col[i][j]],outfile);
      }
  }
  fclose(outfile);
}

void printppmBW(outfile) /* prints picture in b/w PPM format */
FILE *outfile;
{
  int i,j,c;

  fprintf(outfile,"P6\n");
  fprintf(outfile,"#fractal planet image\n");
  fprintf(outfile,"%d %d 1\n",Width,Height);

  for (j=0; j<Height; j++)
    for (i=0; i<Width; i++) {
      if (col[i][j] < WHITE)
	c=0;
      else c=1;
      putc(c,outfile);
      putc(c,outfile);
      putc(c,outfile);
    }
  fclose(outfile);
}

void printbmp(outfile) /* prints picture in BMP format */
FILE *outfile;
{
  int i,j,c,s, W1;

  fprintf(outfile,"BM");

  W1 = (3*Width+3);
  W1 -= W1 % 4;
  s = 54+W1*Height; /* file size */
  putc(s&255,outfile);
  putc((s>>8)&255,outfile);
  putc((s>>16)&255,outfile);
  putc(s>>24,outfile);

  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(54,outfile); /* offset to data */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(40,outfile); /* size of infoheader */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(Width&255,outfile);
  putc((Width>>8)&255,outfile);
  putc((Width>>16)&255,outfile);
  putc(Width>>24,outfile);

  putc(Height&255,outfile);
  putc((Height>>8)&255,outfile);
  putc((Height>>16)&255,outfile);
  putc(Height>>24,outfile);

  putc(1,outfile);  /* no. of planes = 1 */
  putc(0,outfile);

  putc(24,outfile);  /* bpp */
  putc(0,outfile);

  putc(0,outfile); /* no compression */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* image size (unspecified) */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* h. pixels/m */
  putc(32,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* v. pixels/m */
  putc(32,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* colours used (unspecified) */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);


  putc(0,outfile); /* important colours (all) */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  if (doshade) {
    for (j=Height-1; j>=0; j--) {
      for (i=0; i<Width; i++) {
	s =shades[i][j];
	c = s*btable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
	c = s*gtable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
	c = s*rtable[col[i][j]]/150;
	if (c>255) c=255;
	putc(c,outfile);
      }
      for (i=3*Width; i<W1; i++) putc(0,outfile);
    }
  } else {
    for (j=Height-1; j>=0; j--) {
      for (i=0; i<Width; i++) {
	putc(btable[col[i][j]],outfile);
	putc(gtable[col[i][j]],outfile);
	putc(rtable[col[i][j]],outfile);
      }
      for (i=3*Width; i<W1; i++) putc(0,outfile);
    }
  }
  fclose(outfile);
}

void printbmpBW(outfile) /* prints picture in b/w BMP format */
FILE *outfile;
{
  int i,j,c,s, W1;

  fprintf(outfile,"BM");

  W1 = (Width+31);
  W1 -= W1 % 32;
  s = 62+(W1*Height)/8; /* file size */
  putc(s&255,outfile);
  putc((s>>8)&255,outfile);
  putc((s>>16)&255,outfile);
  putc(s>>24,outfile);

  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(62,outfile); /* offset to data */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(40,outfile); /* size of infoheader */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(Width&255,outfile);
  putc((Width>>8)&255,outfile);
  putc((Width>>16)&255,outfile);
  putc(Width>>24,outfile);

  putc(Height&255,outfile);
  putc((Height>>8)&255,outfile);
  putc((Height>>16)&255,outfile);
  putc(Height>>24,outfile);

  putc(1,outfile);  /* no. of planes = 1 */
  putc(0,outfile);

  putc(1,outfile);  /* bpp */
  putc(0,outfile);

  putc(0,outfile); /* no compression */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* image size (unspecified) */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* h. pixels/m */
  putc(32,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* v. pixels/m */
  putc(32,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(2,outfile); /* colours used */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);


  putc(2,outfile); /* important colours (2) */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(0,outfile); /* colour 0 = black */
  putc(0,outfile);
  putc(0,outfile);
  putc(0,outfile);

  putc(255,outfile); /* colour 1 = white */
  putc(255,outfile);
  putc(255,outfile);
  putc(255,outfile);

  for (j=Height-1; j>=0; j--)
    for (i=0; i<W1; i+=8) {
      if (i<Width && col[i][j] >= WHITE)
	c=128;
      else c=0;
      if (i+1<Width && col[i+1][j] >= WHITE)
	c+=64;
      if (i+2<Width && col[i+2][j] >= WHITE)
	c+=32;
      if (i+3<Width && col[i+3][j] >= WHITE)
	c+=16;
      if (i+4<Width && col[i+4][j] >= WHITE)
	c+=8;
      if (i+5<Width && col[i+5][j] >= WHITE)
	c+=4;
      if (i+6<Width && col[i+6][j] >= WHITE)
	c+=2;
      if (i+7<Width && col[i+7][j] >= WHITE)
	c+=1;
      putc(c,outfile);
    }
  fclose(outfile);
}

char *nletters(int n, int c)
{
  int i;
  static char buffer[8];

  buffer[n] = '\0';

  for (i = n-1; i >= 0; i--)
  {
    buffer[i] = letters[c & 0x001F];
    c >>= 5;
  }

  return buffer;
}

void printxpm(outfile) /* prints picture in XPM (X-windows pixel map) format */
FILE *outfile;
{
  int x,y,i,nbytes;

  x = nocols - 1;
  for (nbytes = 0; x != 0; nbytes++)
    x >>= 5;

  fprintf(outfile,"/* XPM */\n");
  fprintf(outfile,"static char *xpmdata[] = {\n");
  fprintf(outfile,"/* width height ncolors chars_per_pixel */\n");
  fprintf(outfile,"\"%d %d %d %d\",\n", Width, Height, nocols, nbytes);
  fprintf(outfile,"/* colors */\n");
  for (i = 0; i < nocols; i++)
    fprintf(outfile,"\"%s c #%2.2X%2.2X%2.2X\",\n", 
	    nletters(nbytes, i), rtable[i], gtable[i], btable[i]);

  fprintf(outfile,"/* pixels */\n");
  for (y = 0 ; y < Height; y++) {
    fprintf(outfile,"\"");
    for (x = 0; x < Width; x++)
      fprintf(outfile, "%s", nletters(nbytes, col[x][y]));
    fprintf(outfile,"\",\n");
  }
  fprintf(outfile,"};\n");

  fclose(outfile);
}

void printxpmBW(outfile) /* prints picture in XPM (X-windows pixel map) format */
FILE *outfile;
{
  int x,y,nbytes;

  x = nocols - 1;
  nbytes = 1;

  fprintf(outfile,"/* XPM */\n");
  fprintf(outfile,"static char *xpmdata[] = {\n");
  fprintf(outfile,"/* width height ncolors chars_per_pixel */\n");
  fprintf(outfile,"\"%d %d %d %d\",\n", Width, Height, 2, nbytes);
  fprintf(outfile,"/* colors */\n");

  fprintf(outfile,"\". c #FFFFFF\",\n");
  fprintf(outfile,"\"X c #000000\",\n");

  fprintf(outfile,"/* pixels */\n");
  for (y = 0 ; y < Height; y++) {
    fprintf(outfile,"\"");
    for (x = 0; x < Width; x++)
      fprintf(outfile, "%s",
	      (col[x][y] < WHITE)
	      ? "X" : ".");
    fprintf(outfile,"\",\n");
  }
  fprintf(outfile,"};\n");

  fclose(outfile);
}


void printheights(outfile) /* prints heightfield */
FILE *outfile;
{
  int i,j;

  for (j=0; j<Height; j++) {
    for (i=0; i<Width; i++)
      fprintf(outfile,"%d ",heights[i][j]);
    putc('\n',outfile);
  }
  fclose(outfile);
}

double log_2(x)
double x;
{ return(log(x)/log(2.0)); }

void print_error(char *filename, char *ext)
{
  fprintf(stderr,"Usage: planet [options]\n\n");
  fprintf(stderr,"options:\n");
  fprintf(stderr,"  -?                (or any illegal option) Output this text\n");
  fprintf(stderr,"  -s seed           Specifies seed as number between 0.0 and 1.0\n");
  fprintf(stderr,"  -w width          Specifies width in pixels, default = 800\n");
  fprintf(stderr,"  -h height         Specifies height in pixels, default = 600\n");
  fprintf(stderr,"  -m magnification  Specifies magnification, default = 1.0\n");
  fprintf(stderr,"  -o output_file    Specifies output file, default is %s%s\n",
                                            filename, ext);
  fprintf(stderr,"  -l longitude      Specifies longitude of centre in degrees, default = 0.0\n");
  fprintf(stderr,"  -L latitude       Specifies latitude of centre in degrees, default = 0.0\n");
  fprintf(stderr,"  -g gridsize       Specifies vertical gridsize in degrees, default = 0.0 (no grid)\n");
  fprintf(stderr,"  -G gridsize       Specifies horisontal gridsize in degrees, default = 0.0 (no grid)\n");
  fprintf(stderr,"  -i init_alt       Specifies initial altitude (default = -0.02)\n");
  fprintf(stderr,"  -c                Colour depends on latitude (cumulative, default: only altitude)\n");
  fprintf(stderr,"  -C file           Read colour definitions from file\n");
  fprintf(stderr,"  -O                Produce a black and white outline map\n");
  fprintf(stderr,"  -E                Trace the edges of land in black on colour map\n");
  fprintf(stderr,"  -B                Use ``bumpmap'' shading\n");
  fprintf(stderr,"  -b                Use ``bumpmap'' shading on land only\n");
  fprintf(stderr,"  -d                Use ``daylight'' shading\n");
  fprintf(stderr,"  -a angle	      Angle of ``light'' in bumpmap shading\n");
  fprintf(stderr,"                    or longitude of sun in daylight shading\n");
  fprintf(stderr,"  -A latitude	      Latitude of sun in daylight shading\n");
  fprintf(stderr,"  -P                Use PPM file format (default is BMP)\n");
  fprintf(stderr,"  -x                Use XPM file format (default is BMP)\n");
  fprintf(stderr,"  -H                Write heightfield (default is BMP)\n");
  fprintf(stderr,"  -V number         Distance contribution to variation (default = 0.03)\n");
  fprintf(stderr,"  -v number         Altitude contribution to variation (default = 0.4)\n");
  fprintf(stderr,"  -pprojection      Specifies projection: m = Mercator (default)\n");
  fprintf(stderr,"                                          p = Peters\n");
  fprintf(stderr,"                                          q = Square\n");
  fprintf(stderr,"                                          s = Stereographic\n");
  fprintf(stderr,"                                          o = Orthographic\n");
  fprintf(stderr,"                                          g = Gnomonic\n");
  fprintf(stderr,"                                          a = Area preserving azimuthal\n");
  fprintf(stderr,"                                          c = Conical (conformal)\n");
  fprintf(stderr,"                                          M = Mollweide\n");
  fprintf(stderr,"                                          S = Sinusoidal\n");
  fprintf(stderr,"                                          i = Icosaheral\n");
  fprintf(stderr,"                                          h = Heightfield (obsolete)\n");
  fprintf(stderr,"                                          f = Find match, see manual\n");
  exit(0);
}

/* With the -pf option a map must be given on standard input.  */
/* This map is 11 lines of 24 characters. The characters are:  */
/*    . : very strong preference for water (value=8)	       */
/*    , : strong preference for water (value=4)		       */
/*    : : preference for water (value=2)		       */
/*    ; : weak preference for water (value=1)		       */
/*    - : don't care (value=0)				       */
/*    * : weak preference for land (value=1)		       */
/*    o : preference for land (value=2)			       */
/*    O : strong preference for land (value=4)		       */
/*    @ : very strong preference for land (value=8)	       */
/*							       */
/* Each point on the map corresponds to a point on a 15° grid. */
/*							       */
/* The program tries seeds starting from the specified and     */
/* successively outputs the seed (and rotation) of the best    */
/* current match, together with a small map of this.	       */
/* This is all ascii, no bitmap is produced.		       a*/

